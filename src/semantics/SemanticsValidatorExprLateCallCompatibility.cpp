#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprLateCallCompatibility(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprLateCallCompatibilityContext &context,
    bool &handledOut) {
  handledOut = false;
  auto publishLateCallCompatibilityDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };

  if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos) {
    std::string builtinName;
    if (getBuiltinClampName(expr, builtinName, true) ||
        getBuiltinMinMaxName(expr, builtinName, true) ||
        getBuiltinAbsSignName(expr, builtinName, true) ||
        getBuiltinSaturateName(expr, builtinName, true) ||
        getBuiltinMathName(expr, builtinName, true)) {
      error_ = "math builtin requires import /std/math/* or /std/math/<name>: " +
               expr.name;
      return publishLateCallCompatibilityDiagnostic();
    }
  }

  if (!expr.isMethodCall && expr.name.find('/') == std::string::npos) {
    const BindingInfo *callableBinding = findParamBinding(params, expr.name);
    if (callableBinding == nullptr) {
      auto localIt = locals.find(expr.name);
      if (localIt != locals.end()) {
        callableBinding = &localIt->second;
      }
    }
    if (callableBinding != nullptr && callableBinding->typeName == "lambda") {
      handledOut = true;
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for lambda calls";
        return publishLateCallCompatibilityDiagnostic();
      }
      if (!expr.templateArgs.empty()) {
        error_ = "lambda calls do not accept template arguments";
        return publishLateCallCompatibilityDiagnostic();
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "lambda calls do not accept block arguments";
        return publishLateCallCompatibilityDiagnostic();
      }
      for (const auto &arg : expr.args) {
        if (!validateExpr(params, locals, arg)) {
          return false;
        }
      }
      return true;
    }
  }

  if (context.dispatchResolvers == nullptr) {
    return true;
  }

  if (!expr.isMethodCall &&
      (resolved == "/std/collections/vector/count" ||
       resolved == "/vector/count") &&
      expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
    std::string elemType;
    std::string arrayElemType;
    const bool resolvesVector =
        context.dispatchResolvers->resolveVectorTarget(expr.args.front(),
                                                       elemType);
    std::string experimentalElemType;
    const bool resolvesExperimentalVector =
        context.dispatchResolvers->resolveExperimentalVectorTarget(
            expr.args.front(), experimentalElemType);
    const bool resolvesArray =
        context.dispatchResolvers->resolveArrayTarget(expr.args.front(),
                                                      arrayElemType);
    const bool resolvesString =
        context.dispatchResolvers->resolveStringTarget(expr.args.front());
    std::string mapKeyType;
    std::string mapValueType;
    const bool resolvesMap = context.dispatchResolvers->resolveMapTarget(
        expr.args.front(), mapKeyType, mapValueType);
    if (!resolvesVector && !resolvesExperimentalVector && !resolvesArray &&
        !resolvesString) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (resolvesMap ||
          context.dispatchResolvers->resolveMapTarget(expr.args.front(),
                                                      mapKeyType,
                                                      mapValueType)) {
        error_ = "unknown call target: /std/collections/vector/count";
        return publishLateCallCompatibilityDiagnostic();
      }
      error_ = "count requires vector target";
      return publishLateCallCompatibilityDiagnostic();
    }
    if (!hasDeclaredDefinitionPath("/vector/count") &&
        !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
        !hasImportedDefinitionPath("/std/collections/vector/count") &&
        (resolvesVector || resolvesExperimentalVector)) {
      error_ = "unknown call target: /std/collections/vector/count";
      return publishLateCallCompatibilityDiagnostic();
    }
    if (resolved == "/std/collections/vector/count" &&
        hasImportedDefinitionPath("/std/collections/vector/count") &&
        (resolvesVector || resolvesExperimentalVector)) {
      handledOut = true;
      return validateExpr(params, locals, expr.args.front());
    }
  }

  if (!expr.isMethodCall &&
      (resolved == "/std/collections/vector/capacity" ||
       resolved == "/vector/capacity") &&
      expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
    std::string elemType;
    const bool resolvesVector =
        context.dispatchResolvers->resolveVectorTarget(expr.args.front(),
                                                       elemType);
    std::string experimentalElemType;
    const bool resolvesExperimentalVector =
        context.dispatchResolvers->resolveExperimentalVectorTarget(
            expr.args.front(), experimentalElemType);
    if (!resolvesVector && !resolvesExperimentalVector) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (context.dispatchResolvers->resolveArrayTarget(expr.args.front(),
                                                       elemType)) {
        error_ = "unknown call target: /std/collections/vector/capacity";
        return false;
      }
      std::string mapKeyType;
      std::string mapValueType;
      if (context.dispatchResolvers->resolveMapTarget(expr.args.front(),
                                                      mapKeyType,
                                                      mapValueType)) {
        error_ = "unknown call target: /std/collections/vector/capacity";
        return publishLateCallCompatibilityDiagnostic();
      }
      error_ = "capacity requires vector target";
      return publishLateCallCompatibilityDiagnostic();
    }
    if (!hasDeclaredDefinitionPath("/vector/capacity") &&
        !hasDeclaredDefinitionPath("/std/collections/vector/capacity") &&
        !hasImportedDefinitionPath("/std/collections/vector/capacity") &&
        (resolvesVector || resolvesExperimentalVector)) {
      error_ = "unknown call target: /std/collections/vector/capacity";
      return publishLateCallCompatibilityDiagnostic();
    }
    if (resolved == "/std/collections/vector/capacity" &&
        hasImportedDefinitionPath("/std/collections/vector/capacity")) {
      handledOut = true;
      return validateExpr(params, locals, expr.args.front());
    }
  }

  return true;
}

} // namespace primec::semantics

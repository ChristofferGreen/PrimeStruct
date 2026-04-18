#include "SemanticsValidator.h"
#include "SemanticsVectorCompatibilityHelpers.h"

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
  auto failLateCallCompatibilityDiagnostic =
      [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos) {
    std::string builtinName;
      if (getBuiltinClampName(expr, builtinName, true) ||
          getBuiltinMinMaxName(expr, builtinName, true) ||
          getBuiltinAbsSignName(expr, builtinName, true) ||
          getBuiltinSaturateName(expr, builtinName, true) ||
          getBuiltinMathName(expr, builtinName, true)) {
      return failLateCallCompatibilityDiagnostic(
          "math builtin requires import /std/math/* or /std/math/<name>: " +
          expr.name);
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
        return failLateCallCompatibilityDiagnostic(
            "named arguments not supported for lambda calls");
      }
      if (!expr.templateArgs.empty()) {
        return failLateCallCompatibilityDiagnostic(
            "lambda calls do not accept template arguments");
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        return failLateCallCompatibilityDiagnostic(
            "lambda calls do not accept block arguments");
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
      resolved == "/std/collections/vector/count" &&
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
    const bool resolvesVectorLikeCountTarget =
        resolvesVector || resolvesExperimentalVector;
    const bool resolvesArray =
        context.dispatchResolvers->resolveArrayTarget(expr.args.front(),
                                                      arrayElemType);
    const bool resolvesString =
        context.dispatchResolvers->resolveStringTarget(expr.args.front());
    std::string mapKeyType;
    std::string mapValueType;
    const bool resolvesMap = context.dispatchResolvers->resolveMapTarget(
        expr.args.front(), mapKeyType, mapValueType);
    const bool resolvesNonVectorCountTarget =
        !resolvesVectorLikeCountTarget && !resolvesArray &&
        !resolvesString;
    const StdNamespacedVectorCompatibilityHelperState
        stdNamespacedVectorCountHelperState{
            hasDeclaredDefinitionPath("/std/collections/vector/count"),
            hasImportedDefinitionPath("/std/collections/vector/count")};
    if (resolvesNonVectorCountTarget) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      const bool resolvesMapAfterValidation =
          resolvesMap ||
          context.dispatchResolvers->resolveMapTarget(expr.args.front(),
                                                      mapKeyType,
                                                      mapValueType);
      const std::string stdNamespacedVectorCountTargetDiagnosticMessage =
          stdNamespacedVectorCountHelperState
              .classifyNonVectorCountTargetDiagnosticMessage(
                  false,
                  resolvesMapAfterValidation,
                  resolvesNonVectorCountTarget);
      return failLateCallCompatibilityDiagnostic(
          std::move(stdNamespacedVectorCountTargetDiagnosticMessage));
    }
    if (resolvesVectorLikeCountTarget &&
        !stdNamespacedVectorCountHelperState.hasDeclaredHelper &&
        !stdNamespacedVectorCountHelperState.hasImportedHelper) {
      return failLateCallCompatibilityDiagnostic(
          vectorCompatibilityUnknownCallTargetDiagnostic("count"));
    }
  }

  if (!expr.isMethodCall &&
      resolved == "/std/collections/vector/capacity" &&
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
      std::string mapKeyType;
      std::string mapValueType;
      const bool resolvesNonVectorCollectionLikeTarget =
          context.dispatchResolvers->resolveArrayTarget(expr.args.front(),
                                                       elemType) ||
          context.dispatchResolvers->resolveMapTarget(expr.args.front(),
                                                     mapKeyType,
                                                     mapValueType);
      if (resolvesNonVectorCollectionLikeTarget) {
        return failLateCallCompatibilityDiagnostic(
            vectorCompatibilityUnknownCallTargetDiagnostic("capacity"));
      }
      return failLateCallCompatibilityDiagnostic(
          vectorCompatibilityRequiresVectorTargetDiagnostic("capacity"));
    }
  }

  return true;
}

} // namespace primec::semantics

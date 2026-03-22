#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprCountCapacityMapBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprCountCapacityMapBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  auto it = defMap_.find(resolved);
  if (it != defMap_.end() && !resolvedMethod) {
    return true;
  }

  const auto *dispatchResolverAdapters = context.dispatchResolverAdapters;
  const auto *dispatchResolvers = context.dispatchResolvers;
  if (dispatchResolverAdapters == nullptr || dispatchResolvers == nullptr ||
      context.resolveMapTarget == nullptr ||
      context.resolveVectorTarget == nullptr) {
    return true;
  }

  std::string logicalResolvedMethod = resolved;
  if (resolvedMethod) {
    std::string canonicalExperimentalMapHelperResolved;
    if (canonicalizeExperimentalMapHelperResolvedPath(
            resolved, canonicalExperimentalMapHelperResolved)) {
      logicalResolvedMethod = canonicalExperimentalMapHelperResolved;
    }
  }

  if (resolvedMethod && (logicalResolvedMethod == "/array/count" ||
                         logicalResolvedMethod == "/vector/count" ||
                         logicalResolvedMethod == "/std/collections/vector/count" ||
                         logicalResolvedMethod == "/soa_vector/count" ||
                         logicalResolvedMethod == "/string/count" ||
                         logicalResolvedMethod == "/map/count" ||
                         logicalResolvedMethod == "/std/collections/map/count")) {
    handledOut = true;
    if (!expr.templateArgs.empty()) {
      error_ = "count does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "count does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin count";
      return false;
    }
    if (logicalResolvedMethod == "/map/count" ||
        logicalResolvedMethod == "/std/collections/map/count") {
      if (!context.resolveMapTarget(expr.args.front())) {
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        error_ = "count requires map target";
        return false;
      }
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (expr.isMethodCall && resolvedMethod &&
      (logicalResolvedMethod == "/std/collections/map/contains" ||
       logicalResolvedMethod == "/std/collections/map/tryAt" ||
       logicalResolvedMethod == "/std/collections/map/at" ||
       logicalResolvedMethod == "/std/collections/map/at_unsafe")) {
    handledOut = true;
    auto isCanonicalMapTypeText = [&](const std::string &typeText) {
      std::string normalizedType = normalizeBindingTypeName(typeText);
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(normalizedType, base, argText)) {
          return false;
        }
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        return base == "std/collections/map" || base == "/std/collections/map";
      }
    };
    auto usesCanonicalMapReceiver = [&](const Expr &receiverExpr) {
      auto bindingTypeText = [](const BindingInfo &binding) {
        if (binding.typeName == "Reference" || binding.typeName == "Pointer") {
          return binding.typeName + "<" + binding.typeTemplateArg + ">";
        }
        if (binding.typeTemplateArg.empty()) {
          return binding.typeName;
        }
        return binding.typeName + "<" + binding.typeTemplateArg + ">";
      };
      if (receiverExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding =
                findParamBinding(params, receiverExpr.name)) {
          return isCanonicalMapTypeText(bindingTypeText(*paramBinding));
        }
        if (auto itLocal = locals.find(receiverExpr.name);
            itLocal != locals.end()) {
          return isCanonicalMapTypeText(bindingTypeText(itLocal->second));
        }
      }
      BindingInfo inferredBinding;
      if (inferBindingTypeFromInitializer(receiverExpr, params, locals,
                                          inferredBinding)) {
        return isCanonicalMapTypeText(bindingTypeText(inferredBinding));
      }
      std::string typeText;
      return inferQueryExprTypeText(receiverExpr, params, locals, typeText) &&
             isCanonicalMapTypeText(typeText);
    };
    auto setCanonicalMapKeyMismatch =
        [&](const Expr &receiverExpr, const std::string &helperName,
            const std::string &mapKeyType) {
          if ((logicalResolvedMethod == "/std/collections/map/at" ||
               logicalResolvedMethod == "/std/collections/map/at_unsafe") &&
              usesCanonicalMapReceiver(receiverExpr)) {
            error_ = "argument type mismatch for " + logicalResolvedMethod +
                     " parameter key";
            return;
          }
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            error_ = helperName + " requires string map key";
          } else {
            error_ = helperName + " requires map key type " + mapKeyType;
          }
        };
    const std::string helperName =
        logicalResolvedMethod.substr(logicalResolvedMethod.find_last_of('/') +
                                     1);
    if (!expr.templateArgs.empty()) {
      error_ = helperName + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = helperName + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 2) {
      error_ = "argument count mismatch for builtin " + helperName;
      return false;
    }
    const Expr &receiverExpr = expr.args.front();
    const Expr &keyExpr = expr.args[1];
    std::string mapKeyType;
    if (!resolveMapKeyType(receiverExpr, *dispatchResolvers, mapKeyType)) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      error_ = helperName + " requires map target";
      return false;
    }
    if (!mapKeyType.empty()) {
      if (normalizeBindingTypeName(mapKeyType) == "string") {
        if (!isStringExprForArgumentValidation(keyExpr, *dispatchResolvers)) {
          setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
          return false;
        }
      } else {
        ReturnKind keyKind =
            returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
        if (keyKind != ReturnKind::Unknown) {
          if ((*dispatchResolvers).resolveStringTarget != nullptr &&
              (*dispatchResolvers).resolveStringTarget(keyExpr)) {
            setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
            return false;
          }
          ReturnKind candidateKind =
              inferExprReturnKind(keyExpr, params, locals);
          if (candidateKind != ReturnKind::Unknown &&
              candidateKind != keyKind) {
            setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
            return false;
          }
        }
      }
    }
    return validateExpr(params, locals, receiverExpr) &&
           validateExpr(params, locals, keyExpr);
  }

  if (!resolvedMethod && isVectorBuiltinName(expr, "count") &&
      !isArrayNamespacedVectorCountCompatibilityCall(expr, *dispatchResolvers) &&
      (!context.shouldBuiltinValidateStdNamespacedVectorCountCall &&
       !context.isStdNamespacedVectorCountCall) &&
      !context.isNamespacedMapCountCall && !context.isResolvedMapCountCall &&
      !isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals,
                                                 *dispatchResolverAdapters) &&
      it == defMap_.end()) {
    handledOut = true;
    if (!context.shouldBuiltinValidateBareMapCountCall) {
      Expr rewrittenMapHelperCall;
      if (tryRewriteBareMapHelperCall(expr, "count", *dispatchResolvers,
                                      rewrittenMapHelperCall)) {
        return validateExpr(params, locals, rewrittenMapHelperCall);
      }
    }
    if (!expr.templateArgs.empty()) {
      error_ = "count does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "count does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin count";
      return false;
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (resolvedMethod &&
      (resolved == "/vector/capacity" ||
       resolved == "/std/collections/vector/capacity")) {
    handledOut = true;
    if (!expr.templateArgs.empty()) {
      error_ = "capacity does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "capacity does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin capacity";
      return false;
    }
    std::string elemType;
    if (!context.resolveVectorTarget(expr.args.front(), elemType)) {
      error_ = "capacity requires vector target";
      return false;
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (!resolvedMethod && isVectorBuiltinName(expr, "capacity") &&
      (!context.shouldBuiltinValidateStdNamespacedVectorCapacityCall &&
       !context.isStdNamespacedVectorCapacityCall) &&
      it == defMap_.end()) {
    handledOut = true;
    if (!expr.templateArgs.empty()) {
      error_ = "capacity does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "capacity does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin capacity";
      return false;
    }
    std::string elemType;
    if (!context.resolveVectorTarget(expr.args.front(), elemType)) {
      error_ = "capacity requires vector target";
      return false;
    }
    return validateExpr(params, locals, expr.args.front());
  }

  return true;
}

} // namespace primec::semantics

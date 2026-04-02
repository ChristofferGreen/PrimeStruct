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
  const auto *dispatchResolverAdapters = context.dispatchResolverAdapters;
  const auto *dispatchResolvers = context.dispatchResolvers;
  if (dispatchResolverAdapters == nullptr || dispatchResolvers == nullptr ||
      context.resolveMapTarget == nullptr ||
      context.resolveVectorTarget == nullptr) {
    return true;
  }
  auto it = defMap_.find(resolved);

  const bool isDirectVectorCountWrapperCall =
      !expr.isMethodCall && !resolvedMethod &&
      (resolved.rfind("/std/collections/vectorCount", 0) == 0 ||
       resolved.rfind("/std/collections/experimental_vector/vectorCount", 0) == 0);
  const bool isDirectVectorCapacityWrapperCall =
      !expr.isMethodCall && !resolvedMethod &&
      (resolved.rfind("/std/collections/vectorCapacity", 0) == 0 ||
       resolved.rfind("/std/collections/experimental_vector/vectorCapacity", 0) == 0);
  const bool isDirectStdNamespacedVectorCountBuiltinCall =
      !expr.isMethodCall && !resolvedMethod &&
      context.shouldBuiltinValidateStdNamespacedVectorCountCall &&
      expr.args.size() == 1 &&
      resolved.rfind("/std/collections/vector/count", 0) == 0;
  const bool isDirectStdNamespacedVectorCapacityBuiltinCall =
      !expr.isMethodCall && !resolvedMethod &&
      context.shouldBuiltinValidateStdNamespacedVectorCapacityCall &&
      expr.args.size() == 1 &&
      resolved.rfind("/std/collections/vector/capacity", 0) == 0;
  const bool isDirectStdNamespacedSoaCountBuiltinCall =
      !expr.isMethodCall && !resolvedMethod &&
      expr.args.size() == 1 &&
      resolved.rfind("/std/collections/soa_vector/count", 0) == 0;
  auto inferDirectVectorElementType = [&](const Expr &target, std::string &elemTypeOut) {
    elemTypeOut.clear();
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      BindingInfo inferredBinding;
      const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
      }
      if (extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      const std::string normalizedBase = normalizeBindingTypeName(inferredBinding.typeName);
      if (normalizedBase == "vector" && !inferredBinding.typeTemplateArg.empty()) {
        elemTypeOut = inferredBinding.typeTemplateArg;
        return true;
      }
    }
    return context.resolveVectorTarget(target, elemTypeOut);
  };
  auto validateSoaHelperReturnTemplateArgs =
      [&](const Expr &receiverExpr, const std::string &elemType, const char *helperName) {
    if (expr.templateArgs.empty()) {
      return true;
    }
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding ||
        expr.templateArgs.size() != 1 ||
        normalizeBindingTypeName(expr.templateArgs.front()) !=
            normalizeBindingTypeName(elemType)) {
      error_ = std::string(helperName) + " does not accept template arguments";
      return false;
    }
    return true;
  };
  auto hasVisibleSamePathSoaCountHelper = [&]() {
    return hasDeclaredDefinitionPath("/soa_vector/count") ||
           hasImportedDefinitionPath("/soa_vector/count");
  };
  auto validateDirectVectorCountCapacityCall =
      [&](const char *helperName, const char *resolvedPath) {
        handledOut = true;
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = std::string(helperName) + " does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + std::string(helperName);
          return false;
        }
        std::string elemType;
        if (!inferDirectVectorElementType(expr.args.front(), elemType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          error_ = std::string(helperName) + " requires vector target";
          return false;
        }
        std::string expectedElemType;
        if (!expr.templateArgs.empty()) {
          if (expr.templateArgs.size() != 1) {
            error_ = "argument type mismatch for " + std::string(resolvedPath) +
                     " parameter values";
            return false;
          }
          expectedElemType = expr.templateArgs.front();
        } else if (it != defMap_.end() && it->second != nullptr &&
                   !it->second->parameters.empty()) {
          BindingInfo paramBinding;
          std::optional<std::string> restrictType;
          std::string parseError;
          if (parseBindingInfo(it->second->parameters.front(),
                               it->second->namespacePrefix,
                               structNames_,
                               importAliases_,
                               paramBinding,
                               restrictType,
                               parseError)) {
            if (normalizeBindingTypeName(paramBinding.typeName) == "vector" &&
                !paramBinding.typeTemplateArg.empty()) {
              expectedElemType = paramBinding.typeTemplateArg;
            } else {
              extractExperimentalVectorElementType(paramBinding, expectedElemType);
            }
          }
        }
        if (!expectedElemType.empty() &&
            normalizeBindingTypeName(expectedElemType) !=
                normalizeBindingTypeName(elemType)) {
          error_ = "argument type mismatch for " + std::string(resolvedPath) +
                   " parameter values";
          return false;
        }
        return validateExpr(params, locals, expr.args.front());
      };
  if (isDirectVectorCountWrapperCall) {
    return validateDirectVectorCountCapacityCall("count", "/std/collections/vectorCount");
  }
  if (isDirectVectorCapacityWrapperCall) {
    return validateDirectVectorCountCapacityCall("capacity", "/std/collections/vectorCapacity");
  }
  if (isDirectStdNamespacedVectorCountBuiltinCall) {
    return validateDirectVectorCountCapacityCall("count", "/std/collections/vector/count");
  }
  if (isDirectStdNamespacedVectorCapacityBuiltinCall) {
    return validateDirectVectorCountCapacityCall("capacity", "/std/collections/vector/capacity");
  }
  if (isDirectStdNamespacedSoaCountBuiltinCall) {
    handledOut = true;
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "count does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin count";
      return false;
    }
    std::string elemType;
    if (!(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),
                                                     elemType)) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      error_ = "count requires soa_vector target";
      return false;
    }
    if (!validateSoaHelperReturnTemplateArgs(expr.args.front(), elemType, "count")) {
      return false;
    }
    return validateExpr(params, locals, expr.args.front());
  }
  if (it != defMap_.end() && !resolvedMethod) {
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
  const bool isExplicitCanonicalMapCountCall =
      !expr.isMethodCall &&
      (expr.name.rfind("/std/collections/map/count", 0) == 0 ||
       expr.namespacePrefix == "/std/collections/map" ||
       expr.namespacePrefix == "std/collections/map");

  if (resolvedMethod && (logicalResolvedMethod == "/array/count" ||
                         logicalResolvedMethod == "/vector/count" ||
                         logicalResolvedMethod == "/std/collections/vector/count" ||
                         logicalResolvedMethod == "/soa_vector/count" ||
                         logicalResolvedMethod == "/std/collections/soa_vector/count" ||
                         logicalResolvedMethod == "/string/count" ||
                         logicalResolvedMethod == "/map/count" ||
                         logicalResolvedMethod == "/std/collections/map/count")) {
    handledOut = true;
    if (logicalResolvedMethod == "/std/collections/map/count" &&
        isExplicitCanonicalMapCountCall &&
        !hasImportedDefinitionPath("/std/collections/map/count") &&
        !hasDeclaredDefinitionPath("/std/collections/map/count")) {
      error_ = "unknown call target: /std/collections/map/count";
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
    } else if (logicalResolvedMethod == "/soa_vector/count" ||
               logicalResolvedMethod == "/std/collections/soa_vector/count") {
      std::string elemType;
      if (!(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),
                                                       elemType)) {
        if (logicalResolvedMethod == "/soa_vector/count" &&
            hasVisibleSamePathSoaCountHelper()) {
          handledOut = false;
          return true;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        error_ = "count requires soa_vector target";
        return false;
      }
      if (!validateSoaHelperReturnTemplateArgs(expr.args.front(), elemType, "count")) {
        return false;
      }
    } else if (!expr.templateArgs.empty()) {
      error_ = "count does not accept template arguments";
      return false;
    }
    return validateExpr(params, locals, expr.args.front());
  }

  const bool shouldBuiltinValidateMapCountCall =
      !expr.isMethodCall && !hasNamedArguments(expr.argNames) &&
      (context.isNamespacedMapCountCall || context.isResolvedMapCountCall ||
       isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals,
                                                 *dispatchResolverAdapters));
  if (shouldBuiltinValidateMapCountCall && it == defMap_.end()) {
    handledOut = true;
    if (isExplicitCanonicalMapCountCall &&
        !hasImportedDefinitionPath("/std/collections/map/count") &&
        !hasDeclaredDefinitionPath("/std/collections/map/count")) {
      error_ = "unknown call target: /std/collections/map/count";
      return false;
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
    if (!context.resolveMapTarget(expr.args.front())) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      error_ = "count requires map target";
      return false;
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

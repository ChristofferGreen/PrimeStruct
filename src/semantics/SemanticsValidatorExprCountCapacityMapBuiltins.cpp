#include "SemanticsValidator.h"

#include <string>
#include <utility>
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

  auto failCountCapacityMapBuiltin = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  const bool isDirectExperimentalVectorCountCall =
      !expr.isMethodCall && !resolvedMethod &&
      resolved.rfind("/std/collections/experimental_vector/vectorCount", 0) == 0;
  const bool isDirectExperimentalVectorCapacityCall =
      !expr.isMethodCall && !resolvedMethod &&
      resolved.rfind("/std/collections/experimental_vector/vectorCapacity", 0) == 0;
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
  auto canonicalizeSoaCountHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  auto isCanonicalSoaCountHelperPath = [](const std::string &candidate) {
    return candidate.rfind("/std/collections/soa_vector/", 0) == 0 &&
           isLegacyOrCanonicalSoaHelperPath(candidate, "count");
  };
  const std::string resolvedSoaCountCanonical =
      canonicalizeSoaCountHelperPath(resolved);
  const bool isDirectStdNamespacedSoaCountBuiltinCall =
      !expr.isMethodCall && !resolvedMethod &&
      expr.args.size() == 1 &&
      isCanonicalSoaCountHelperPath(resolvedSoaCountCanonical);
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
      return failCountCapacityMapBuiltin(std::string(helperName) +
                                         " does not accept template arguments");
    }
    return true;
  };
  auto validateDirectVectorCountCapacityCall =
      [&](const char *helperName, const char *resolvedPath) {
        handledOut = true;
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          return failCountCapacityMapBuiltin(std::string(helperName) +
                                             " does not accept block arguments");
        }
        if (expr.args.size() != 1) {
          return failCountCapacityMapBuiltin("argument count mismatch for builtin " +
                                             std::string(helperName));
        }
        std::string elemType;
        if (!inferDirectVectorElementType(expr.args.front(), elemType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return failCountCapacityMapBuiltin(std::string(helperName) +
                                             " requires vector target");
        }
        std::string expectedElemType;
        if (!expr.templateArgs.empty()) {
          if (expr.templateArgs.size() != 1) {
            return failCountCapacityMapBuiltin("argument type mismatch for " +
                                               std::string(resolvedPath) +
                                               " parameter values");
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
          return failCountCapacityMapBuiltin("argument type mismatch for " +
                                             std::string(resolvedPath) +
                                             " parameter values");
        }
        return validateExpr(params, locals, expr.args.front());
      };
  if (isDirectExperimentalVectorCountCall) {
    return validateDirectVectorCountCapacityCall(
        "count", "/std/collections/experimental_vector/vectorCount");
  }
  if (isDirectExperimentalVectorCapacityCall) {
    return validateDirectVectorCountCapacityCall(
        "capacity", "/std/collections/experimental_vector/vectorCapacity");
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
      return failCountCapacityMapBuiltin("count does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityMapBuiltin("argument count mismatch for builtin count");
    }
    std::string elemType;
    if (!(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),
                                                     elemType)) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      return failCountCapacityMapBuiltin("count requires soa_vector target");
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
  const std::string logicalSoaCountCanonical =
      canonicalizeSoaCountHelperPath(logicalResolvedMethod);
  const auto isExplicitOldSurfaceSoaCountCall = [&]() {
    const std::string normalizedName = canonicalizeSoaCountHelperPath(expr.name);
    const std::string normalizedNamespacePrefix =
        canonicalizeSoaCountHelperPath(expr.namespacePrefix);
    if (!expr.isMethodCall) {
      if (normalizedName == "/soa_vector/count") {
        return true;
      }
      return (normalizedNamespacePrefix == "/soa_vector" ||
              normalizedNamespacePrefix == "soa_vector") &&
             expr.name == "count";
    }
    return normalizedNamespacePrefix == "/soa_vector" &&
           expr.name == "count";
  };
  const bool isExplicitCanonicalMapCountCall =
      !expr.isMethodCall &&
      (logicalResolvedMethod == "/std/collections/map/count" ||
       logicalResolvedMethod == "/std/collections/map/count_ref" ||
       expr.name.rfind("/std/collections/map/count", 0) == 0 ||
       expr.namespacePrefix == "/std/collections/map" ||
       expr.namespacePrefix == "std/collections/map");

  if (resolvedMethod && (logicalResolvedMethod == "/array/count" ||
                         logicalResolvedMethod == "/vector/count" ||
                         logicalResolvedMethod == "/std/collections/vector/count" ||
                         isLegacyOrCanonicalSoaHelperPath(
                             logicalSoaCountCanonical,
                             "count") ||
                         logicalResolvedMethod == "/string/count" ||
                         logicalResolvedMethod == "/map/count" ||
                         logicalResolvedMethod == "/std/collections/map/count" ||
                         logicalResolvedMethod == "/std/collections/map/count_ref")) {
    handledOut = true;
    if ((logicalResolvedMethod == "/std/collections/map/count" ||
         logicalResolvedMethod == "/std/collections/map/count_ref") &&
        isExplicitCanonicalMapCountCall &&
        !hasImportedDefinitionPath(logicalResolvedMethod) &&
        !hasDeclaredDefinitionPath(logicalResolvedMethod)) {
      return failCountCapacityMapBuiltin(
          "unknown call target: " + logicalResolvedMethod);
    }
    const std::string countHelperName =
        logicalResolvedMethod == "/std/collections/map/count_ref" ? "count_ref" : "count";
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityMapBuiltin(countHelperName + " does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityMapBuiltin("argument count mismatch for builtin " + countHelperName);
    }
    if (logicalResolvedMethod == "/map/count" ||
        logicalResolvedMethod == "/std/collections/map/count" ||
        logicalResolvedMethod == "/std/collections/map/count_ref") {
      if (!context.resolveMapTarget(expr.args.front())) {
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return failCountCapacityMapBuiltin(countHelperName + " requires map target");
      }
    } else if (isLegacyOrCanonicalSoaHelperPath(
                   logicalSoaCountCanonical,
                   "count")) {
      if (isExplicitOldSurfaceSoaCountCall() &&
          !hasVisibleDefinitionPathForCurrentImports("/soa_vector/count")) {
        return failCountCapacityMapBuiltin(
            soaUnavailableMethodDiagnostic("/soa_vector/count"));
      }
      std::string elemType;
      if (!(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),
                                                       elemType)) {
        const bool oldSurfaceCallShape =
            isSimpleCallName(expr, "count") ||
            (expr.isMethodCall && expr.name == "count") ||
            isLegacyOrCanonicalSoaHelperPath(
                logicalSoaCountCanonical,
                "count");
        if (oldSurfaceCallShape &&
            hasVisibleSoaHelperTargetForCurrentImports("count")) {
          handledOut = false;
          return true;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return failCountCapacityMapBuiltin("count requires soa_vector target");
      }
      if (!validateSoaHelperReturnTemplateArgs(expr.args.front(), elemType, "count")) {
        return false;
      }
    } else if (!expr.templateArgs.empty()) {
      return failCountCapacityMapBuiltin(
          "count does not accept template arguments");
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
        !hasImportedDefinitionPath(logicalResolvedMethod == "/std/collections/map/count_ref"
                                       ? "/std/collections/map/count_ref"
                                       : "/std/collections/map/count") &&
        !hasDeclaredDefinitionPath(logicalResolvedMethod == "/std/collections/map/count_ref"
                                       ? "/std/collections/map/count_ref"
                                       : "/std/collections/map/count")) {
      return failCountCapacityMapBuiltin(
          std::string("unknown call target: ") +
          (logicalResolvedMethod == "/std/collections/map/count_ref"
               ? "/std/collections/map/count_ref"
               : "/std/collections/map/count"));
    }
    const std::string countHelperName =
        logicalResolvedMethod == "/std/collections/map/count_ref" ? "count_ref" : "count";
    if (!expr.templateArgs.empty()) {
      return failCountCapacityMapBuiltin(
          countHelperName + " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityMapBuiltin(countHelperName + " does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityMapBuiltin("argument count mismatch for builtin " + countHelperName);
    }
    if (!context.resolveMapTarget(expr.args.front())) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      return failCountCapacityMapBuiltin(countHelperName + " requires map target");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (expr.isMethodCall && resolvedMethod &&
      (logicalResolvedMethod == "/std/collections/map/contains" ||
       logicalResolvedMethod == "/std/collections/map/contains_ref" ||
       logicalResolvedMethod == "/std/collections/map/tryAt" ||
       logicalResolvedMethod == "/std/collections/map/tryAt_ref" ||
       logicalResolvedMethod == "/std/collections/map/at" ||
       logicalResolvedMethod == "/std/collections/map/at_ref" ||
       logicalResolvedMethod == "/std/collections/map/at_unsafe" ||
       logicalResolvedMethod == "/std/collections/map/at_unsafe_ref")) {
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
        return base == "std/collections/map" || base == "/std/collections/map" ||
               base == "Map" || base == "/Map" ||
               base == "std/collections/experimental_map/Map" ||
               base == "/std/collections/experimental_map/Map";
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
            const std::string &mapKeyType) -> bool {
          if ((logicalResolvedMethod == "/std/collections/map/at" ||
               logicalResolvedMethod == "/std/collections/map/at_ref" ||
               logicalResolvedMethod == "/std/collections/map/at_unsafe" ||
               logicalResolvedMethod == "/std/collections/map/at_unsafe_ref") &&
              usesCanonicalMapReceiver(receiverExpr)) {
            return failCountCapacityMapBuiltin("argument type mismatch for " +
                                               logicalResolvedMethod +
                                               " parameter key");
          }
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            return failCountCapacityMapBuiltin(helperName +
                                               " requires string map key");
          }
          return failCountCapacityMapBuiltin(helperName +
                                             " requires map key type " +
                                             mapKeyType);
        };
    const std::string helperName =
        logicalResolvedMethod.substr(logicalResolvedMethod.find_last_of('/') +
                                     1);
    if (!expr.templateArgs.empty()) {
      return failCountCapacityMapBuiltin(helperName +
                                         " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityMapBuiltin(helperName +
                                         " does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failCountCapacityMapBuiltin("argument count mismatch for builtin " +
                                         helperName);
    }
    const Expr &receiverExpr = expr.args.front();
    const Expr &keyExpr = expr.args[1];
    std::string mapKeyType;
    if (!resolveMapKeyType(receiverExpr, *dispatchResolvers, mapKeyType)) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      return failCountCapacityMapBuiltin(helperName + " requires map target");
    }
    if (!mapKeyType.empty()) {
      if (normalizeBindingTypeName(mapKeyType) == "string") {
        if (!isStringExprForArgumentValidation(keyExpr, *dispatchResolvers)) {
          return setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
        }
      } else {
        ReturnKind keyKind =
            returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
        if (keyKind != ReturnKind::Unknown) {
          if ((*dispatchResolvers).resolveStringTarget != nullptr &&
              (*dispatchResolvers).resolveStringTarget(keyExpr)) {
            setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
            return failExprDiagnostic(expr, error_);
          }
          ReturnKind candidateKind =
              inferExprReturnKind(keyExpr, params, locals);
          if (candidateKind != ReturnKind::Unknown &&
              candidateKind != keyKind) {
            return setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
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
      return failCountCapacityMapBuiltin(
          "count does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityMapBuiltin("count does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityMapBuiltin("argument count mismatch for builtin count");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (resolvedMethod &&
      (resolved == "/vector/capacity" ||
       resolved == "/std/collections/vector/capacity")) {
    handledOut = true;
    if (!expr.templateArgs.empty()) {
      return failCountCapacityMapBuiltin(
          "capacity does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityMapBuiltin(
          "capacity does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityMapBuiltin(
          "argument count mismatch for builtin capacity");
    }
    std::string elemType;
    if (!context.resolveVectorTarget(expr.args.front(), elemType)) {
      return failCountCapacityMapBuiltin("capacity requires vector target");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (!resolvedMethod && isVectorBuiltinName(expr, "capacity") &&
      (!context.shouldBuiltinValidateStdNamespacedVectorCapacityCall &&
       !context.isStdNamespacedVectorCapacityCall) &&
      it == defMap_.end()) {
    handledOut = true;
    if (!expr.templateArgs.empty()) {
      return failCountCapacityMapBuiltin(
          "capacity does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityMapBuiltin(
          "capacity does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityMapBuiltin(
          "argument count mismatch for builtin capacity");
    }
    std::string elemType;
    if (!context.resolveVectorTarget(expr.args.front(), elemType)) {
      return failCountCapacityMapBuiltin("capacity requires vector target");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  return true;
}

} // namespace primec::semantics

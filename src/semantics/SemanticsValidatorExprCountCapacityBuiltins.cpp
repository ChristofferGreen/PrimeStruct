#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprCountCapacityBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprCountCapacityBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  const auto *dispatchResolvers = context.dispatchResolvers;
  if (dispatchResolvers == nullptr || context.resolveVectorTarget == nullptr) {
    return true;
  }
  auto it = defMap_.find(resolved);

  auto failCountCapacityBuiltin = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  const std::string canonicalVectorCountPath =
      canonicalVectorCompatibilityHelperPath("count");
  const std::string canonicalVectorCapacityPath =
      canonicalVectorCompatibilityHelperPath("capacity");
  const std::string legacyExperimentalVectorCountPath =
      legacyExperimentalVectorCompatibilityHelperPath("count");
  const std::string legacyExperimentalVectorCapacityPath =
      legacyExperimentalVectorCompatibilityHelperPath("capacity");
  const bool isDirectExperimentalVectorCountCall =
      !expr.isMethodCall && !resolvedMethod &&
      matchesResolvedPath(resolved, legacyExperimentalVectorCountPath);
  const bool isDirectExperimentalVectorCapacityCall =
      !expr.isMethodCall && !resolvedMethod &&
      matchesResolvedPath(resolved, legacyExperimentalVectorCapacityPath);
  auto canonicalizeSoaCountHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  auto isCanonicalSoaCountHelperPath = [](const std::string &candidate) {
    return isCanonicalStdlibSoaHelperPath(candidate, "count") ||
           isCanonicalStdlibSoaHelperPath(candidate, "count_ref");
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
      return failCountCapacityBuiltin(std::string(helperName) +
                                         " does not accept template arguments");
    }
    return true;
  };
  auto validateDirectVectorCountCapacityCall =
      [&](const char *helperName, const std::string &resolvedPath) {
        handledOut = true;
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          return failCountCapacityBuiltin(std::string(helperName) +
                                             " does not accept block arguments");
        }
        if (expr.args.size() != 1) {
          return failCountCapacityBuiltin("argument count mismatch for builtin " +
                                             std::string(helperName));
        }
        std::string elemType;
        if (!inferDirectVectorElementType(expr.args.front(), elemType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return failCountCapacityBuiltin(std::string(helperName) +
                                             " requires vector target");
        }
        std::string expectedElemType;
        if (!expr.templateArgs.empty()) {
          if (expr.templateArgs.size() != 1) {
            return failCountCapacityBuiltin("argument type mismatch for " +
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
          return failCountCapacityBuiltin("argument type mismatch for " +
                                             std::string(resolvedPath) +
                                             " parameter values");
        }
        return validateExpr(params, locals, expr.args.front());
      };
  if (isDirectExperimentalVectorCountCall) {
    return validateDirectVectorCountCapacityCall(
        "count", legacyExperimentalVectorCountPath);
  }
  if (isDirectExperimentalVectorCapacityCall) {
    return validateDirectVectorCountCapacityCall(
        "capacity", legacyExperimentalVectorCapacityPath);
  }
  if (isImportedResolvedStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "count",
          hasImportedDefinitionPath(canonicalVectorCountPath),
          resolvedMethod,
          expr.args.size(),
          resolved)) {
    return validateDirectVectorCountCapacityCall("count",
                                                canonicalVectorCountPath);
  }
  if (isImportedResolvedStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "capacity",
          hasImportedDefinitionPath(canonicalVectorCapacityPath),
          resolvedMethod,
          expr.args.size(),
          resolved)) {
    return validateDirectVectorCountCapacityCall("capacity",
                                                canonicalVectorCapacityPath);
  }
  if (isDirectStdNamespacedSoaCountBuiltinCall) {
    handledOut = true;
    const std::string soaCountHelperName =
        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCountCanonical, "count_ref")
            ? "count_ref"
            : "count";
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityBuiltin(soaCountHelperName +
                                         " does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityBuiltin("argument count mismatch for builtin " +
                                         soaCountHelperName);
    }
    std::string elemType;
    if (!(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),
                                                     elemType)) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      return failCountCapacityBuiltin(soaCountHelperName +
                                         " requires soa target");
    }
    if (!validateSoaHelperReturnTemplateArgs(expr.args.front(), elemType,
                                            soaCountHelperName.c_str())) {
      return false;
    }
    return validateExpr(params, locals, expr.args.front());
  }
  if (it != defMap_.end() && !resolvedMethod) {
    return true;
  }

  const std::string logicalResolvedMethod = resolved;
  const std::string logicalSoaCountCanonical =
      canonicalizeSoaCountHelperPath(logicalResolvedMethod);
  const auto isExplicitOldSurfaceSoaCountCall = [&]() {
    const std::string normalizedName = canonicalizeSoaCountHelperPath(expr.name);
    const std::string normalizedNamespacePrefix =
        canonicalizeSoaCountHelperPath(expr.namespacePrefix);
    if (!expr.isMethodCall) {
      if (normalizedName == "/soa" "_vector/count") {
        return true;
      }
      if (normalizedName == "/soa" "_vector/count_ref") {
        return true;
      }
      return (normalizedNamespacePrefix == "/soa" "_vector" ||
              normalizedNamespacePrefix == "soa" "_vector") &&
             (expr.name == "count" || expr.name == "count_ref");
    }
    return normalizedNamespacePrefix == "/soa" "_vector" &&
           (expr.name == "count" || expr.name == "count_ref");
  };
  const auto validateVectorCountBuiltinCall = [&]() -> bool {
    handledOut = true;
    if (expr.args.size() != 1) {
      return failCountCapacityBuiltin(
          "argument count mismatch for builtin count");
    }
    if (!expr.templateArgs.empty()) {
      return failCountCapacityBuiltin(
          "count does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityBuiltin("count does not accept block arguments");
    }
    return validateExpr(params, locals, expr.args.front());
  };
  const auto validateVectorCountBuiltinPath = [&]() -> bool {
    handledOut = true;
    return validateVectorCountBuiltinCall();
  };
  const bool shouldValidateVectorCountBuiltinFallback =
      !resolvedMethod && isVectorBuiltinName(expr, "count") &&
      !isArrayNamespacedVectorCountCompatibilityCall(expr, *dispatchResolvers) &&
      !isStdNamespacedVectorCompatibilityDirectCall(expr.isMethodCall,
                                                    resolveCalleePath(expr),
                                                    "count") &&
      it == defMap_.end();
  const auto tryValidateVectorCountBuiltinPath = [&]() -> std::optional<bool> {
    if (resolvedMethod &&
        isStdNamespacedVectorCompatibilityHelperPath(logicalResolvedMethod,
                                                     "count")) {
      return validateVectorCountBuiltinPath();
    }
    if (shouldValidateVectorCountBuiltinFallback) {
      return validateVectorCountBuiltinPath();
    }
    return std::nullopt;
  };

  if (std::optional<bool> validatedVectorCountBuiltinPath =
          tryValidateVectorCountBuiltinPath()) {
    return *validatedVectorCountBuiltinPath;
  }

  if ((resolvedMethod && (logicalResolvedMethod == "/array/count" ||
                          isLegacyOrCanonicalSoaHelperPath(
                              logicalSoaCountCanonical,
                              "count") ||
                          isLegacyOrCanonicalSoaHelperPath(
                              logicalSoaCountCanonical,
                              "count_ref") ||
                          logicalResolvedMethod == "/string/count"))) {
    handledOut = true;
    const std::string countHelperName = "count";
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityBuiltin(countHelperName + " does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failCountCapacityBuiltin("argument count mismatch for builtin " + countHelperName);
    }
    if (isLegacyOrCanonicalSoaHelperPath(
                   logicalSoaCountCanonical,
                   "count") ||
               isLegacyOrCanonicalSoaHelperPath(
                   logicalSoaCountCanonical,
                   "count_ref")) {
      const std::string soaCountHelperName =
          isLegacyOrCanonicalSoaHelperPath(logicalSoaCountCanonical, "count_ref")
              ? "count_ref"
              : "count";
      const bool explicitOldSurfaceSoaCountCall =
          isExplicitOldSurfaceSoaCountCall();
      const bool hasVisibleSamePathSoaCountHelper =
          hasVisibleDefinitionPathForCurrentImports("/soa" "_vector/" +
                                                    soaCountHelperName);
      if (explicitOldSurfaceSoaCountCall &&
          !hasVisibleDefinitionPathForCurrentImports("/soa" "_vector/" +
                                                    soaCountHelperName)) {
        return failCountCapacityBuiltin(
            soaUnavailableMethodDiagnostic("/soa" "_vector/" +
                                           soaCountHelperName));
      }
      std::string argsPackElemType;
      if ((*dispatchResolvers).resolveArgsPackAccessTarget != nullptr &&
          (*dispatchResolvers).resolveArgsPackAccessTarget(expr.args.front(),
                                                          argsPackElemType)) {
        return validateVectorCountBuiltinCall();
      }
      std::string elemType;
      if (!(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),
                                                       elemType)) {
        const bool oldSurfaceCallShape =
            isSimpleCallName(expr, soaCountHelperName.c_str()) ||
            (expr.isMethodCall && expr.name == soaCountHelperName) ||
            isLegacyOrCanonicalSoaHelperPath(
                logicalSoaCountCanonical,
                soaCountHelperName);
        if ((explicitOldSurfaceSoaCountCall &&
             hasVisibleSoaHelperTargetForCurrentImports(soaCountHelperName)) ||
            (!explicitOldSurfaceSoaCountCall && oldSurfaceCallShape &&
             hasVisibleSamePathSoaCountHelper)) {
          handledOut = false;
          return true;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return failCountCapacityBuiltin(soaCountHelperName +
                                           " requires soa target");
      }
      if (!validateSoaHelperReturnTemplateArgs(expr.args.front(), elemType,
                                              soaCountHelperName.c_str())) {
        return false;
      }
    } else if (!expr.templateArgs.empty()) {
      return failCountCapacityBuiltin(
          "count does not accept template arguments");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  const auto validateVectorCapacityBuiltinCall = [&]() -> bool {
    handledOut = true;
    if (expr.args.size() != 1) {
      return failCountCapacityBuiltin(
          "argument count mismatch for builtin capacity");
    }
    if (!expr.templateArgs.empty()) {
      return failCountCapacityBuiltin(
          "capacity does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCountCapacityBuiltin(
          "capacity does not accept block arguments");
    }
    std::string elemType;
    if (!context.resolveVectorTarget(expr.args.front(), elemType)) {
      return failCountCapacityBuiltin(
          vectorCompatibilityRequiresVectorTargetDiagnostic("capacity"));
    }
    return validateExpr(params, locals, expr.args.front());
  };

  if (resolvedMethod &&
      matchesResolvedPath(resolved, canonicalVectorCapacityPath)) {
    return validateVectorCapacityBuiltinCall();
  }

  if (!resolvedMethod && isVectorBuiltinName(expr, "capacity") &&
      !isStdNamespacedVectorCompatibilityDirectCall(expr.isMethodCall,
                                                    resolveCalleePath(expr),
                                                    "capacity") &&
      it == defMap_.end()) {
    return validateVectorCapacityBuiltinCall();
  }

  return true;
}

} // namespace primec::semantics

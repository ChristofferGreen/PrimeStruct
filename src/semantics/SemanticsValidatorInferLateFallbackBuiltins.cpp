// soa-surface-audit: exempt
#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string_view>
#include <utility>

namespace primec::semantics {
namespace {

const StdlibSurfaceMetadata *lateFallbackKeyValueHelperSurfaceMetadata() {
  return keyValueHelperSurfaceMetadataLocal();
}

bool resolveLateFallbackKeyValueHelperName(std::string path,
                                           std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      lateFallbackKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr || path.empty()) {
    return false;
  }
  if (path.find('/') != std::string::npos && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  if (path.find('/') != std::string::npos) {
    const StdlibSurfaceMetadata *pathMetadata =
        findStdlibSurfaceMetadataByResolvedPath(path);
    if (pathMetadata == nullptr || pathMetadata->id != metadata->id) {
      return false;
    }
  }
  const std::string_view helperName =
      resolveStdlibSurfaceMemberName(*metadata, path);
  if (helperName.empty()) {
    return false;
  }
  helperNameOut.assign(helperName);
  return true;
}

std::string lateFallbackCanonicalKeyValueHelperPath(
    std::string_view helperName) {
  std::string resolvedHelperName;
  if (!resolveLateFallbackKeyValueHelperName(std::string(helperName),
                                             resolvedHelperName)) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata =
      lateFallbackKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return std::string(metadata->canonicalPath) + "/" + resolvedHelperName;
}

bool resolveLateFallbackCanonicalKeyValueHelperName(
    std::string path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      lateFallbackKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr || path.empty()) {
    return false;
  }
  if (path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  const std::string canonicalPrefix =
      std::string(metadata->canonicalPath) + "/";
  if (path.rfind(canonicalPrefix, 0) != 0) {
    return false;
  }
  return resolveLateFallbackKeyValueHelperName(std::move(path), helperNameOut);
}

bool isCanonicalKeyValueContainsHelperPath(const std::string &path) {
  std::string helperName;
  return resolveLateFallbackCanonicalKeyValueHelperName(path, helperName) &&
         (helperName == "contains" || helperName == "contains_ref");
}

bool isCanonicalKeyValueTryAtHelperPath(const std::string &path) {
  std::string helperName;
  return resolveLateFallbackCanonicalKeyValueHelperName(path, helperName) &&
         (helperName == "tryAt" || helperName == "tryAt_ref");
}

bool isCanonicalKeyValueAccessHelperPath(const std::string &path) {
  std::string helperName;
  return resolveLateFallbackCanonicalKeyValueHelperName(path, helperName) &&
         (helperName == "at" || helperName == "at_ref" ||
          helperName == "at_unsafe" || helperName == "at_unsafe_ref");
}

bool isLateFallbackKeyValueAccessHelperName(std::string_view helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

bool isCollectionPairImportAliasAccessHelperPath(std::string path) {
  const StdlibSurfaceMetadata *metadata =
      lateFallbackKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr || path.empty()) {
    return false;
  }
  if (path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  std::string helperName;
  if (!resolveLateFallbackKeyValueHelperName(path, helperName) ||
      !isLateFallbackKeyValueAccessHelperName(helperName)) {
    return false;
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    std::string rootedAlias(alias);
    if (!rootedAlias.empty() && rootedAlias.front() != '/') {
      rootedAlias.insert(rootedAlias.begin(), '/');
    }
    if (!rootedAlias.empty() && path.size() > rootedAlias.size() &&
        path.rfind(rootedAlias + "/", 0) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace

ReturnKind SemanticsValidator::inferLateFallbackReturnKind(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const InferLateFallbackBuiltinContext &context,
    bool &handled) {
  handled = false;
  if (context.collectionDispatchSetup == nullptr || context.dispatchResolvers == nullptr) {
    return ReturnKind::Unknown;
  }

  const auto &inferCollectionDispatchSetup = *context.collectionDispatchSetup;
  const auto &builtinCollectionDispatchResolvers = *context.dispatchResolvers;
  const auto &resolveArgsPackAccessTarget =
      builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
  const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
  const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
  const auto &resolveSoaVectorTarget =
      builtinCollectionDispatchResolvers.resolveSoaVectorTarget;
  const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;
  const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
  const auto &resolveKeyValueTarget =
      builtinCollectionDispatchResolvers.resolveMapTarget;
  const auto &resolveExperimentalKeyValueTarget =
      builtinCollectionDispatchResolvers.resolveKeyValueTarget;
  auto finish = [&](ReturnKind kind) -> ReturnKind {
    handled = true;
    return kind;
  };
  auto failInferLateFallbackDiagnostic = [&](std::string message) -> ReturnKind {
    (void)failExprDiagnostic(expr, std::move(message));
    return finish(ReturnKind::Unknown);
  };
  auto isCanonicalKeyValueAccessHelperName =
      [&](const std::string &helperName) {
    return helperName == "at" || helperName == "at_ref" ||
           helperName == "at_unsafe" || helperName == "at_unsafe_ref";
  };

  const auto resolvedIt = defMap_.find(context.resolved);
  if (!expr.isMethodCall &&
      (isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos") ||
       isSimpleCallName(expr, "to_aos_ref")) &&
      expr.args.size() == 1 && resolvedIt == defMap_.end()) {
    std::string elemType;
    if (isSimpleCallName(expr, "to_soa")) {
      if (resolveVectorTarget != nullptr &&
          resolveVectorTarget(expr.args.front(), elemType)) {
        return finish(ReturnKind::Array);
      }
    } else if (resolveSoaVectorTarget != nullptr &&
               resolveSoaVectorTarget(expr.args.front(), elemType)) {
      return finish(ReturnKind::Array);
    }
  }

  const auto &countCapacityContext =
      inferCollectionDispatchSetup
          .builtinCollectionCountCapacityDispatchContext;
  if (resolvedIt == defMap_.end() &&
      (countCapacityContext.isCountLike ||
       countCapacityContext.isCapacityLike)) {
    ReturnKind builtinCollectionKind = ReturnKind::Unknown;
    if (resolveBuiltinCollectionCountCapacityReturnKind(
            expr, countCapacityContext, builtinCollectionKind)) {
      return finish(builtinCollectionKind);
    }
  }

  const bool isBuiltinGet = isSimpleCallName(expr, "get");
  const bool isBuiltinGetRef = isSimpleCallName(expr, "get_ref");
  const bool isBuiltinRef = isSimpleCallName(expr, "ref");
  if (!expr.isMethodCall &&
      ((inferCollectionDispatchSetup.isBuiltinAccess && !expr.args.empty()) ||
       ((isBuiltinGet || isBuiltinGetRef || isBuiltinRef) &&
        expr.args.size() == 2)) &&
      (resolvedIt == defMap_.end() ||
       inferCollectionDispatchSetup.shouldDeferNamespacedVectorAccessCall ||
       inferCollectionDispatchSetup.shouldDeferNamespacedKeyValueAccessCall)) {
    const std::string helperName =
        inferCollectionDispatchSetup.isBuiltinAccess
            ? inferCollectionDispatchSetup.builtinAccessName
            : (isBuiltinGet ? "get" : (isBuiltinGetRef ? "get_ref" : "ref"));
    auto isCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      return (resolveVectorTarget != nullptr &&
              resolveVectorTarget(candidate, elemType)) ||
             (resolveArrayTarget != nullptr &&
              resolveArrayTarget(candidate, elemType)) ||
             (resolveSoaVectorTarget != nullptr &&
              resolveSoaVectorTarget(candidate, elemType)) ||
             (resolveStringTarget != nullptr && resolveStringTarget(candidate)) ||
             (resolveKeyValueTarget != nullptr &&
              resolveKeyValueTarget(candidate, keyType, valueType)) ||
             (resolveExperimentalKeyValueTarget != nullptr &&
              resolveExperimentalKeyValueTarget(candidate, keyType, valueType));
    };
    auto tryResolveReceiverIndex = [&](size_t index,
                                       bool skipResolvedResult,
                                       ReturnKind &resolvedKindOut) -> bool {
      if (index >= expr.args.size()) {
        return false;
      }
      const Expr &receiverCandidate = expr.args[index];
      std::string methodResolved;
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (resolveVectorTarget != nullptr &&
          resolveVectorTarget(receiverCandidate, elemType)) {
        methodResolved =
            canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      } else if (resolveArrayTarget != nullptr &&
                 resolveArrayTarget(receiverCandidate, elemType)) {
        methodResolved = "/array/" + helperName;
      } else if ((helperName == "get" || helperName == "get_ref" ||
                  helperName == "ref" ||
                  helperName == "ref_ref") &&
                 resolveSoaVectorTarget != nullptr &&
                 resolveSoaVectorTarget(receiverCandidate, elemType)) {
        methodResolved =
            preferredSoaHelperTargetForCollectionType(helperName,
                                                      "/soa");
      } else if (resolveStringTarget != nullptr &&
                 resolveStringTarget(receiverCandidate)) {
        methodResolved = "/string/" + helperName;
      } else if (resolveKeyValueTarget != nullptr &&
                 resolveKeyValueTarget(receiverCandidate, keyType, valueType)) {
        methodResolved = lateFallbackCanonicalKeyValueHelperPath(helperName);
      } else if (resolveExperimentalKeyValueTarget != nullptr &&
                 resolveExperimentalKeyValueTarget(receiverCandidate, keyType, valueType)) {
        methodResolved = lateFallbackCanonicalKeyValueHelperPath(helperName);
      } else {
        return false;
      }
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      ReturnKind builtinMethodKind = ReturnKind::Unknown;
      if (defMap_.find(methodResolved) == defMap_.end() &&
          resolveBuiltinCollectionMethodReturnKind(
              methodResolved,
              receiverCandidate,
              builtinCollectionDispatchResolvers,
              builtinMethodKind)) {
        if (skipResolvedResult) {
          return false;
        }
        resolvedKindOut = builtinMethodKind;
        return true;
      }
      auto methodIt = defMap_.find(methodResolved);
      if (methodIt == defMap_.end()) {
        return false;
      }
      if (skipResolvedResult) {
        return false;
      }
      if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
        resolvedKindOut = ReturnKind::Unknown;
        return true;
      }
      auto kindIt = returnKinds_.find(methodResolved);
      resolvedKindOut =
          kindIt != returnKinds_.end() ? kindIt->second : ReturnKind::Unknown;
      return true;
    };
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    const bool probePositionalReorderedReceiver =
        !hasNamedArgs && expr.args.size() > 1 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name &&
          !isCollectionAccessReceiverExpr(expr.args.front())));
    ReturnKind resolvedKind = ReturnKind::Unknown;
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          hasValuesNamedReceiver = true;
          if (tryResolveReceiverIndex(i, false, resolvedKind)) {
            return finish(resolvedKind);
          }
        }
      }
      if (!hasValuesNamedReceiver) {
        if (tryResolveReceiverIndex(0, false, resolvedKind)) {
          return finish(resolvedKind);
        }
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (tryResolveReceiverIndex(i, false, resolvedKind)) {
            return finish(resolvedKind);
          }
        }
      }
    } else {
      bool hasAlternativeCollectionReceiver = false;
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (isCollectionAccessReceiverExpr(expr.args[i])) {
            hasAlternativeCollectionReceiver = true;
            break;
          }
        }
      }
      if (tryResolveReceiverIndex(0, hasAlternativeCollectionReceiver, resolvedKind)) {
        return finish(resolvedKind);
      }
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (tryResolveReceiverIndex(i, false, resolvedKind)) {
            return finish(resolvedKind);
          }
        }
      }
    }
  }

  if (resolvedIt != defMap_.end() &&
      inferCollectionDispatchSetup
          .shouldDeferResolvedNamespacedCollectionHelperReturn) {
    if (!ensureDefinitionReturnKindReady(*resolvedIt->second)) {
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(
              expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
        return finish(builtinAccessKind);
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(
              expr,
              inferCollectionDispatchSetup
                  .builtinCollectionCountCapacityDispatchContext,
              builtinCollectionKind)) {
        return finish(builtinCollectionKind);
      }
      return finish(ReturnKind::Unknown);
    }
    ReturnKind builtinAccessKind = ReturnKind::Unknown;
    if (resolveBuiltinCollectionAccessCallReturnKind(
            expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
      return finish(builtinAccessKind);
    }
    ReturnKind builtinCollectionKind = ReturnKind::Unknown;
    if (resolveBuiltinCollectionCountCapacityReturnKind(
            expr,
            inferCollectionDispatchSetup
                .builtinCollectionCountCapacityDispatchContext,
            builtinCollectionKind)) {
      return finish(builtinCollectionKind);
    }
    auto kindIt = returnKinds_.find(context.resolved);
    if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
      return finish(kindIt->second);
    }
    return finish(ReturnKind::Unknown);
  }

  if (inferCollectionDispatchSetup.hasPreferredBuiltinAccessKind) {
    return finish(inferCollectionDispatchSetup.preferredBuiltinAccessKind);
  }

  std::string builtinAccessName;
  if (resolvedIt == defMap_.end() &&
      inferCollectionDispatchSetup.hasBuiltinAccessSpelling &&
      (!inferCollectionDispatchSetup.isStdNamespacedVectorAccessSpelling ||
       inferCollectionDispatchSetup.shouldAllowStdAccessCompatibilityFallback) &&
      (!inferCollectionDispatchSetup.isStdNamespacedKeyValueAccessSpelling ||
       inferCollectionDispatchSetup.hasStdNamespacedKeyValueAccessDefinition) &&
      expr.args.size() == 2) {
    builtinAccessName = inferCollectionDispatchSetup.builtinAccessName;
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands = bareKeyValueHelperOperandIndices(
        expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    std::string elemType;
    if (resolveStringTarget != nullptr && resolveStringTarget(receiverExpr)) {
      return finish(ReturnKind::Int);
    }
    std::string keyType;
    std::string valueType;
    if (resolveKeyValueTarget != nullptr &&
        resolveKeyValueTarget(receiverExpr, keyType, valueType)) {
      const ReturnKind kind =
          returnKindForTypeName(normalizeBindingTypeName(valueType));
      return finish(kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind);
    }
    if ((resolveArgsPackAccessTarget != nullptr &&
         resolveArgsPackAccessTarget(receiverExpr, elemType)) ||
        (resolveArrayTarget != nullptr &&
         resolveArrayTarget(receiverExpr, elemType))) {
      const ReturnKind kind =
          returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        return finish(kind);
      }
    }
    return finish(ReturnKind::Unknown);
  }

  if (resolvedIt == defMap_.end() && !expr.isMethodCall &&
      isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
      inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueContainsCall) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands = bareKeyValueHelperOperandIndices(
        expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    std::string keyType;
    std::string valueType;
    if (resolveKeyValueTarget != nullptr &&
        resolveKeyValueTarget(receiverExpr, keyType, valueType)) {
      return finish(ReturnKind::Bool);
    }
    return finish(ReturnKind::Unknown);
  }

  if (!expr.isMethodCall && resolvedIt == defMap_.end() && expr.args.size() == 2 &&
      (isSimpleCallName(expr, "contains") || isSimpleCallName(expr, "tryAt") ||
       inferCollectionDispatchSetup.hasBuiltinAccessSpelling)) {
    builtinAccessName = inferCollectionDispatchSetup.builtinAccessName;
    if ((isSimpleCallName(expr, "contains") &&
         !inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueContainsCall) ||
        (isSimpleCallName(expr, "tryAt") &&
         !inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueTryAtCall) ||
        (isCanonicalKeyValueAccessHelperName(builtinAccessName) &&
         !inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueAccessCall)) {
      Expr rewrittenKeyValueHelperCall;
      if (tryRewriteBareKeyValueHelperCall(
              expr,
              isSimpleCallName(expr, "contains")
                  ? "contains"
                  : (isSimpleCallName(expr, "tryAt") ? "tryAt"
                                                     : builtinAccessName),
              builtinCollectionDispatchResolvers,
              rewrittenKeyValueHelperCall)) {
        return finish(
            inferExprReturnKind(rewrittenKeyValueHelperCall, params, locals));
      }
    }
    std::string keyType;
    std::string valueType;
    const size_t keyValueReceiverIndex =
        keyValueHelperReceiverIndex(expr, builtinCollectionDispatchResolvers);
    const Expr &receiverExpr = keyValueReceiverIndex < expr.args.size()
                                   ? expr.args[keyValueReceiverIndex]
                                   : expr.args.front();
    if (resolveKeyValueTarget != nullptr &&
        resolveKeyValueTarget(receiverExpr, keyType, valueType)) {
      std::string methodResolved;
      if (context.resolveMethodCallPath != nullptr &&
          context.resolveMethodCallPath(expr.name, methodResolved)) {
        if (isCanonicalKeyValueContainsHelperPath(methodResolved) &&
            !inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueContainsCall &&
            !inferCollectionDispatchSetup.isIndexedArgsPackKeyValueReceiverTarget(
                receiverExpr) &&
            !hasImportedDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved)) {
          return failInferLateFallbackDiagnostic(
              "unknown call target: " + methodResolved);
        }
        if (isCanonicalKeyValueTryAtHelperPath(methodResolved) &&
            !inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueTryAtCall &&
            !inferCollectionDispatchSetup.isIndexedArgsPackKeyValueReceiverTarget(
                receiverExpr) &&
            !hasImportedDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath("/tryAt") &&
            !hasDeclaredDefinitionPath("/tryAt")) {
          return failInferLateFallbackDiagnostic(
              "unknown call target: " + methodResolved);
        }
        const bool isCanonicalAccessPath =
            isCanonicalKeyValueAccessHelperPath(methodResolved);
        const std::string canonicalAccessPath =
            isCanonicalAccessPath
                ? methodResolved
                : lateFallbackCanonicalKeyValueHelperPath(builtinAccessName);
        if ((isCollectionPairImportAliasAccessHelperPath(methodResolved) ||
             isCanonicalAccessPath) &&
            !inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueAccessCall &&
            !inferCollectionDispatchSetup.isIndexedArgsPackKeyValueReceiverTarget(
                receiverExpr) &&
            !hasImportedDefinitionPath(canonicalAccessPath) &&
            !hasDeclaredDefinitionPath(canonicalAccessPath)) {
          return failInferLateFallbackDiagnostic(
              "unknown call target: " + canonicalAccessPath);
        }
        auto methodIt = defMap_.find(methodResolved);
        if (methodIt != defMap_.end()) {
          if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
            return finish(ReturnKind::Unknown);
          }
          auto kindIt = returnKinds_.find(methodResolved);
          if (kindIt != returnKinds_.end()) {
            return finish(kindIt->second);
          }
        }
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionMethodReturnKind(
                methodResolved,
                receiverExpr,
                builtinCollectionDispatchResolvers,
                builtinMethodKind)) {
          return finish(builtinMethodKind);
        }
      }
    }
  }

  std::string builtinName;
  if (getBuiltinGpuName(expr, builtinName)) {
    return finish(ReturnKind::Int);
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "try") && expr.args.size() == 1) {
    ResultTypeInfo argResult;
    if (resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) &&
        argResult.isResult && argResult.hasValue) {
      return finish(
          returnKindForTypeName(normalizeBindingTypeName(argResult.valueType)));
    }
    return finish(ReturnKind::Unknown);
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "buffer_load") &&
      expr.args.size() == 2) {
    std::string elemType;
    if (resolveBufferTarget != nullptr &&
        resolveBufferTarget(expr.args.front(), elemType)) {
      const ReturnKind kind = returnKindForTypeName(elemType);
      return finish(kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind);
    }
    return finish(ReturnKind::Unknown);
  }

  if (!expr.isMethodCall &&
      (isSimpleCallName(expr, "buffer") || isSimpleCallName(expr, "upload") ||
       isSimpleCallName(expr, "readback"))) {
    return finish(ReturnKind::Array);
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "contains") &&
      inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueContainsCall &&
      expr.args.size() == 2) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands = bareKeyValueHelperOperandIndices(
        expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    std::string keyType;
    std::string valueType;
    if (resolveKeyValueTarget != nullptr &&
        resolveKeyValueTarget(receiverExpr, keyType, valueType)) {
      return finish(ReturnKind::Bool);
    }
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") &&
      expr.args.size() == 2 &&
      (inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueTryAtCall ||
       inferCollectionDispatchSetup.isIndexedArgsPackKeyValueReceiverTarget(
           expr.args[keyValueHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)]))) {
    ResultTypeInfo argResult;
    if (resolveResultTypeForExpr(expr, params, locals, argResult) &&
        argResult.isResult) {
      return finish(ReturnKind::Array);
    }
  }

  if (!expr.isMethodCall &&
      inferCollectionDispatchSetup.hasBuiltinAccessSpelling &&
      expr.args.size() == 2 &&
      (inferCollectionDispatchSetup.shouldInferBuiltinBareKeyValueAccessCall ||
       inferCollectionDispatchSetup.isIndexedArgsPackKeyValueReceiverTarget(
           expr.args[keyValueHelperReceiverIndex(expr,
                                            builtinCollectionDispatchResolvers)]))) {
    builtinAccessName = inferCollectionDispatchSetup.builtinAccessName;
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands = bareKeyValueHelperOperandIndices(
        expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    std::string keyType;
    std::string valueType;
    if (resolveKeyValueTarget != nullptr &&
        resolveKeyValueTarget(receiverExpr, keyType, valueType)) {
      const ReturnKind kind =
          returnKindForTypeName(normalizeBindingTypeName(valueType));
      return finish(kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind);
    }
  }

  if (!expr.isMethodCall &&
      inferCollectionDispatchSetup.hasBuiltinAccessSpelling &&
      expr.args.size() == 2) {
    builtinAccessName = inferCollectionDispatchSetup.builtinAccessName;
    std::string elemType;
    std::string keyType;
    std::string valueType;
    const Expr &receiver =
        expr.args[keyValueHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)];
    const bool hasCollectionReceiver =
        (resolveArgsPackAccessTarget != nullptr &&
         resolveArgsPackAccessTarget(receiver, elemType)) ||
        (resolveVectorTarget != nullptr &&
         resolveVectorTarget(receiver, elemType)) ||
        (resolveArrayTarget != nullptr &&
         resolveArrayTarget(receiver, elemType)) ||
        (resolveStringTarget != nullptr && resolveStringTarget(receiver)) ||
        (resolveKeyValueTarget != nullptr &&
         resolveKeyValueTarget(receiver, keyType, valueType)) ||
        (resolveExperimentalKeyValueTarget != nullptr &&
         resolveExperimentalKeyValueTarget(receiver, keyType, valueType));
      if (!hasCollectionReceiver) {
        std::string methodResolved;
        if (context.resolveMethodCallPath != nullptr &&
            context.resolveMethodCallPath(builtinAccessName, methodResolved) &&
            !methodResolved.empty()) {
          return failInferLateFallbackDiagnostic(
              soaUnavailableMethodDiagnostic(methodResolved));
        }
        return finish(ReturnKind::Unknown);
      }
  }

  return ReturnKind::Unknown;
}

} // namespace primec::semantics

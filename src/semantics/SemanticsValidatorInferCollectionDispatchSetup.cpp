#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

namespace primec::semantics {

namespace {

bool isCanonicalMapAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

const StdlibSurfaceMetadata *dispatchSetupMapHelperSurfaceMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

bool resolveDispatchSetupMapHelperPath(std::string_view path,
                                       std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      dispatchSetupMapHelperSurfaceMetadata();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperResolvedPath(path, metadata->id,
                                                      helperNameOut);
}

bool resolveRootMapHelperAliasPath(std::string_view rawPath,
                                   std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      dispatchSetupMapHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view path = trimLeadingSlash(rawPath);
  bool matchesRootAlias = false;
  for (std::string_view alias : metadata->importAliasSpellings) {
    alias = trimLeadingSlash(alias);
    if (alias.find('/') != std::string_view::npos ||
        path.size() <= alias.size() || path.rfind(alias, 0) != 0 ||
        path[alias.size()] != '/') {
      continue;
    }
    matchesRootAlias = true;
    break;
  }
  if (!matchesRootAlias) {
    return false;
  }
  return resolveDispatchSetupMapHelperPath("/" + std::string(path),
                                           helperNameOut);
}

bool isMapAccessCompatibilityPath(const std::string &path) {
  std::string helperName;
  return resolveRootMapHelperAliasPath(path, helperName) &&
         isCanonicalMapAccessHelperName(helperName);
}

bool isStdNamespacedCanonicalMapAccessPath(const std::string &path) {
  std::string helperName;
  return resolveDispatchSetupMapHelperPath(path, helperName) &&
         isCanonicalMapAccessHelperName(helperName);
}

std::string canonicalMapHelperNamespace() {
  const StdlibSurfaceMetadata *metadata =
      dispatchSetupMapHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return "";
  }
  std::string namespacePath(metadata->canonicalPath);
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.erase(namespacePath.begin());
  }
  return namespacePath;
}

} // namespace

void SemanticsValidator::prepareInferCollectionDispatchSetup(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const std::function<bool(const std::string &, std::string &)> &resolveMethodCallPath,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackCountTarget,
    const std::function<bool(const std::string &, ReturnKind &)> &inferResolvedPathReturnKind,
    const BuiltinCollectionDispatchResolvers &builtinCollectionDispatchResolvers,
    InferCollectionDispatchSetup &setupOut) {
  setupOut = {};

  std::string namespacedCollection;
  std::string namespacedHelper;
  const bool isNamespacedCollectionHelperCall =
      getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
  const bool isNamespacedVectorHelperCall =
      isNamespacedCollectionHelperCall && namespacedCollection == "vector";
  const bool isNamespacedMapHelperCall =
      isNamespacedCollectionHelperCall && namespacedCollection == "map";
  const std::string resolvedCalleePath = resolveCalleePath(expr);
  auto hasImportedCanonicalVectorHelperDefinition =
      [&](std::string_view helperName) {
        const std::string canonicalPath =
            canonicalVectorCompatibilityHelperPath(helperName);
        return !canonicalPath.empty() && hasImportedDefinitionPath(canonicalPath);
      };
  const bool isStdNamespacedVectorCountCall =
      !expr.isMethodCall &&
      isStdNamespacedVectorCompatibilityHelperPath(resolvedCalleePath, "count");
  const bool hasStdNamespacedVectorCountDefinition =
      hasImportedCanonicalVectorHelperDefinition("count");
  const bool shouldBuiltinValidateStdNamespacedVectorCountCall =
      isStdNamespacedVectorCountCall && hasStdNamespacedVectorCountDefinition;
  const bool isNamespacedVectorCountCall =
      !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "count" &&
      isVectorBuiltinName(expr, "count") &&
      !isArrayNamespacedVectorCountCompatibilityCall(expr, builtinCollectionDispatchResolvers);
  const std::string directRemovedMapCompatibilityPath =
      !expr.isMethodCall
          ? directMapHelperCompatibilityPath(
                expr, params, locals, BuiltinCollectionDispatchResolverAdapters{})
          : std::string();
  const bool isMapNamespacedAccessCompatibilityCall =
      isMapAccessCompatibilityPath(directRemovedMapCompatibilityPath);
  auto getCanonicalMapAccessHelperNameForDispatch =
      [&](const Expr &candidate, std::string &helperNameOut) -> bool {
        helperNameOut.clear();
        if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
            candidate.args.size() != 2 || candidate.name.empty()) {
          return false;
        }
        if (getBuiltinArrayAccessName(candidate, helperNameOut)) {
          return true;
        }
        const std::string resolvedPath = resolveCalleePath(candidate);
        std::string resolvedMapHelperName;
        if (resolveDispatchSetupMapHelperPath(resolvedPath,
                                              resolvedMapHelperName) &&
            (resolvedMapHelperName == "at_ref" ||
             resolvedMapHelperName == "at_unsafe_ref")) {
          helperNameOut = resolvedMapHelperName;
          return true;
        }
        std::string namespacePrefix = candidate.namespacePrefix;
        if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
          namespacePrefix.erase(namespacePrefix.begin());
        }
        if (namespacePrefix == canonicalMapHelperNamespace() &&
            isCanonicalMapAccessHelperName(candidate.name)) {
          helperNameOut = candidate.name;
          return true;
        }
        if (candidate.name.find('/') == std::string::npos &&
            isCanonicalMapAccessHelperName(candidate.name)) {
          helperNameOut = candidate.name;
          return true;
        }
        return false;
      };
  std::string directMapAccessHelperName;
  setupOut.hasBuiltinAccessSpelling =
      getCanonicalMapAccessHelperNameForDispatch(expr,
                                                 directMapAccessHelperName);
  if (setupOut.hasBuiltinAccessSpelling) {
    setupOut.builtinAccessName = directMapAccessHelperName;
  }
  const bool isNamespacedVectorCapacityCall =
      !expr.isMethodCall && isNamespacedVectorHelperCall &&
      namespacedHelper == "capacity" &&
      isVectorBuiltinName(expr, "capacity");
  const bool callsStdNamespacedVectorCapacityHelper =
      !expr.isMethodCall &&
      isStdNamespacedVectorCompatibilityHelperPath(resolvedCalleePath, "capacity");
  const bool stdNamespacedVectorCapacityHelperAvailableForInfer =
      !callsStdNamespacedVectorCapacityHelper ||
      hasImportedCanonicalVectorHelperDefinition("capacity");
  const bool isInferBuiltinCapacityLike =
      !expr.isMethodCall && isVectorBuiltinName(expr, "capacity") &&
      stdNamespacedVectorCapacityHelperAvailableForInfer;
  const bool isInferBuiltinSingleArgCapacityLike =
      isInferBuiltinCapacityLike && expr.args.size() == 1;
  setupOut.isStdNamespacedVectorAccessSpelling =
      setupOut.hasBuiltinAccessSpelling && !expr.isMethodCall &&
      (isStdNamespacedVectorCompatibilityHelperPath(resolvedCalleePath, "at") ||
       isStdNamespacedVectorCompatibilityHelperPath(resolvedCalleePath, "at_unsafe"));
  const bool isStdlibVectorAccessWrapperDefinition =
      currentValidationState_.context.definitionPath.rfind("/std/collections/", 0) == 0 ||
      currentValidationState_.context.definitionPath.rfind("/std/image/", 0) == 0 ||
      currentValidationState_.context.definitionPath.rfind("/std/ui/", 0) == 0;
  const bool hasStdNamespacedVectorAccessDefinition =
      setupOut.isStdNamespacedVectorAccessSpelling &&
      (hasImportedDefinitionPath(resolvedCalleePath) ||
       hasDeclaredDefinitionPath(resolvedCalleePath) ||
       isStdlibVectorAccessWrapperDefinition);
  setupOut.isStdNamespacedMapAccessSpelling =
      setupOut.hasBuiltinAccessSpelling && !expr.isMethodCall &&
      isStdNamespacedCanonicalMapAccessPath(resolvedCalleePath);
  setupOut.hasStdNamespacedMapAccessDefinition =
      setupOut.isStdNamespacedMapAccessSpelling &&
      (hasImportedDefinitionPath(resolvedCalleePath) ||
       hasDeclaredDefinitionPath(resolvedCalleePath));
  const bool isResolvedMapAccessCall =
      !expr.isMethodCall &&
      isMapAccessCompatibilityPath(resolved) &&
      !isMapNamespacedAccessCompatibilityCall;
  setupOut.shouldAllowStdAccessCompatibilityFallback = false;
  if (setupOut.isStdNamespacedVectorAccessSpelling && expr.args.size() == 2) {
    size_t receiverIndex = 0;
    if (hasNamedArguments(expr.argNames)) {
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          receiverIndex = i;
          break;
        }
      }
    }
    if (receiverIndex < expr.args.size()) {
      const Expr &receiverExpr = expr.args[receiverIndex];
      std::string elemType;
      const bool isNonVectorCompatibilityReceiver =
          (builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget != nullptr &&
           builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget(receiverExpr, elemType)) ||
          (builtinCollectionDispatchResolvers.resolveArrayTarget != nullptr &&
           builtinCollectionDispatchResolvers.resolveArrayTarget(receiverExpr, elemType)) ||
          (builtinCollectionDispatchResolvers.resolveStringTarget != nullptr &&
           builtinCollectionDispatchResolvers.resolveStringTarget(receiverExpr));
      if (isNonVectorCompatibilityReceiver) {
        std::string ignoredElemType;
        const bool isDirectVectorReceiver =
            (builtinCollectionDispatchResolvers.resolveVectorTarget != nullptr &&
             builtinCollectionDispatchResolvers.resolveVectorTarget(receiverExpr, ignoredElemType)) ||
            (builtinCollectionDispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
             builtinCollectionDispatchResolvers.resolveExperimentalVectorValueTarget(receiverExpr, ignoredElemType));
        setupOut.shouldAllowStdAccessCompatibilityFallback =
            !isDirectVectorReceiver;
      }
    }
  }
  setupOut.isBuiltinAccess =
      setupOut.hasBuiltinAccessSpelling &&
      (!setupOut.isStdNamespacedVectorAccessSpelling ||
       hasStdNamespacedVectorAccessDefinition) &&
      !setupOut.isStdNamespacedMapAccessSpelling && !isResolvedMapAccessCall;
  const bool isNamespacedVectorAccessCall =
      !expr.isMethodCall && setupOut.isBuiltinAccess &&
      isNamespacedVectorHelperCall &&
      (namespacedHelper == "at" || namespacedHelper == "at_unsafe");
  const bool isNamespacedMapAccessCall =
      !expr.isMethodCall && setupOut.isBuiltinAccess && isNamespacedMapHelperCall &&
      isCanonicalMapAccessHelperName(namespacedHelper) &&
      !isMapNamespacedAccessCompatibilityCall && !hasDefinitionPath(resolved);
  setupOut.shouldInferBuiltinBareMapContainsCall =
      shouldBuiltinValidateCurrentMapWrapperHelper("contains");
  setupOut.shouldInferBuiltinBareMapTryAtCall =
      shouldBuiltinValidateCurrentMapWrapperHelper("tryAt");
  setupOut.shouldInferBuiltinBareMapAccessCall = true;
  setupOut.isIndexedArgsPackMapReceiverTarget =
      [&](const Expr &receiverExpr) -> bool {
    std::string elemType;
    std::string keyType;
    std::string valueType;
    return ((builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType(
                 receiverExpr, elemType) ||
             builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType(
                 receiverExpr, elemType) ||
             builtinCollectionDispatchResolvers
                 .resolveDereferencedIndexedArgsPackElementType(receiverExpr,
                                                                elemType)) &&
            extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
  };

  auto preferVectorStdlibHelperPathForDispatch =
      [&](const std::string &path) {
        return preferVectorStdlibHelperPath(path);
      };

  setupOut.builtinCollectionCountCapacityDispatchContext.isCountLike =
      isVectorBuiltinName(expr, "count") &&
      expr.args.size() == 1 &&
      !isArrayNamespacedVectorCountCompatibilityCall(
          expr, builtinCollectionDispatchResolvers) &&
      (!isStdNamespacedVectorCountCall ||
       shouldBuiltinValidateStdNamespacedVectorCountCall);
  setupOut.builtinCollectionCountCapacityDispatchContext.isCapacityLike =
      isInferBuiltinSingleArgCapacityLike;
  setupOut.builtinCollectionCountCapacityDispatchContext.resolveMethodCallPath =
      resolveMethodCallPath;
  setupOut.builtinCollectionCountCapacityDispatchContext
      .preferVectorStdlibHelperPath = preferVectorStdlibHelperPathForDispatch;
  setupOut.builtinCollectionCountCapacityDispatchContext.dispatchResolvers =
      &builtinCollectionDispatchResolvers;

  const bool hasResolvedDefinition = defMap_.find(resolved) != defMap_.end();
  const std::string normalizedCallName = [&]() {
    std::string normalized = expr.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized;
  }();
  const bool isStdNamespacedMapAccessPath =
      isStdNamespacedCanonicalMapAccessPath("/" + normalizedCallName);
  setupOut.shouldDeferNamespacedVectorAccessCall =
      isNamespacedVectorAccessCall &&
      (!hasResolvedDefinition || setupOut.isStdNamespacedVectorAccessSpelling);
  setupOut.shouldDeferNamespacedMapAccessCall =
      isNamespacedMapAccessCall &&
      (!hasResolvedDefinition || isStdNamespacedMapAccessPath);
  setupOut.hasPreferredBuiltinAccessKind =
      setupOut.hasBuiltinAccessSpelling &&
      resolveBuiltinCollectionAccessCallReturnKind(
          expr, builtinCollectionDispatchResolvers,
          setupOut.preferredBuiltinAccessKind);
  setupOut.shouldDeferResolvedNamespacedCollectionHelperReturn =
      isNamespacedVectorCountCall || isNamespacedVectorCapacityCall ||
      setupOut.shouldDeferNamespacedVectorAccessCall ||
      setupOut.shouldDeferNamespacedMapAccessCall;

  const bool isDirectBuiltinCountCapacityCountCall =
      !expr.isMethodCall &&
      isVectorBuiltinName(expr, "count") &&
      !expr.args.empty() &&
      !isArrayNamespacedVectorCountCompatibilityCall(
          expr, builtinCollectionDispatchResolvers) &&
      (!isStdNamespacedVectorCountCall ||
       shouldBuiltinValidateStdNamespacedVectorCountCall) &&
      (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCountCall);
  const bool inferBuiltinCapacityCallTargetsDirectReceiver =
      defMap_.find(resolved) == defMap_.end() ||
      isNamespacedVectorCapacityCall;
  const bool isDirectBuiltinSingleArgCountCapacityCapacityCall =
      isInferBuiltinSingleArgCapacityLike &&
      inferBuiltinCapacityCallTargetsDirectReceiver;
  setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountCall =
      isDirectBuiltinCountCapacityCountCall;
  setupOut.builtinCollectionDirectCountCapacityContext.countHelperName =
      setupOut.builtinCollectionCountCapacityDispatchContext.countHelperName;
  setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountSingleArg =
      isDirectBuiltinCountCapacityCountCall && expr.args.size() == 1;
  setupOut.builtinCollectionDirectCountCapacityContext.isDirectCapacityCall =
      isInferBuiltinCapacityLike &&
      !expr.args.empty() &&
      inferBuiltinCapacityCallTargetsDirectReceiver;
  setupOut.builtinCollectionDirectCountCapacityContext
      .isDirectCapacitySingleArg =
      isDirectBuiltinSingleArgCountCapacityCapacityCall;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveMethodCallPath =
      resolveMethodCallPath;
  setupOut.builtinCollectionDirectCountCapacityContext
      .preferVectorStdlibHelperPath = preferVectorStdlibHelperPathForDispatch;
  setupOut.builtinCollectionDirectCountCapacityContext
      .inferResolvedPathReturnKind = inferResolvedPathReturnKind;
  setupOut.builtinCollectionDirectCountCapacityContext
      .resolveArgsPackCountTarget = resolveArgsPackCountTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveVectorTarget =
      builtinCollectionDispatchResolvers.resolveVectorTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveArrayTarget =
      builtinCollectionDispatchResolvers.resolveArrayTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveStringTarget =
      builtinCollectionDispatchResolvers.resolveStringTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.dispatchResolvers =
      &builtinCollectionDispatchResolvers;
}

} // namespace primec::semantics

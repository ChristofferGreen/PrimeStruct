#include "SemanticsValidator.h"

namespace primec::semantics {

namespace {

bool isCanonicalMapAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

bool isMapAccessCompatibilityPath(const std::string &path) {
  return path == "/map/at" || path == "/map/at_ref" ||
         path == "/map/at_unsafe" || path == "/map/at_unsafe_ref";
}

bool isStdNamespacedCanonicalMapAccessPath(const std::string &path) {
  return path == "/std/collections/map/at" ||
         path == "/std/collections/map/at_ref" ||
         path == "/std/collections/map/at_unsafe" ||
         path == "/std/collections/map/at_unsafe_ref";
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
  const bool isStdNamespacedVectorCountCall =
      !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/count", 0) == 0;
  const bool hasStdNamespacedVectorCountDefinition =
      hasImportedDefinitionPath("/std/collections/vector/count");
  const bool shouldBuiltinValidateStdNamespacedVectorCountCall =
      isStdNamespacedVectorCountCall && hasStdNamespacedVectorCountDefinition;
  const bool isStdNamespacedMapCountCall =
      !expr.isMethodCall &&
      (resolveCalleePath(expr) == "/std/collections/map/count" ||
       resolveCalleePath(expr) == "/std/collections/map/count_ref");
  const bool isNamespacedVectorCountCall =
      !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "count" &&
      isVectorBuiltinName(expr, "count") &&
      !isArrayNamespacedVectorCountCompatibilityCall(expr, builtinCollectionDispatchResolvers);
  const std::string directRemovedMapCompatibilityPath =
      !expr.isMethodCall
          ? directMapHelperCompatibilityPath(
                expr, params, locals, BuiltinCollectionDispatchResolverAdapters{})
          : std::string();
  const bool isMapNamespacedCountCompatibilityCall =
      directRemovedMapCompatibilityPath == "/map/count";
  const bool isMapNamespacedAccessCompatibilityCall =
      isMapAccessCompatibilityPath(directRemovedMapCompatibilityPath);
  const bool isNamespacedMapCountCall =
      !expr.isMethodCall && isNamespacedMapHelperCall &&
      (namespacedHelper == "count" || namespacedHelper == "count_ref") &&
      !isMapNamespacedCountCompatibilityCall && !isStdNamespacedMapCountCall &&
      !hasDefinitionPath(resolved);
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
        if (resolvedPath == "/map/at_ref" ||
            resolvedPath == "/std/collections/map/at_ref") {
          helperNameOut = "at_ref";
          return true;
        }
        if (resolvedPath == "/map/at_unsafe_ref" ||
            resolvedPath == "/std/collections/map/at_unsafe_ref") {
          helperNameOut = "at_unsafe_ref";
          return true;
        }
        std::string namespacePrefix = candidate.namespacePrefix;
        if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
          namespacePrefix.erase(namespacePrefix.begin());
        }
        if ((namespacePrefix == "map" ||
             namespacePrefix == "std/collections/map") &&
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
  const bool prefersExplicitDirectMapAccessAliasDefinition =
      !expr.isMethodCall &&
      (((isNamespacedMapHelperCall &&
         isCanonicalMapAccessHelperName(namespacedHelper)) ||
        (((expr.namespacePrefix == "map") || (expr.namespacePrefix == "/map")) &&
         isCanonicalMapAccessHelperName(expr.name)))) &&
      hasDefinitionPath("/map/" +
                        (isCanonicalMapAccessHelperName(expr.name)
                             ? expr.name
                             : namespacedHelper));
  const BuiltinCollectionDispatchResolverAdapters mapCountDispatchResolverAdapters;
  const bool isUnnamespacedMapCountFallbackCall =
      !expr.isMethodCall && isUnnamespacedMapCountBuiltinFallbackCall(
                                expr, params, locals, mapCountDispatchResolverAdapters);
  const bool isResolvedMapCountCall =
      !expr.isMethodCall && resolved == "/map/count" &&
      !isMapNamespacedCountCompatibilityCall &&
      !isUnnamespacedMapCountFallbackCall;
  const bool isNamespacedVectorCapacityCall =
      !expr.isMethodCall && isNamespacedVectorHelperCall &&
      namespacedHelper == "capacity" &&
      isVectorBuiltinName(expr, "capacity");
  const bool callsStdNamespacedVectorCapacityHelper =
      !expr.isMethodCall &&
      resolveCalleePath(expr).rfind("/std/collections/vector/capacity", 0) == 0;
  const bool stdNamespacedVectorCapacityHelperAvailableForInfer =
      !callsStdNamespacedVectorCapacityHelper ||
      hasImportedDefinitionPath("/std/collections/vector/capacity");
  const bool isInferBuiltinCapacityLike =
      !expr.isMethodCall && isVectorBuiltinName(expr, "capacity") &&
      stdNamespacedVectorCapacityHelperAvailableForInfer;
  const bool isInferBuiltinSingleArgCapacityLike =
      isInferBuiltinCapacityLike && expr.args.size() == 1;
  setupOut.isStdNamespacedVectorAccessSpelling =
      setupOut.hasBuiltinAccessSpelling && !expr.isMethodCall &&
      resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
  const bool hasStdNamespacedVectorAccessDefinition =
      setupOut.isStdNamespacedVectorAccessSpelling &&
      hasImportedDefinitionPath(resolveCalleePath(expr));
  setupOut.isStdNamespacedMapAccessSpelling =
      setupOut.hasBuiltinAccessSpelling && !expr.isMethodCall &&
      isStdNamespacedCanonicalMapAccessPath(resolveCalleePath(expr));
  setupOut.hasStdNamespacedMapAccessDefinition =
      setupOut.isStdNamespacedMapAccessSpelling &&
      (hasImportedDefinitionPath(resolveCalleePath(expr)) ||
       hasDeclaredDefinitionPath(resolveCalleePath(expr)));
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
      !prefersExplicitDirectMapAccessAliasDefinition &&
      !isMapNamespacedAccessCompatibilityCall && !hasDefinitionPath(resolved);
  setupOut.shouldInferBuiltinBareMapContainsCall = true;
  setupOut.shouldInferBuiltinBareMapTryAtCall = true;
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

  const bool shouldInferBuiltinBareMapCountCall =
      shouldBuiltinValidateCurrentMapWrapperHelper("count");
  auto preferVectorStdlibHelperPathForDispatch =
      [&](const std::string &path) {
        return preferVectorStdlibHelperPath(path);
      };
  auto hasDeclaredDefinitionPathForDispatch =
      [&](const std::string &path) { return hasDeclaredDefinitionPath(path); };

  setupOut.builtinCollectionCountCapacityDispatchContext.isCountLike =
      (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
       isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall ||
       isResolvedMapCountCall) &&
      expr.args.size() == 1 &&
      !isArrayNamespacedVectorCountCompatibilityCall(
          expr, builtinCollectionDispatchResolvers) &&
      (!isStdNamespacedVectorCountCall ||
       shouldBuiltinValidateStdNamespacedVectorCountCall);
  setupOut.builtinCollectionCountCapacityDispatchContext.isCapacityLike =
      isInferBuiltinSingleArgCapacityLike;
  setupOut.builtinCollectionCountCapacityDispatchContext
      .isUnnamespacedMapCountFallbackCall = isUnnamespacedMapCountFallbackCall;
  setupOut.builtinCollectionCountCapacityDispatchContext
      .shouldInferBuiltinBareMapCountCall = shouldInferBuiltinBareMapCountCall;
  setupOut.builtinCollectionCountCapacityDispatchContext.resolveMethodCallPath =
      resolveMethodCallPath;
  setupOut.builtinCollectionCountCapacityDispatchContext
      .preferVectorStdlibHelperPath = preferVectorStdlibHelperPathForDispatch;
  setupOut.builtinCollectionCountCapacityDispatchContext
      .hasDeclaredDefinitionPath = hasDeclaredDefinitionPathForDispatch;
  setupOut.builtinCollectionCountCapacityDispatchContext.resolveMapTarget =
      builtinCollectionDispatchResolvers.resolveMapTarget;
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
      normalizedCallName.rfind("std/collections/map/", 0) == 0;
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
      isNamespacedVectorCountCall || isNamespacedMapCountCall ||
      isResolvedMapCountCall || isNamespacedVectorCapacityCall ||
      setupOut.shouldDeferNamespacedVectorAccessCall ||
      setupOut.shouldDeferNamespacedMapAccessCall;

  const bool isDirectBuiltinCountCapacityCountCall =
      !expr.isMethodCall &&
      (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
       isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall ||
       isResolvedMapCountCall) &&
      !expr.args.empty() &&
      !isArrayNamespacedVectorCountCompatibilityCall(
          expr, builtinCollectionDispatchResolvers) &&
      (!isStdNamespacedVectorCountCall ||
       shouldBuiltinValidateStdNamespacedVectorCountCall) &&
      ((defMap_.find(resolved) == defMap_.end() && !isStdNamespacedMapCountCall) ||
       isNamespacedVectorCountCall || isStdNamespacedMapCountCall ||
       isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall ||
       isResolvedMapCountCall);
  const bool inferBuiltinCapacityCallTargetsDirectReceiver =
      defMap_.find(resolved) == defMap_.end() ||
      isNamespacedVectorCapacityCall;
  const bool isDirectBuiltinSingleArgCountCapacityCapacityCall =
      isInferBuiltinSingleArgCapacityLike &&
      inferBuiltinCapacityCallTargetsDirectReceiver;
  setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountCall =
      isDirectBuiltinCountCapacityCountCall;
  setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountSingleArg =
      isDirectBuiltinCountCapacityCountCall && expr.args.size() == 1;
  setupOut.builtinCollectionDirectCountCapacityContext.isDirectCapacityCall =
      isInferBuiltinCapacityLike &&
      !expr.args.empty() &&
      inferBuiltinCapacityCallTargetsDirectReceiver;
  setupOut.builtinCollectionDirectCountCapacityContext
      .isDirectCapacitySingleArg =
      isDirectBuiltinSingleArgCountCapacityCapacityCall;
  setupOut.builtinCollectionDirectCountCapacityContext
      .shouldInferBuiltinBareMapCountCall = shouldInferBuiltinBareMapCountCall;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveMethodCallPath =
      resolveMethodCallPath;
  setupOut.builtinCollectionDirectCountCapacityContext
      .preferVectorStdlibHelperPath = preferVectorStdlibHelperPathForDispatch;
  setupOut.builtinCollectionDirectCountCapacityContext
      .hasDeclaredDefinitionPath = hasDeclaredDefinitionPathForDispatch;
  setupOut.builtinCollectionDirectCountCapacityContext
      .inferResolvedPathReturnKind = inferResolvedPathReturnKind;
  setupOut.builtinCollectionDirectCountCapacityContext.tryRewriteBareMapHelperCall =
      [&](const Expr &candidate, Expr &rewrittenOut) {
        return tryRewriteBareMapHelperCall(candidate, "count",
                                           builtinCollectionDispatchResolvers,
                                           rewrittenOut);
      };
  setupOut.builtinCollectionDirectCountCapacityContext
      .inferRewrittenExprReturnKind =
      [&](const Expr &rewrittenExpr) {
        return inferExprReturnKind(rewrittenExpr, params, locals);
      };
  setupOut.builtinCollectionDirectCountCapacityContext
      .resolveArgsPackCountTarget = resolveArgsPackCountTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveVectorTarget =
      builtinCollectionDispatchResolvers.resolveVectorTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveArrayTarget =
      builtinCollectionDispatchResolvers.resolveArrayTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveStringTarget =
      builtinCollectionDispatchResolvers.resolveStringTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.resolveMapTarget =
      builtinCollectionDispatchResolvers.resolveMapTarget;
  setupOut.builtinCollectionDirectCountCapacityContext.dispatchResolvers =
      &builtinCollectionDispatchResolvers;
}

} // namespace primec::semantics

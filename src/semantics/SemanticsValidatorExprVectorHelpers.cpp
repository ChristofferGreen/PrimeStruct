// soa-surface-audit: exempt
#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <functional>
#include <string>
#include <vector>

namespace primec::semantics {
namespace {

bool isStdNamespacedVectorCompatibilityHelperCallPath(
    const std::string &resolved,
    const std::string &namespacedCollection,
    const std::string &namespacedHelper) {
  return namespacedCollection == "vector" &&
         isStdNamespacedVectorCompatibilityHelperPath(resolved, namespacedHelper);
}

bool isStdNamespacedVectorCompatibilityDirectCall(
    const Expr &expr,
    const std::string &resolved,
    const std::string &namespacedCollection,
    const std::string &namespacedHelper) {
  return !expr.isMethodCall &&
         isStdNamespacedVectorCompatibilityHelperCallPath(
             resolved, namespacedCollection, namespacedHelper);
}

bool isStdNamespacedVectorCompatibilityMethodCallSite(
    const Expr &expr,
    const std::string &resolved,
    const std::string &namespacedCollection,
    const std::string &namespacedHelper) {
  return expr.isMethodCall &&
         isStdNamespacedVectorCompatibilityHelperCallPath(
             resolved, namespacedCollection, namespacedHelper);
}

bool isStdNamespacedVectorDirectCallReceiverCompatible(
    const std::string &receiverFamily,
    bool hasReceiverCompatibleVisibleDefinition,
    bool isCountCapacityNamedArgException) {
  return receiverFamily == "vector" ||
         receiverFamily == legacyExperimentalVectorCompatibilityFamilyName() ||
         hasReceiverCompatibleVisibleDefinition ||
         isCountCapacityNamedArgException;
}

bool isVectorHelperReceiverName(const Expr &candidate,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals,
                                const std::function<bool(const BindingInfo &)> &isCollectionTraitBinding);

bool shouldProbePositionalReorderedVectorHelperReceiver(
    const Expr &expr,
    bool isStdNamespacedVectorCompatibilityDirectCallSite,
    bool hasNamedArgs,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const BindingInfo &)> &isCollectionTraitBinding) {
  return !isStdNamespacedVectorCompatibilityDirectCallSite &&
         !hasNamedArgs && expr.args.size() > 1 &&
         (expr.args.front().kind == Expr::Kind::Literal ||
          expr.args.front().kind == Expr::Kind::BoolLiteral ||
          expr.args.front().kind == Expr::Kind::FloatLiteral ||
          expr.args.front().kind == Expr::Kind::StringLiteral ||
          (expr.args.front().kind == Expr::Kind::Name &&
           !isVectorHelperReceiverName(expr.args.front(), params, locals,
                                       isCollectionTraitBinding)));
}

bool isVectorHelperReceiverName(const Expr &candidate,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals,
                                const std::function<bool(const BindingInfo &)> &isCollectionTraitBinding) {
  if (candidate.kind != Expr::Kind::Name) {
    return false;
  }
  BindingInfo binding;
  for (const auto &param : params) {
    if (param.name == candidate.name) {
      binding = param.binding;
      break;
    }
  }
  if (binding.typeName.empty()) {
    auto it = locals.find(candidate.name);
    if (it != locals.end()) {
      binding = it->second;
    }
  }
  if (binding.typeName.empty()) {
    return false;
  }
  if (isCollectionTraitBinding(binding)) {
    return true;
  }
  const std::string typeName = normalizeBindingTypeName(binding.typeName);
  return typeName == "vector" || isInternalSoaCollectionTypeName(typeName) ||
         isLegacyExperimentalVectorCompatibilityTypePath(typeName) ||
         isLegacyExperimentalVectorCompatibilityTypePath("/" + typeName);
}

} // namespace

bool SemanticsValidator::isUnqualifiedCollectionBuiltinName(const Expr &candidate,
                                                            const char *helper) const {
  if (candidate.kind != Expr::Kind::Call || helper == nullptr ||
      candidate.name != helper) {
    return false;
  }
  if (candidate.name.find('/') != std::string::npos) {
    return false;
  }
  if (candidate.namespacePrefix.empty()) {
    return true;
  }
  const std::string resolved = resolveCalleePath(candidate);
  if (!candidate.isMethodCall && hasImportedDefinitionPath(resolved)) {
    return true;
  }
  return resolved.empty() || (!hasDefinitionPath(resolved) &&
                              !hasDeclaredDefinitionPath(resolved) &&
                              !hasImportedDefinitionPath(resolved));
}

bool SemanticsValidator::resolveVectorHelperMethodTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    const std::string &helperName,
    std::string &resolvedOut) {
  resolvedOut.clear();
  std::string normalizedHelperName = helperName;
  if (!normalizedHelperName.empty() && normalizedHelperName.front() == '/') {
    normalizedHelperName.erase(normalizedHelperName.begin());
  }
  if (const size_t specializationSuffix = normalizedHelperName.find("__t");
      specializationSuffix != std::string::npos) {
    normalizedHelperName.erase(specializationSuffix);
  }
  std::string canonicalVectorHelperName;
  if (resolveCanonicalVectorHelperNameFromResolvedPath(
          "/" + normalizedHelperName, canonicalVectorHelperName)) {
    normalizedHelperName = canonicalVectorHelperName;
  } else if (isUnrootedVectorHelperPath(normalizedHelperName)) {
    normalizedHelperName.erase(0, unrootedVectorHelperPrefix().size());
  } else if (std::string soaHelperName;
             splitSoaSurfaceHelperPath(normalizedHelperName,
                                       &soaHelperName,
                                       nullptr)) {
    normalizedHelperName = soaHelperName;
  } else if (const std::string keyValueHelperName =
                 metadataBackedKeyValueHelperMethodName(normalizedHelperName);
             keyValueHelperName != normalizedHelperName) {
    normalizedHelperName = keyValueHelperName;
  }
  auto explicitCallPath = [](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
        callExpr.name.empty()) {
      return {};
    }
    if (callExpr.name.front() == '/') {
      return callExpr.name;
    }
    std::string prefix = callExpr.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    if (prefix.empty()) {
      return "/" + callExpr.name;
    }
    return prefix + "/" + callExpr.name;
  };
  auto isRootKeyValueAliasPath = [](const std::string &path) {
    return path == "/map" ||
           path.rfind("/map__", 0) == 0;
  };
  auto isLocalRootKeyValueAliasCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    return isRootKeyValueAliasPath(resolveCalleePath(candidate)) ||
           isRootKeyValueAliasPath(explicitCallPath(candidate));
  };
  auto bindingHasCollectionTrait = [&](const BindingInfo &binding,
                                       const std::string &namespacePrefix,
                                       std::string_view traitName) {
    return typeHasCollectionCategoryTrait(binding.typeName, namespacePrefix,
                                          traitName);
  };
  auto resolveCollectionVectorReceiver = [&](const Expr &candidate,
                                               std::string &elemTypeOut) -> bool {
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractCollectionVectorElementType(*paramBinding, elemTypeOut);
      }
      auto localIt = locals.find(candidate.name);
      if (localIt != locals.end()) {
        return extractCollectionVectorElementType(localIt->second, elemTypeOut);
      }
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(candidate, params, locals, collectionTypePath) &&
          (collectionTypePath == "/vector" ||
           isInternalSoaCollectionTypePath(collectionTypePath) ||
           collectionTypePath == "/map")) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(candidate));
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDefinitionReturnBinding(*defIt->second, inferredBinding) &&
          extractCollectionVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      if (inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          extractCollectionVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
        inferredBinding.typeTemplateArg.clear();
      }
      if (extractCollectionVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    return false;
  };
  auto resolveExperimentalSoaVectorReceiver = [&](const Expr &candidate,
                                                  std::string &elemTypeOut) -> bool {
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractExperimentalSoaVectorElementType(*paramBinding, elemTypeOut);
      }
      auto localIt = locals.find(candidate.name);
      if (localIt != locals.end()) {
        return extractExperimentalSoaVectorElementType(localIt->second, elemTypeOut);
      }
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(candidate, params, locals, collectionTypePath) &&
          (collectionTypePath == "/vector" ||
           isInternalSoaCollectionTypePath(collectionTypePath) ||
           collectionTypePath == "/map")) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(candidate));
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDefinitionReturnBinding(*defIt->second, inferredBinding) &&
          extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      if (inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
        inferredBinding.typeTemplateArg.clear();
      }
      if (extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    return false;
  };
  auto resolveBorrowedSoaVectorReceiver = [&](const Expr &candidate,
                                              std::string &elemTypeOut) {
    auto bindingTypeText = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return resolveExperimentalBorrowedSoaTypeText(bindingTypeText(*paramBinding),
                                                      elemTypeOut);
      }
      if (auto localIt = locals.find(candidate.name); localIt != locals.end()) {
        return resolveExperimentalBorrowedSoaTypeText(
            bindingTypeText(localIt->second), elemTypeOut);
      }
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string resolvedCandidatePath = resolveCalleePath(candidate);
      if (const std::string concreteResolvedCandidatePath =
              resolveExprConcreteCallPath(params, locals, candidate, resolvedCandidatePath);
          !concreteResolvedCandidatePath.empty()) {
        resolvedCandidatePath = concreteResolvedCandidatePath;
      }
      auto defIt = defMap_.find(resolvedCandidatePath);
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDefinitionReturnBinding(*defIt->second, inferredBinding) &&
          resolveExperimentalBorrowedSoaTypeText(bindingTypeText(inferredBinding),
                                                 elemTypeOut)) {
        return true;
      }
      if (inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          resolveExperimentalBorrowedSoaTypeText(bindingTypeText(inferredBinding),
                                                 elemTypeOut)) {
        return true;
      }
    }
    std::string inferredTypeText;
    return inferQueryExprTypeText(candidate, params, locals, inferredTypeText) &&
           !inferredTypeText.empty() &&
           resolveExperimentalBorrowedSoaTypeText(inferredTypeText, elemTypeOut);
  };
  auto preferredBorrowedSoaAccessHelperTarget = [&](std::string_view helperName) {
    if (helperName == "count") {
      helperName = "count_ref";
    } else if (helperName == "get") {
      helperName = "get_ref";
    } else if (helperName == "ref") {
      helperName = "ref_ref";
    } else if (helperName == "to_aos") {
      helperName = "to_aos_ref";
    }
    return preferredSoaHelperTargetForCollectionType(
        helperName, internalSoaCollectionTypePath(true));
  };
  auto tryResolveVectorReceiverSamePathSoaHelper =
      [&](std::string_view helperName) -> bool {
    if (!isSupportedCompatibilitySoaHelperName(helperName)) {
      return false;
    }
    if (!usesSamePathSoaHelperTargetForCollectionType(helperName, "/vector")) {
      return false;
    }
    resolvedOut = preferredSoaHelperTargetForCollectionType(helperName,
                                                            "/vector");
    return true;
  };
  auto tryResolvePublishedVectorAccessHelper =
      [&](std::string_view helperName) -> bool {
    if (!(helperName == "at" || helperName == "at_unsafe")) {
      return false;
    }
    const std::string preferredHelperTarget =
        preferredBareVectorHelperTarget(helperName);
    if (!hasDeclaredDefinitionPath(preferredHelperTarget) &&
        !hasImportedDefinitionPath(preferredHelperTarget)) {
      return false;
    }
    resolvedOut = preferredHelperTarget;
    return true;
  };
  std::string experimentalElemType;
  if (resolveCollectionVectorReceiver(receiver, experimentalElemType)) {
    if (tryResolvePublishedVectorAccessHelper(normalizedHelperName)) {
      return true;
    }
    resolvedOut = categoryCollectionVectorHelperTarget(normalizedHelperName,
                                                       experimentalElemType);
    return true;
  }
  std::string experimentalSoaElemType;
  if (resolveBorrowedSoaVectorReceiver(receiver, experimentalSoaElemType) &&
      (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
       normalizedHelperName == "get" || normalizedHelperName == "get_ref" ||
       normalizedHelperName == "ref" || normalizedHelperName == "ref_ref" ||
       normalizedHelperName == "to_aos" || normalizedHelperName == "to_aos_ref")) {
    resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedHelperName);
    return true;
  }
  if (resolveExperimentalSoaVectorReceiver(receiver, experimentalSoaElemType) &&
      (normalizedHelperName == "get" || normalizedHelperName == "ref" ||
       normalizedHelperName == "to_aos" ||
       normalizedHelperName == "push" || normalizedHelperName == "reserve")) {
    resolvedOut =
        preferredSoaHelperTargetForCollectionType(normalizedHelperName,
                                                  internalSoaCollectionTypePath(true));
    return true;
  }
  if (receiver.kind == Expr::Kind::Call &&
      (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
       normalizedHelperName == "capacity" ||
       normalizedHelperName == "at" || normalizedHelperName == "at_unsafe" ||
       normalizedHelperName == "insert" ||
       normalizedHelperName == "get" || normalizedHelperName == "ref" ||
       normalizedHelperName == "to_aos" ||
       normalizedHelperName == "push" || normalizedHelperName == "reserve")) {
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, collectionTypePath)) {
      if (collectionTypePath == "/vector") {
        if (tryResolveVectorReceiverSamePathSoaHelper(normalizedHelperName)) {
          return true;
        }
        resolvedOut = preferredBareVectorHelperTarget(normalizedHelperName);
        return true;
      }
      if (isInternalSoaCollectionTypePath(collectionTypePath) &&
          (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
           normalizedHelperName == "get" || normalizedHelperName == "ref" ||
           normalizedHelperName == "to_aos" ||
           normalizedHelperName == "push" || normalizedHelperName == "reserve")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(normalizedHelperName,
                                                      internalSoaCollectionTypePath(true));
        return true;
      }
      if (collectionTypePath == "/map" &&
          !isLocalRootKeyValueAliasCall(receiver) &&
          (normalizedHelperName == "contains" || normalizedHelperName == "contains_ref" ||
           normalizedHelperName == "tryAt" || normalizedHelperName == "tryAt_ref" ||
           normalizedHelperName == "at" || normalizedHelperName == "at_ref" ||
           normalizedHelperName == "at_unsafe" || normalizedHelperName == "at_unsafe_ref" ||
           normalizedHelperName == "insert" || normalizedHelperName == "insert_ref")) {
        resolvedOut = preferredBareKeyValueHelperTarget(normalizedHelperName);
        return true;
      }
    }
  }
  auto resolveReceiverTypePath = [&](const std::string &typeName,
                                     const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      return "/" + normalizeBindingTypeName(typeName);
    }
    std::string resolvedType = resolveTypePath(typeName, namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    return resolvedType;
  };
  auto tryResolveConcreteExperimentalSoaWrapperHelper =
      [&](const std::string &resolvedType) -> bool {
    const std::string normalizedResolvedType =
        std::string(trimLeadingSlash(resolvedType));
    const bool isExperimentalSoaWrapperType =
        isExperimentalSoaVectorTypePath(normalizedResolvedType) ||
        isExperimentalSoaVectorSpecializedTypePath(resolvedType);
    if (!isExperimentalSoaWrapperType ||
        !hasVisibleSoaHelperTargetForCurrentImports(helperName)) {
      return false;
    }
    resolvedOut =
        preferredSoaHelperTargetForCollectionType(
            helperName, internalSoaCollectionTypePath(true));
    return true;
  };
  if (receiver.kind == Expr::Kind::Name) {
    std::string typeName;
    std::string receiverTypeText;
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
      receiverTypeText = bindingTypeText(*paramBinding);
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
        receiverTypeText = bindingTypeText(it->second);
      }
    }
    if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
      return false;
    }
    const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
    const std::string collectionTypePath =
        inferMethodCollectionTypePathFromTypeText(receiverTypeText.empty() ? typeName
                                                                          : receiverTypeText);
    const std::string resolvedType = !collectionTypePath.empty()
                                         ? collectionTypePath
                                         : resolveReceiverTypePath(typeName,
                                                                   receiver.namespacePrefix);
    if (resolvedType.empty()) {
      return false;
    }
    const bool receiverHasKeyValueTrait =
        typeHasCollectionCategoryTrait(typeName, receiver.namespacePrefix,
                                       "KeyValue");
    if (resolvedType == "/vector" &&
        tryResolveVectorReceiverSamePathSoaHelper(normalizedHelperName)) {
      return true;
    }
    if ((resolvedType == "/map" || receiverHasKeyValueTrait) &&
        (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
         normalizedHelperName == "size" ||
         normalizedHelperName == "contains" || normalizedHelperName == "contains_ref" ||
         normalizedHelperName == "tryAt" || normalizedHelperName == "tryAt_ref" ||
         normalizedHelperName == "at" || normalizedHelperName == "at_ref" ||
         normalizedHelperName == "at_unsafe" || normalizedHelperName == "at_unsafe_ref" ||
         normalizedHelperName == "insert" || normalizedHelperName == "insert_ref")) {
      resolvedOut = preferredBareKeyValueHelperTarget(normalizedHelperName);
      return true;
    }
    if (resolvedType == "/vector" &&
        isVectorCompatibilityHelperName(normalizedHelperName)) {
      resolvedOut = preferredBareVectorHelperTarget(normalizedHelperName);
      return true;
    }
    if ((isInternalSoaCollectionTypePath(resolvedType) ||
         isInternalSoaCollectionTypeName(normalizedTypeName)) &&
        (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
         normalizedHelperName == "get" || normalizedHelperName == "get_ref" ||
         normalizedHelperName == "ref" || normalizedHelperName == "ref_ref" ||
         normalizedHelperName == "to_aos" || normalizedHelperName == "to_aos_ref" ||
         normalizedHelperName == "push" || normalizedHelperName == "reserve")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedHelperName,
                                                    internalSoaCollectionTypePath(true));
      return true;
    }
    if (tryResolveConcreteExperimentalSoaWrapperHelper(resolvedType)) {
      return true;
    }
    if (sumNames_.count(resolvedType) > 0) {
      return false;
    }
    resolvedOut = resolvedType + "/" + normalizedHelperName;
    return true;
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string resolvedType = resolveCalleePath(receiver);
    if (resolvedType.empty() || structNames_.count(resolvedType) == 0) {
      resolvedType = inferStructReturnPath(receiver, params, locals);
    }
    if (!resolvedType.empty()) {
      if (isInternalSoaCollectionTypePath(resolvedType) &&
          (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
           normalizedHelperName == "get" || normalizedHelperName == "get_ref" ||
           normalizedHelperName == "ref" || normalizedHelperName == "ref_ref" ||
           normalizedHelperName == "to_aos" || normalizedHelperName == "to_aos_ref" ||
           normalizedHelperName == "push" || normalizedHelperName == "reserve")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(normalizedHelperName,
                                                      internalSoaCollectionTypePath(true));
        return true;
      }
      if (isVectorCompatibilityHelperName(normalizedHelperName)) {
        BindingInfo receiverBinding;
        receiverBinding.typeName = resolvedType;
        std::string experimentalElemType;
        if (bindingHasCollectionTrait(receiverBinding, receiver.namespacePrefix,
                                      "Collection") &&
            extractCollectionVectorElementType(receiverBinding, experimentalElemType)) {
          if (tryResolvePublishedVectorAccessHelper(normalizedHelperName)) {
            return true;
          }
          resolvedOut = categoryCollectionVectorHelperTarget(normalizedHelperName,
                                                             experimentalElemType);
          return true;
        }
      }
      if (tryResolveConcreteExperimentalSoaWrapperHelper(resolvedType)) {
        return true;
      }
      resolvedOut = resolvedType + "/" + normalizedHelperName;
      return true;
    }
    const ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
    if (receiverKind == ReturnKind::Unknown || receiverKind == ReturnKind::Void ||
        receiverKind == ReturnKind::Array) {
      return false;
    }
    const std::string receiverType = typeNameForReturnKind(receiverKind);
    if (receiverType.empty()) {
      return false;
    }
    resolvedOut = "/" + receiverType + "/" + normalizedHelperName;
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveExprVectorHelperCall(const std::vector<ParameterInfo> &params,
                                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                                     const Expr &expr,
                                                     bool allowStatementOnlyMutator,
                                                     bool &hasResolutionOut,
                                                     std::string &resolvedPathOut,
                                                     size_t &receiverIndexOut) {
  auto failVectorHelperDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  hasResolutionOut = false;
  resolvedPathOut.clear();
  receiverIndexOut = 0;

  std::string vectorHelper;
  if (!getVectorMutatorHelperName(expr, vectorHelper)) {
    return true;
  }
  auto isCollectionTraitBinding = [&](const BindingInfo &binding) {
    return typeHasCollectionCategoryTrait(
        binding.typeName, expr.namespacePrefix, "Collection");
  };
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path) ||
        hasDeclaredDefinitionPath(path)) {
      return true;
    }
    if (!hasDefinitionFamilyPath(path)) {
      return false;
    }
    if (isStdNamespacedVectorCompatibilityHelperPath(path, vectorHelper)) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractCollectionVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };
  auto classifyReceiverFamily = [&](const Expr &candidate) -> std::string {
    BindingInfo receiverBinding;
    auto assignBindingFromTypeText = [&](const std::string &typeText) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        receiverBinding.typeName = normalizeBindingTypeName(base);
        receiverBinding.typeTemplateArg = argText;
      } else {
        receiverBinding.typeName = normalizedType;
        receiverBinding.typeTemplateArg.clear();
      }
    };
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        receiverBinding = *paramBinding;
      } else if (auto it = locals.find(candidate.name); it != locals.end()) {
        receiverBinding = it->second;
      }
    }
    if (receiverBinding.typeName.empty()) {
      if (!inferBindingTypeFromInitializer(candidate, params, locals, receiverBinding)) {
        std::string receiverTypeText;
        if (!inferQueryExprTypeText(candidate, params, locals, receiverTypeText)) {
          return {};
        }
        assignBindingFromTypeText(receiverTypeText);
      }
    }
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (typeHasCollectionCategoryTrait(receiverBinding.typeName,
                                       expr.namespacePrefix,
                                       "KeyValue")) {
      return "map";
    }
    if (typeHasCollectionCategoryTrait(receiverBinding.typeName,
                                       expr.namespacePrefix,
                                       "Collection")) {
      return legacyExperimentalVectorCompatibilityFamilyName();
    }
    if (extractCollectionVectorElementType(receiverBinding, elemType)) {
      return legacyExperimentalVectorCompatibilityFamilyName();
    }
    if (extractKeyValueCollectionTypes(receiverBinding, keyType, valueType)) {
      return "map";
    }
    const std::string normalizedBase = normalizeBindingTypeName(receiverBinding.typeName);
    if (normalizedBase == "vector" ||
        isInternalSoaCollectionTypeName(normalizedBase) ||
        normalizedBase == "array" || normalizedBase == "string") {
      return normalizedBase;
    }
    return {};
  };
  auto hasReceiverCompatibleVisibleDefinitionPath = [&](const std::string &path, const Expr &receiverExpr) {
    if (!hasVisibleDefinitionPath(path)) {
      return false;
    }
    auto paramsIt = paramsByDef_.find(path);
    if (paramsIt == paramsByDef_.end() || paramsIt->second.empty()) {
      return false;
    }
    const std::string receiverFamily = classifyReceiverFamily(receiverExpr);
    if (receiverFamily.empty()) {
      return false;
    }
    const BindingInfo &binding = paramsIt->second.front().binding;
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (extractCollectionVectorElementType(binding, elemType)) {
      return receiverFamily ==
             legacyExperimentalVectorCompatibilityFamilyName();
    }
    if (extractKeyValueCollectionTypes(binding, keyType, valueType)) {
      return receiverFamily == "map";
    }
    const std::string normalizedBase = normalizeBindingTypeName(binding.typeName);
    return normalizedBase == receiverFamily;
  };

  std::string resolved = resolveCalleePath(expr);
  std::string namespacedCollection;
  std::string namespacedHelper;
  getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
  auto diagnosticCallTarget = [&]() {
    if (!expr.name.empty() && expr.name.front() == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      std::string prefix = expr.namespacePrefix;
      if (!prefix.empty() && prefix.front() != '/') {
        prefix.insert(prefix.begin(), '/');
      }
      return prefix + "/" + expr.name;
    }
    return resolved;
  };
  const bool hasNamedArgs = hasNamedArguments(expr.argNames);
  const bool isStdNamespacedVectorCompatibilityDirectCallSite =
      isStdNamespacedVectorCompatibilityDirectCall(
          expr, resolved, namespacedCollection, namespacedHelper);
  if (expr.isMethodCall && !expr.args.empty() &&
      (vectorHelper == "count" || vectorHelper == "capacity")) {
    std::string methodTarget;
    const bool resolvedVectorMethod =
        resolveVectorHelperMethodTarget(params, locals, expr.args.front(),
                                        vectorHelper, methodTarget);
    const bool targetsStdNamespacedVectorCount =
        vectorHelper == "count" &&
        (isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") ||
         (resolvedVectorMethod &&
          isStdNamespacedVectorCompatibilityHelperPath(methodTarget, "count")));
    const bool targetsStdNamespacedVectorCapacity =
        vectorHelper == "capacity" &&
        (isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity") ||
         (resolvedVectorMethod &&
          isStdNamespacedVectorCompatibilityHelperPath(methodTarget, "capacity")));
    if ((targetsStdNamespacedVectorCount || targetsStdNamespacedVectorCapacity) &&
        expr.args.size() != 1) {
      if (hasNamedArgs) {
        return failVectorHelperDiagnostic(
            "named arguments not supported for builtin calls");
      }
      return failVectorHelperDiagnostic(
          "argument count mismatch for builtin " + vectorHelper);
    }
  }
  const bool isStdNamespacedVectorCountCapacityNamedArgException =
      isStdNamespacedVectorCompatibilityDirectCallSite &&
      (namespacedHelper == "count" || namespacedHelper == "capacity") &&
      hasNamedArgs;
  auto isBareVectorMutatorExpressionReceiver = [&](const Expr &receiverCandidate) {
    return !expr.isMethodCall &&
           expr.namespacePrefix.empty() &&
           expr.name == vectorHelper &&
           classifyReceiverFamily(receiverCandidate) == "vector";
  };
  auto stdlibWrapperVectorSugarReceiverIndex = [&]() -> size_t {
    if (expr.args.empty()) {
      return expr.args.size();
    }
    if (hasNamedArgs) {
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          return i;
        }
      }
    }
    return 0;
  };
  const bool isBarePublishedVectorMutator =
      !expr.isMethodCall &&
      expr.namespacePrefix.empty() &&
      expr.name == vectorHelper &&
      expr.name.find('/') == std::string::npos &&
      isPublishedVectorMutatorHelperName(vectorHelper);
  const bool isMethodPublishedVectorMutator =
      expr.isMethodCall && isPublishedVectorMutatorHelperName(vectorHelper);
  if ((isBarePublishedVectorMutator || isMethodPublishedVectorMutator) &&
      !expr.args.empty()) {
    const size_t receiverIndex = stdlibWrapperVectorSugarReceiverIndex();
    if (receiverIndex < expr.args.size() &&
        classifyReceiverFamily(expr.args[receiverIndex]) ==
            legacyExperimentalVectorCompatibilityFamilyName()) {
      return failVectorHelperDiagnostic(
          "unknown call target: " +
          (isMethodPublishedVectorMutator
               ? vectorHelper
               : canonicalVectorCompatibilityHelperPathOrFallback(vectorHelper)));
    }
  }
  if (isStdNamespacedVectorCompatibilityDirectCallSite &&
      !hasVisibleDefinitionPath(resolved) &&
      !isStdNamespacedVectorCountCapacityNamedArgException) {
    return failVectorHelperDiagnostic("unknown call target: " + diagnosticCallTarget());
  }
  if (isStdNamespacedVectorCompatibilityMethodCallSite(
          expr, resolved, namespacedCollection, namespacedHelper)) {
    return true;
  }
  if (isStdNamespacedVectorCompatibilityDirectCallSite &&
      expr.args.size() == 1 &&
      !expr.hasBodyArguments &&
      expr.bodyArguments.empty()) {
    const Expr &receiverExpr = expr.args.front();
    const std::string receiverFamily = classifyReceiverFamily(receiverExpr);
    if (!isStdNamespacedVectorDirectCallReceiverCompatible(
            receiverFamily,
            defMap_.count(resolved) > 0 &&
                hasReceiverCompatibleVisibleDefinitionPath(resolved, receiverExpr),
            isStdNamespacedVectorCountCapacityNamedArgException)) {
      return failVectorHelperDiagnostic("unknown call target: " + diagnosticCallTarget());
    }
  }

  bool resolvedVectorHelperDefinitionMissing =
      !hasVisibleDefinitionPath(resolved);
  size_t namedValuesReceiverIndex = expr.args.size();
  const bool hasNamedValuesReceiver = [&]() {
    if (!hasNamedArgs) {
      return false;
    }
    for (size_t i = 0; i < expr.args.size(); ++i) {
      if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
          *expr.argNames[i] == "values") {
        namedValuesReceiverIndex = i;
        return true;
      }
    }
    return false;
  }();
  size_t resolvedReceiverIndex = 0;
  bool resolvedReceiver = false;
  bool failedReceiverProbe = false;
  if ((!isStdNamespacedVectorCompatibilityDirectCallSite ||
       isStdNamespacedVectorCountCapacityNamedArgException) &&
      !expr.args.empty()) {
    auto tryResolveReceiverIndex = [&](size_t receiverIndex) {
      if (receiverIndex >= expr.args.size()) {
        return false;
      }
      const Expr &receiverCandidate = expr.args[receiverIndex];
      if (isBareVectorMutatorExpressionReceiver(receiverCandidate) && !allowStatementOnlyMutator) {
        failedReceiverProbe = true;
        return failVectorHelperDiagnostic(
            vectorHelper + " is only supported as a statement");
      }
      std::string methodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, vectorHelper, methodTarget)) {
        if (!expr.isMethodCall) {
          methodTarget = preferVectorStdlibHelperPath(methodTarget);
        }
      }
      if (!hasVisibleDefinitionPath(methodTarget)) {
        return false;
      }
      resolved = methodTarget;
      resolvedVectorHelperDefinitionMissing =
          !hasVisibleDefinitionPath(resolved);
      resolvedReceiverIndex = receiverIndex;
      return true;
    };

    auto markResolvedReceiver = [&](bool didResolve) {
      resolvedReceiver = didResolve;
      return didResolve;
    };
    auto tryResolveRemainingReceivers = [&](size_t startIndex) {
      for (size_t i = startIndex; i < expr.args.size(); ++i) {
        if (markResolvedReceiver(tryResolveReceiverIndex(i))) {
          return true;
        }
      }
      return false;
    };
    auto tryResolvePrimaryReceiver = [&]() {
      return markResolvedReceiver(tryResolveReceiverIndex(0));
    };
    auto tryResolveNamedValuesReceiver = [&]() {
      if (!hasNamedValuesReceiver) {
        return false;
      }
      return markResolvedReceiver(
          tryResolveReceiverIndex(namedValuesReceiverIndex));
    };
    auto tryResolveNamedFallbackReceivers = [&]() {
      if (hasNamedValuesReceiver || resolvedReceiver) {
        return false;
      }
      if (tryResolvePrimaryReceiver()) {
        return true;
      }
      return tryResolveRemainingReceivers(1);
    };
    auto tryResolveNamedInitialReceivers = [&]() {
      tryResolveNamedValuesReceiver();
      return tryResolveNamedFallbackReceivers();
    };
    auto tryResolvePositionalInitialReceiver = [&]() {
      return tryResolvePrimaryReceiver();
    };
    auto tryResolveInitialReceiverForCallShape = [&]() {
      if (hasNamedArgs) {
        return tryResolveNamedInitialReceivers();
      }
      return tryResolvePositionalInitialReceiver();
    };
    tryResolveInitialReceiverForCallShape();
    auto tryResolveReorderedReceiver = [&]() {
      if (resolvedReceiver) {
        return false;
      }
      if (!shouldProbePositionalReorderedVectorHelperReceiver(
              expr, isStdNamespacedVectorCompatibilityDirectCallSite,
              hasNamedArgs, params, locals, isCollectionTraitBinding)) {
        return false;
      }
      return tryResolveRemainingReceivers(1);
    };
    tryResolveReorderedReceiver();
  }
  if (failedReceiverProbe) {
    return false;
  }

  if (resolvedVectorHelperDefinitionMissing) {
    const bool requestsExplicitCollectionHelperNamespace =
        expr.namespacePrefix == "vector" ||
        expr.namespacePrefix == "/vector" ||
        isCompatibilitySoaSurfaceNamespace(expr.namespacePrefix) ||
        isRootedVectorHelperPath(expr.name) ||
        splitSoaSurfaceHelperPath(expr.name, nullptr, nullptr) ||
        isStdNamespacedVectorCompatibilityHelperPath(expr.name, vectorHelper) ||
        namespacedCollection == "vector" ||
        isCompatibilitySoaSurfaceNamespace(namespacedCollection);
    if (!isStdNamespacedVectorCompatibilityDirectCallSite) {
      // When the definition is missing, check whether the first argument is a
      // plain vector receiver. Soa_vector helpers are incompatible with plain
      // vector<T> targets, so reject them even in statement context rather than
      // silently accepting and letting IR lowering fail later.
      auto firstArgIsVectorFamily = [&]() -> bool {
        if (expr.args.empty()) {
          return false;
        }
        return classifyReceiverFamily(expr.args.front()) == "vector";
      };
      const bool isExplicitOldSoaMethodSurface =
          expr.isMethodCall &&
          (isCompatibilitySoaSurfaceNamespace(expr.namespacePrefix) ||
           splitSoaSurfaceHelperPath(expr.name, nullptr, nullptr));
      if (isExplicitOldSoaMethodSurface && !firstArgIsVectorFamily()) {
        return true;
      }
      if (!requestsExplicitCollectionHelperNamespace && !resolvedReceiver) {
        return true;
      }
      if (allowStatementOnlyMutator && !firstArgIsVectorFamily()) {
        hasResolutionOut = true;
        resolvedPathOut = resolved;
        return true;
      }
      return failVectorHelperDiagnostic(vectorHelper + " is only supported as a statement");
    }
    return true;
  }

  if (expr.isMethodCall &&
      (vectorHelper == "at" || vectorHelper == "at_unsafe")) {
    return true;
  }

  hasResolutionOut = true;
  resolvedPathOut = resolved;
  receiverIndexOut = resolvedReceiverIndex;
  return true;
}

} // namespace primec::semantics

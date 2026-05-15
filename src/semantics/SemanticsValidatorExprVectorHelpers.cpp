#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

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
                                const std::unordered_map<std::string, BindingInfo> &locals);

bool shouldProbePositionalReorderedVectorHelperReceiver(
    const Expr &expr,
    bool isStdNamespacedVectorCompatibilityDirectCallSite,
    bool hasNamedArgs,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  return !isStdNamespacedVectorCompatibilityDirectCallSite &&
         !hasNamedArgs && expr.args.size() > 1 &&
         (expr.args.front().kind == Expr::Kind::Literal ||
          expr.args.front().kind == Expr::Kind::BoolLiteral ||
          expr.args.front().kind == Expr::Kind::FloatLiteral ||
          expr.args.front().kind == Expr::Kind::StringLiteral ||
          (expr.args.front().kind == Expr::Kind::Name &&
           !isVectorHelperReceiverName(expr.args.front(), params, locals)));
}

bool isVectorHelperReceiverName(const Expr &candidate,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals) {
  if (candidate.kind != Expr::Kind::Name) {
    return false;
  }
  std::string typeName;
  for (const auto &param : params) {
    if (param.name == candidate.name) {
      typeName = normalizeBindingTypeName(param.binding.typeName);
      break;
    }
  }
  if (typeName.empty()) {
    auto it = locals.find(candidate.name);
    if (it != locals.end()) {
      typeName = normalizeBindingTypeName(it->second.typeName);
    }
  }
  return typeName == "vector" || isInternalSoaCollectionTypeName(typeName) ||
         typeName == "Vector" ||
         isLegacyExperimentalVectorCompatibilityTypePath(typeName) ||
         isLegacyExperimentalVectorCompatibilityTypePath("/" + typeName);
}

} // namespace

bool SemanticsValidator::isVectorBuiltinName(const Expr &candidate, const char *helper) const {
  const std::string helperName = helper == nullptr ? std::string() : std::string(helper);
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (isStdNamespacedVectorCompatibilityHelperPath(path, helperName)) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };
  if (isSimpleCallName(candidate, helper)) {
    return true;
  }
  std::string namespacedCollection;
  std::string namespacedHelper;
  if (!getNamespacedCollectionHelperName(candidate, namespacedCollection, namespacedHelper)) {
    return false;
  }
  if (!(namespacedCollection == "vector" && namespacedHelper == helper)) {
    const std::string resolvedPath = resolveCalleePath(candidate);
    if (resolvedPath.rfind("/std/", 0) != 0 || defMap_.count(resolvedPath) != 0) {
      return false;
    }
    const size_t lastSlash = resolvedPath.find_last_of('/');
    if (lastSlash == std::string::npos || resolvedPath.substr(lastSlash + 1) != helperName) {
      return false;
    }
    return isVectorCompatibilityHelperName(helperName);
  }
  const std::string resolvedCandidate = resolveCalleePath(candidate);
  if ((namespacedHelper != "count" && namespacedHelper != "capacity") ||
      !isStdNamespacedVectorCompatibilityHelperPath(resolvedCandidate, namespacedHelper)) {
    return true;
  }
  return hasVisibleDefinitionPath(resolvedCandidate);
}

bool SemanticsValidator::resolveVectorHelperMethodTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    const std::string &helperName,
    std::string &resolvedOut) {
  resolvedOut.clear();
  const std::string rawHelperName = helperName;
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
  } else if (normalizedHelperName.rfind("std/collections/map/", 0) == 0) {
    normalizedHelperName.erase(0, std::string("std/collections/map/").size());
  } else if (normalizedHelperName.rfind("map/", 0) == 0) {
    normalizedHelperName.erase(0, std::string("map/").size());
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
  auto isRootMapAliasPath = [](const std::string &path) {
    return path == "/map" ||
           path.rfind("/map__", 0) == 0;
  };
  auto isLocalRootMapAliasCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    return isRootMapAliasPath(resolveCalleePath(candidate)) ||
           isRootMapAliasPath(explicitCallPath(candidate));
  };
  auto resolveExperimentalVectorReceiver = [&](const Expr &candidate,
                                               std::string &elemTypeOut) -> bool {
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractExperimentalVectorElementType(*paramBinding, elemTypeOut);
      }
      auto localIt = locals.find(candidate.name);
      if (localIt != locals.end()) {
        return extractExperimentalVectorElementType(localIt->second, elemTypeOut);
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
          extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      if (inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
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
      if (extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
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
  if (resolveExperimentalVectorReceiver(receiver, experimentalElemType)) {
    if (tryResolvePublishedVectorAccessHelper(normalizedHelperName)) {
      return true;
    }
    resolvedOut = specializedExperimentalVectorHelperTarget(normalizedHelperName,
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
          !isLocalRootMapAliasCall(receiver) &&
          (normalizedHelperName == "contains" || normalizedHelperName == "contains_ref" ||
           normalizedHelperName == "tryAt" || normalizedHelperName == "tryAt_ref" ||
           normalizedHelperName == "at" || normalizedHelperName == "at_ref" ||
           normalizedHelperName == "at_unsafe" || normalizedHelperName == "at_unsafe_ref" ||
           normalizedHelperName == "insert" || normalizedHelperName == "insert_ref")) {
        resolvedOut = preferredBareMapHelperTarget(normalizedHelperName);
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
    if (resolvedType == "/vector" &&
        tryResolveVectorReceiverSamePathSoaHelper(normalizedHelperName)) {
      return true;
    }
    if ((resolvedType == "/map" || isMapCollectionTypeName(normalizedTypeName)) &&
        (normalizedHelperName == "count" || normalizedHelperName == "count_ref" ||
         normalizedHelperName == "size" ||
         normalizedHelperName == "contains" || normalizedHelperName == "contains_ref" ||
         normalizedHelperName == "tryAt" || normalizedHelperName == "tryAt_ref" ||
         normalizedHelperName == "at" || normalizedHelperName == "at_ref" ||
         normalizedHelperName == "at_unsafe" || normalizedHelperName == "at_unsafe_ref" ||
         normalizedHelperName == "insert" || normalizedHelperName == "insert_ref")) {
      resolvedOut = preferredBareMapHelperTarget(normalizedHelperName);
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
        if (extractExperimentalVectorElementType(receiverBinding, experimentalElemType)) {
          if (tryResolvePublishedVectorAccessHelper(normalizedHelperName)) {
            return true;
          }
          resolvedOut = specializedExperimentalVectorHelperTarget(normalizedHelperName,
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
  if (!getVectorStatementHelperName(expr, vectorHelper)) {
    return true;
  }
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (isStdNamespacedVectorCompatibilityHelperPath(path, vectorHelper)) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
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
    if (extractExperimentalVectorElementType(receiverBinding, elemType)) {
      return legacyExperimentalVectorCompatibilityFamilyName();
    }
    if (extractMapKeyValueTypes(receiverBinding, keyType, valueType)) {
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
    if (extractExperimentalVectorElementType(binding, elemType)) {
      return receiverFamily ==
             legacyExperimentalVectorCompatibilityFamilyName();
    }
    if (extractMapKeyValueTypes(binding, keyType, valueType)) {
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
      const bool isBareMutatorExpressionReceiver =
          isBareVectorMutatorExpressionReceiver(receiverCandidate);
      std::string methodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, vectorHelper, methodTarget)) {
        if (!expr.isMethodCall) {
          methodTarget = preferVectorStdlibHelperPath(methodTarget);
        }
      }
      if (!hasVisibleDefinitionPath(methodTarget)) {
        return false;
      }
      if (isBareMutatorExpressionReceiver) {
        failedReceiverProbe = true;
        return failVectorHelperDiagnostic(
            vectorHelper + " is only supported as a statement");
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
              hasNamedArgs, params, locals)) {
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
      const bool isExplicitOldSoaMethodSurface =
          expr.isMethodCall &&
          (isCompatibilitySoaSurfaceNamespace(expr.namespacePrefix) ||
           splitSoaSurfaceHelperPath(expr.name, nullptr, nullptr));
      if (isExplicitOldSoaMethodSurface) {
        return true;
      }
      if (!requestsExplicitCollectionHelperNamespace && !resolvedReceiver) {
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

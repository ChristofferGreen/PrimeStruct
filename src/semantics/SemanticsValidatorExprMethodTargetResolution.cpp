// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorMethodTargetResolutionDetail.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
using namespace method_target_detail;

bool SemanticsValidator::isStaticHelperDefinition(const Definition &def) const {
  for (const auto &transform : def.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::hasDeclaredDefinitionPath(const std::string &path) const {
  std::string canonicalPath = path;
  const size_t generatedSuffix = canonicalPath.find("__");
  if (generatedSuffix != std::string::npos) {
    canonicalPath.erase(generatedSuffix);
  }
  const std::string templatedPrefix = canonicalPath + "<";
  const std::string specializedPrefix = canonicalPath + "__";
  for (const auto &def : program_.definitions) {
    if (def.fullPath == canonicalPath ||
        def.fullPath.rfind(templatedPrefix, 0) == 0 ||
        def.fullPath.rfind(specializedPrefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveExplicitOrCanonicalCollectionMethodTarget(
    const std::string &path,
    const std::string &explicitRemovedMethodPath,
    const std::string &normalizedMethodName,
    const Expr &receiver,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string &resolvedOut,
    bool &isBuiltinOut) {
  auto isValueSurfaceAccessMethodName = [](std::string_view helperName) {
    return helperName == "at" || helperName == "at_unsafe";
  };
  const std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTargetFn =
      [this, &params, &locals](const Expr &target, std::string &elemType) -> bool {
    return this->resolveArgsPackAccessTarget(target, elemType, params, locals);
  };
  auto shouldPreserveBuiltinCompatibilityForExplicitRemovedMethod = [&]() {
    if (explicitRemovedMethodPath.empty()) {
      return false;
    }
    const bool isExplicitArrayCompatibilityPath =
        explicitRemovedMethodPath.rfind("/array/", 0) == 0;
    std::string ignoredElemType;
    const bool isCanonicalStdVectorPath =
        isCanonicalVectorCompatibilityPath(explicitRemovedMethodPath);
    if (normalizedMethodName == "count") {
      if (isExplicitArrayCompatibilityPath) {
        return false;
      }
      if (isCanonicalStdVectorPath) {
        return resolveVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn);
      }
      return resolveArgsPackCountTarget(receiver, ignoredElemType, params, locals) ||
             resolveVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn) ||
             resolveSoaVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn) ||
             resolveArrayTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn) ||
             resolveStringTarget(receiver, params, locals, resolveArgsPackAccessTargetFn);
    }
    if (normalizedMethodName == "count_ref") {
      if (isExplicitArrayCompatibilityPath || isCanonicalStdVectorPath) {
        return false;
      }
      return resolveSoaVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn) ||
             resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTargetFn);
    }
    if (normalizedMethodName == "capacity") {
      if (isExplicitArrayCompatibilityPath) {
        return false;
      }
      if (isCanonicalStdVectorPath) {
        return resolveVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn) ||
               resolveSoaVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn);
      }
      return resolveVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn) ||
             resolveSoaVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn);
    }
    if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
      const bool isVectorReceiver =
          resolveVectorTarget(receiver, ignoredElemType, params, locals, resolveArgsPackAccessTargetFn);
      if (isVectorReceiver) {
        return false;
      }
      if (isCanonicalStdVectorPath) {
        return false;
      }
      return resolveArgsPackAccessTarget(receiver, ignoredElemType, params, locals);
    }
    return false;
  };
  if (!explicitRemovedMethodPath.empty() &&
      isRootedVectorHelperPath(explicitRemovedMethodPath) &&
      hasDeclaredDefinitionPath(explicitRemovedMethodPath)) {
    resolvedOut = explicitRemovedMethodPath;
    isBuiltinOut = false;
    return true;
  }
  if (!explicitRemovedMethodPath.empty() &&
      path.rfind("/string/", 0) == 0 &&
      isValueSurfaceAccessMethodName(normalizedMethodName)) {
    resolvedOut = explicitRemovedMethodPath;
    isBuiltinOut = false;
    return true;
  }
  if (!explicitRemovedMethodPath.empty() &&
      path.rfind("/array/", 0) == 0) {
    std::string ignoredElemType;
    const bool isArgsPackArrayBuiltin =
        (normalizedMethodName == "count" &&
         resolveArgsPackCountTarget(receiver, ignoredElemType, params, locals)) ||
        (isValueSurfaceAccessMethodName(normalizedMethodName) &&
         resolveArgsPackAccessTarget(receiver, ignoredElemType, params, locals));
    if (isArgsPackArrayBuiltin) {
      resolvedOut = path;
      isBuiltinOut = true;
      return true;
    }
  }
  if (!explicitRemovedMethodPath.empty() && path.rfind("/string/", 0) != 0) {
    if (shouldPreserveBuiltinCompatibilityForExplicitRemovedMethod()) {
      resolvedOut = explicitRemovedMethodPath;
      isBuiltinOut = true;
      return true;
    }
    resolvedOut = explicitRemovedMethodPath;
    isBuiltinOut = false;
    return true;
  }
  resolvedOut = preferVectorStdlibHelperPath(path);
  if (resolvedOut.rfind("/array/", 0) == 0 &&
      defMap_.count(resolvedOut) == 0 &&
      !hasDeclaredDefinitionPath(resolvedOut)) {
    isBuiltinOut = true;
    return true;
  }
  const std::string resolvedSoaRefCanonical =
      canonicalizeLegacySoaRefHelperPath(resolvedOut);
  auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  auto isCanonicalSoaHelperPath = [](const std::string &candidate,
                                     std::string_view helperName) {
    return isCanonicalStdlibSoaHelperPath(candidate, helperName);
  };
  const std::string resolvedSoaCountCanonical =
      canonicalizeSoaHelperPath(resolvedOut);
  const std::string resolvedSoaGetCanonical =
      canonicalizeLegacySoaGetHelperPath(resolvedOut);
  const bool matchesSoaToAosHelperPath =
      isCanonicalStdlibSoaHelperPath(resolvedOut, "to_aos");
  const bool matchesBorrowedSoaToAosHelperPath =
      isCanonicalStdlibSoaHelperPath(resolvedOut, "to_aos_ref");
  const bool matchesBuiltinSoaCollectionHelper =
      isCanonicalSoaHelperPath(resolvedSoaCountCanonical, "count") ||
      isCanonicalSoaHelperPath(resolvedSoaCountCanonical, "count_ref") ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical, "get") ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical,
                                       "get_ref") ||
      matchesSoaToAosHelperPath ||
      matchesBorrowedSoaToAosHelperPath ||
      isCanonicalSoaRefLikeHelperPath(resolvedSoaRefCanonical);
  const bool hasImportedBuiltinSoaCollectionHelper =
      hasImportedDefinitionPath(resolvedOut) ||
      (resolvedSoaCountCanonical != resolvedOut &&
       hasImportedDefinitionPath(resolvedSoaCountCanonical)) ||
      (resolvedSoaGetCanonical != resolvedOut &&
       hasImportedDefinitionPath(resolvedSoaGetCanonical)) ||
      (resolvedSoaRefCanonical != resolvedOut &&
       hasImportedDefinitionPath(resolvedSoaRefCanonical));
  const bool hasLocalBuiltinSoaCollectionHelperDefinition =
      defMap_.count(resolvedOut) != 0 ||
      (resolvedSoaCountCanonical != resolvedOut &&
       defMap_.count(resolvedSoaCountCanonical) != 0) ||
      (resolvedSoaGetCanonical != resolvedOut &&
       defMap_.count(resolvedSoaGetCanonical) != 0) ||
      (resolvedSoaRefCanonical != resolvedOut &&
       defMap_.count(resolvedSoaRefCanonical) != 0);
  if (matchesBuiltinSoaCollectionHelper &&
      hasImportedBuiltinSoaCollectionHelper &&
      !hasLocalBuiltinSoaCollectionHelperDefinition) {
    isBuiltinOut = true;
    return true;
  }
  std::string resolvedCanonicalKeyValueHelperName;
  if (resolveCanonicalKeyValueHelperNameFromSpelling(
          resolvedOut, resolvedCanonicalKeyValueHelperName) &&
      isCanonicalMapBuiltinMethodHelper(resolvedCanonicalKeyValueHelperName) &&
      (this->shouldBuiltinValidateCurrentMapWrapperHelper(
           resolvedCanonicalKeyValueHelperName) ||
       hasImportedDefinitionPath(resolvedOut))) {
    isBuiltinOut = true;
    return true;
  }
  isBuiltinOut = !this->hasDefinitionFamilyPath(resolvedOut) &&
                 !hasImportedDefinitionPath(resolvedOut);
  return true;
}

bool SemanticsValidator::withPreservedError(const std::function<bool()> &fn) {
  const std::string previousError = error_;
  error_.clear();
  const bool ok = fn();
  error_.clear();
  error_ = previousError;
  return ok;
}

void SemanticsValidator::inferMethodTargetReceiverType(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    std::string &typeNameOut,
    std::string &typeTemplateArgOut) {
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeNameOut = paramBinding->typeName;
      typeTemplateArgOut = paramBinding->typeTemplateArg;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeNameOut = it->second.typeName;
        typeTemplateArgOut = it->second.typeTemplateArg;
      }
    }
  }
  if (typeNameOut.empty()) {
    if (receiver.kind == Expr::Kind::Call) {
      BindingInfo inferredReceiverBinding;
      if (withPreservedError([&]() {
            return inferBindingTypeFromInitializer(
                receiver, params, locals, inferredReceiverBinding);
          }) &&
          !inferredReceiverBinding.typeName.empty()) {
        typeNameOut = normalizeBindingTypeName(inferredReceiverBinding.typeName);
        typeTemplateArgOut = inferredReceiverBinding.typeTemplateArg;
      }
    }
  }
  if (typeNameOut.empty()) {
    if (receiver.kind == Expr::Kind::Call) {
      auto defIt = defMap_.find(resolveCalleePath(receiver));
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        BindingInfo inferredReturn;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
          typeNameOut = normalizeBindingTypeName(inferredReturn.typeName);
          typeTemplateArgOut = inferredReturn.typeTemplateArg;
        }
      }
    }
  }
  if (typeNameOut.empty()) {
    std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
    if (!inferredStruct.empty()) {
      std::string normalizedStruct = normalizeBindingTypeName(inferredStruct);
      if (!normalizedStruct.empty() && normalizedStruct.front() != '/') {
        normalizedStruct.insert(normalizedStruct.begin(), '/');
      }
      if (normalizedStruct == "/map" ||
          isSpecializedExperimentalKeyValueBackingTypeForMethodTargets(normalizedStruct)) {
        typeNameOut = "/map";
      } else {
        typeNameOut = inferredStruct;
      }
    }
  }
  if (typeNameOut.empty()) {
    ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
    std::string inferred;
    if (inferredKind == ReturnKind::Array) {
      inferred = inferStructReturnPath(receiver, params, locals);
      if (inferred.empty()) {
        inferred = typeNameForReturnKind(inferredKind);
      }
    } else {
      inferred = typeNameForReturnKind(inferredKind);
    }
    if (!inferred.empty()) {
      typeNameOut = inferred;
    }
  }
}

bool SemanticsValidator::resolveExplicitDirectCallReturnMethodTarget(
    const Expr &receiverExpr, const std::string &canonicalCollectionHelperName,
    const std::string &normalizedMethodName, const Expr &receiver,
    const std::string &explicitRemovedMethodPath,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string &resolvedOut, bool &isBuiltinOut) {
  if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
    return false;
  }
  auto defIt = defMap_.find(resolveCalleePath(receiverExpr));
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
    std::string normalizedReturnBaseType = normalizedReturnType;
    std::string normalizedReturnArgText;
    if (!normalizedReturnBaseType.empty() && normalizedReturnBaseType.front() == '/') {
      normalizedReturnBaseType.erase(normalizedReturnBaseType.begin());
    }
    std::string returnBase;
    if (splitTemplateTypeName(normalizedReturnBaseType, returnBase, normalizedReturnArgText) &&
        !returnBase.empty()) {
      normalizedReturnBaseType = normalizeBindingTypeName(returnBase);
    }
    const std::string normalizedReturnCollectionType =
        normalizeCollectionTypePath(normalizedReturnType);
    if (!normalizedReturnCollectionType.empty()) {
      if (isInternalSoaCollectionTypePath(normalizedReturnCollectionType) &&
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName)) {
        return resolveExplicitOrCanonicalCollectionMethodTarget(
            preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                      internalSoaCollectionTypePath(true)),
            explicitRemovedMethodPath, normalizedMethodName, receiver, params, locals, resolvedOut,
            isBuiltinOut);
      }
      return false;
    }
    if (normalizedReturnType.empty() || normalizedReturnBaseType == "auto") {
      return false;
    }
    if (normalizedReturnBaseType == "Reference" ||
        normalizedReturnBaseType == "Pointer") {
      const std::string normalizedReturnCollectionType =
          normalizeCollectionTypePath(normalizedReturnArgText);
      const bool isBorrowedSoaWrapperMethod =
          normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
          normalizedMethodName == "get" || normalizedMethodName == "get_ref" ||
          normalizedMethodName == "ref" || normalizedMethodName == "ref_ref" ||
          normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref";
      if (isInternalSoaCollectionTypePath(normalizedReturnCollectionType) &&
          isBorrowedSoaWrapperMethod) {
        return resolveExplicitOrCanonicalCollectionMethodTarget(
            preferredBorrowedSoaAccessHelperTarget(normalizedMethodName), explicitRemovedMethodPath,
            normalizedMethodName, receiver, params, locals, resolvedOut, isBuiltinOut);
      }
      const std::string normalizedPointeeType =
          normalizeBindingTypeName(normalizedReturnArgText);
      if (!normalizedPointeeType.empty() &&
          normalizeCollectionTypePath(normalizedPointeeType).empty()) {
        std::string normalizedPointeeBaseType = normalizedPointeeType;
        if (!normalizedPointeeBaseType.empty() &&
            normalizedPointeeBaseType.front() == '/') {
          normalizedPointeeBaseType.erase(normalizedPointeeBaseType.begin());
        }
        if (isPrimitiveBindingTypeName(normalizedPointeeBaseType)) {
          resolvedOut = "/" + normalizedPointeeBaseType + "/" + normalizedMethodName;
          return true;
        }
        std::string resolvedPointeeType =
            resolveMethodTargetStructTypePath(normalizedPointeeType,
                                              defIt->second->namespacePrefix);
        if (resolvedPointeeType.empty()) {
          resolvedPointeeType =
              resolveTypePath(normalizedPointeeType, defIt->second->namespacePrefix);
        }
        if (!resolvedPointeeType.empty()) {
          resolvedOut = resolvedPointeeType + "/" + normalizedMethodName;
          return true;
        }
      }
      resolvedOut = "/" + normalizedReturnBaseType + "/" + normalizedMethodName;
      return true;
    }
    if (isPrimitiveBindingTypeName(normalizedReturnBaseType)) {
      resolvedOut = "/" + normalizedReturnBaseType + "/" + normalizedMethodName;
      return true;
    }
    std::string resolvedReturnType =
        resolveMethodTargetStructTypePath(normalizedReturnType, defIt->second->namespacePrefix);
    if (resolvedReturnType.empty()) {
      resolvedReturnType = resolveTypePath(normalizedReturnType, defIt->second->namespacePrefix);
    }
    if (!resolvedReturnType.empty()) {
      if (tryRedirectConcreteExperimentalSoaMethodTarget(
              resolvedReturnType, canonicalCollectionHelperName, receiver,
              explicitRemovedMethodPath, normalizedMethodName, params, locals, resolvedOut,
              isBuiltinOut)) {
        return true;
      }
      resolvedOut = resolvedReturnType + "/" + normalizedMethodName;
      return true;
    }
    return false;
  }
  return false;
}

bool SemanticsValidator::isValueSurfaceAccessMethodName(std::string_view helperName) const {
  return helperName == "at" || helperName == "at_unsafe";
}

bool SemanticsValidator::isCanonicalKeyValueAccessMethodName(std::string_view helperName) const {
  return isValueSurfaceAccessMethodName(helperName) ||
         helperName == "size" ||
         helperName == "at_ref" || helperName == "at_unsafe_ref";
}

std::string SemanticsValidator::preferredBufferMethodTarget(const std::string &helperName) const {
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::GfxBufferHelpers);
  if (metadata == nullptr) {
    return std::string{};
  }
  const std::string canonical = stdlibSurfaceCanonicalHelperPath(
      StdlibSurfaceId::GfxBufferHelpers,
      helperName);
  const std::string canonicalFallback =
      canonical.empty() ? std::string(metadata->canonicalPath) + "/" + helperName
                        : canonical;
  if (hasDeclaredDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    return canonical;
  }
  if (!canonical.empty()) {
    for (const std::string_view spelling : metadata->compatibilitySpellings) {
      const std::string compatibility = std::string(spelling) + "/" + helperName;
      if (stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::GfxBufferHelpers,
                                           compatibility) != canonical) {
        continue;
      }
      if (hasDeclaredDefinitionPath(compatibility) || hasImportedDefinitionPath(compatibility)) {
        return compatibility;
      }
    }
  }
  return canonicalFallback;
}

bool SemanticsValidator::resolveCollectionMethodFromTypePath(
    const std::string &collectionTypePath, const std::string &normalizedMethodName,
    const Expr &receiver, const std::string &explicitVectorHelperPath,
    const std::string &explicitKeyValueHelperPath, const std::string &explicitRemovedMethodPath,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string &resolvedOut,
    bool &isBuiltinOut) {
  auto setCollectionMethodTargetLocal = [&](const std::string &path) -> bool {
    return resolveExplicitOrCanonicalCollectionMethodTarget(
        path, explicitRemovedMethodPath, normalizedMethodName, receiver, params, locals,
        resolvedOut, isBuiltinOut);
  };
  auto setPreferredKeyValueMethodTargetLocal = [&](const Expr &receiverExpr,
                                                    const std::string &helperName) -> bool {
    return setPreferredKeyValueMethodTarget(receiverExpr, helperName, explicitKeyValueHelperPath,
                                            receiver, explicitRemovedMethodPath,
                                            normalizedMethodName, params, locals,
                                            resolvedOut, isBuiltinOut);
  };
  if (normalizedMethodName == "count" || normalizedMethodName == "count_ref") {
    if (normalizedMethodName == "count" && collectionTypePath == "/array") {
      return setCollectionMethodTargetLocal("/array/count");
    }
    if (collectionTypePath == "/vector" &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      return setCollectionMethodTargetLocal(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector"));
    }
    if (normalizedMethodName == "count" && collectionTypePath == "/vector") {
      return setCollectionMethodTargetLocal(
          canonicalVectorCompatibilityHelperPathOrFallback("count"));
    }
    if (isInternalSoaCollectionTypePath(collectionTypePath)) {
      return setCollectionMethodTargetLocal(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    internalSoaCollectionTypePath(true)));
    }
    if (collectionTypePath == "/soa") {
      return setCollectionMethodTargetLocal(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa"));
    }
    if (normalizedMethodName == "count" && collectionTypePath == "/string") {
      return setCollectionMethodTargetLocal("/string/count");
    }
    if (collectionTypePath == "/map") {
      if (normalizedMethodName == "count") {
        if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
                receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
                resolvedOut, isBuiltinOut);
            explicitTarget.has_value()) {
          return *explicitTarget;
        }
      }
      return setPreferredKeyValueMethodTargetLocal(receiver, normalizedMethodName);
    }
    if (normalizedMethodName == "count" && collectionTypePath == "/Buffer") {
      return setCollectionMethodTargetLocal(preferredBufferMethodTarget("count"));
    }
  }
  if (normalizedMethodName == "capacity" && collectionTypePath == "/array" &&
      (hasDeclaredDefinitionPath("/array/capacity") ||
       hasImportedDefinitionPath("/array/capacity"))) {
    return setCollectionMethodTargetLocal("/array/capacity");
  }
  if (normalizedMethodName == "capacity" && collectionTypePath == "/vector") {
    return setCollectionMethodTargetLocal(
        canonicalVectorCompatibilityHelperPathOrFallback("capacity"));
  }
  if ((normalizedMethodName == "empty" || normalizedMethodName == "is_valid" ||
       normalizedMethodName == "readback" || normalizedMethodName == "load" ||
       normalizedMethodName == "store") &&
      collectionTypePath == "/Buffer") {
    return setCollectionMethodTargetLocal(preferredBufferMethodTarget(normalizedMethodName));
  }
  if (normalizedMethodName == "contains" && collectionTypePath == "/map") {
    return setPreferredKeyValueMethodTargetLocal(receiver, "contains");
  }
  if (normalizedMethodName == "tryAt" && collectionTypePath == "/map") {
    return setPreferredKeyValueMethodTargetLocal(receiver, "tryAt");
  }
  if (normalizedMethodName == "insert" && collectionTypePath == "/map") {
    return setPreferredKeyValueMethodTargetLocal(receiver, "insert");
  }
  if (normalizedMethodName == "size" && collectionTypePath == "/map") {
    return setPreferredKeyValueMethodTargetLocal(receiver, "size");
  }
  if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
    if (collectionTypePath == "/array") {
      return setCollectionMethodTargetLocal("/array/" + normalizedMethodName);
    }
    if (collectionTypePath == "/vector") {
      return setCollectionMethodTargetLocal(
          canonicalVectorCompatibilityHelperPathOrFallback(normalizedMethodName));
    }
    if (collectionTypePath == "/string") {
      return setCollectionMethodTargetLocal("/string/" + normalizedMethodName);
    }
  }
  if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
      collectionTypePath == "/map") {
    return setPreferredKeyValueMethodTargetLocal(receiver, normalizedMethodName);
  }
  if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       (collectionTypePath == "/vector" &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : "/vector"));
  }
  if ((normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       (collectionTypePath == "/vector" &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : "/vector"));
  }
  if ((normalizedMethodName == "push" || normalizedMethodName == "reserve") &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       (collectionTypePath == "/vector" &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                     "/vector")))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : "/vector"));
  }
  if (normalizedMethodName == "to_soa" && collectionTypePath == "/vector") {
    return setCollectionMethodTargetLocal("/to_soa");
  }
  if ((normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       collectionTypePath == "/vector")) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : "/vector"));
  }
  return false;
}

const char *SemanticsValidator::exprKindName(Expr::Kind kind) {
  switch (kind) {
  case Expr::Kind::Literal:
    return "Literal";
  case Expr::Kind::BoolLiteral:
    return "BoolLiteral";
  case Expr::Kind::FloatLiteral:
    return "FloatLiteral";
  case Expr::Kind::StringLiteral:
    return "StringLiteral";
  case Expr::Kind::Call:
    return "Call";
  case Expr::Kind::Name:
    return "Name";
  }
  return "Unknown";
}

bool SemanticsValidator::resolveMethodTargetGenericFallback(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &callNamespacePrefix, const Expr &receiver,
    const std::string &normalizedMethodName,
    const std::string &canonicalCollectionHelperName,
    const std::string &explicitVectorHelperPath,
    const std::string &explicitKeyValueHelperPath,
    const std::string &explicitRemovedMethodPath, bool traceFileErrorResult,
    const std::function<bool(std::string)> &failMethodTargetResolutionDiagnostic,
    const std::function<void(std::string_view, std::string_view, std::string_view)>
        &stampFileErrorResultFailure,
    std::string &resolvedOut, bool &isBuiltinOut) {
  auto normalizedTypeLeafName = [](std::string value) {
    value = normalizeBindingTypeName(value);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(value, base, argText) && !base.empty()) {
      value = base;
    }
    if (!value.empty() && value.front() == '/') {
      value.erase(value.begin());
    }
    const size_t slash = value.find_last_of('/');
    return slash == std::string::npos ? value : value.substr(slash + 1);
  };
  auto typeMatches = [&](std::string_view candidate, std::string_view expected) {
    return candidate == expected || normalizedTypeLeafName(std::string(candidate)) == expected;
  };
  auto setCollectionMethodTarget = [&](const std::string &path) -> bool {
    return resolveExplicitOrCanonicalCollectionMethodTarget(
        path, explicitRemovedMethodPath, normalizedMethodName, receiver, params, locals,
        resolvedOut, isBuiltinOut);
  };
  auto canonicalVectorHelperTarget = [](std::string_view helperName) {
    return canonicalVectorCompatibilityHelperPathOrFallback(helperName);
  };

  std::string typeName;
  std::string typeTemplateArg;
  inferMethodTargetReceiverType(params, locals, receiver, typeName, typeTemplateArg);
  if (typeMatches(typeName, "File") && isFileMethodName(normalizedMethodName)) {
    resolvedOut = preferredFileHelperTarget(normalizedMethodName,
                                           currentValidationState_.context.definitionPath);
    isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
    return true;
  }
  const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
  const std::string normalizedCollectionTypePath =
      normalizeCollectionTypePath(normalizedTypeName);
  std::string normalizedBaseTypeName = normalizedTypeName;
  if (!normalizedBaseTypeName.empty() && normalizedBaseTypeName.front() == '/') {
    normalizedBaseTypeName.erase(normalizedBaseTypeName.begin());
  }
  bool handledRetiredMaybeMutableHelper = false;
  if (bool ok = maybeFailRetiredMaybeMutableHelperForType(
          typeName, typeTemplateArg, normalizedMethodName, receiver,
          handledRetiredMaybeMutableHelper);
      handledRetiredMaybeMutableHelper) {
    return ok;
  }
  if (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
      normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
    BindingInfo receiverBinding;
    receiverBinding.typeName = typeName;
    receiverBinding.typeTemplateArg = typeTemplateArg;
    std::string experimentalElemType;
    if (extractCollectionVectorElementType(receiverBinding, experimentalElemType)) {
      if (normalizedMethodName == "count") {
        return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
      }
      if (normalizedMethodName == "capacity") {
        return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
      }
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
  }
  if (normalizedMethodName == "to_soa" &&
      normalizedCollectionTypePath == "/vector") {
    return setCollectionMethodTarget("/to_soa");
  }
  if ((normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") &&
      (isInternalSoaCollectionTypePath(normalizedCollectionTypePath) ||
       normalizedCollectionTypePath == "/vector")) {
    return setCollectionMethodTarget(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(normalizedCollectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : "/vector"));
  }
  if (isKeyValueSurfaceTypeName(normalizeBindingTypeName(typeName)) &&
      (normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
       normalizedMethodName == "size" ||
       normalizedMethodName == "contains" || normalizedMethodName == "contains_ref" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "tryAt_ref" ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       normalizedMethodName == "insert" || normalizedMethodName == "insert_ref")) {
    if (isRootedKeyValueHelperAliasPathForMethodTargets(explicitKeyValueHelperPath)) {
      return resolveExplicitRootKeyValueMethodPath(explicitKeyValueHelperPath, receiver,
                                                    resolvedOut, isBuiltinOut);
    }
    const std::string canonicalKeyValueHelper =
        canonicalKeyValueHelperPathLocal(normalizedMethodName);
    if (hasDeclaredDefinitionPath(canonicalKeyValueHelper) || hasImportedDefinitionPath(canonicalKeyValueHelper)) {
      resolvedOut = canonicalKeyValueHelper;
      isBuiltinOut = false;
      return true;
    }
    return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName,
                                            explicitKeyValueHelperPath, receiver,
                                            explicitRemovedMethodPath, normalizedMethodName,
                                            params, locals, resolvedOut, isBuiltinOut);
  }
  if (typeName == "Reference" &&
      (normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
       normalizedMethodName == "contains" || normalizedMethodName == "contains_ref" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "tryAt_ref" ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       normalizedMethodName == "insert" || normalizedMethodName == "insert_ref")) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalKeyValueTarget(receiver, keyType, valueType, params, locals)) {
      resolvedOut =
          this->preferredCanonicalExperimentalKeyValueHelperTarget(
              normalizedMethodName);
      isBuiltinOut = false;
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Name && receiver.name == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "eof" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    isBuiltinOut = resolvedOut == "/file_error/why";
    return !resolvedOut.empty();
  }
  if (typeMatches(typeName, "FileError") &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    isBuiltinOut = resolvedOut == "/file_error/why";
    return !resolvedOut.empty();
  }
  if (typeName == "string" &&
      (normalizedMethodName == "count" || normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
    return setCollectionMethodTarget("/string/" + normalizedMethodName);
  }
  if (typeName.empty()) {
    if (receiver.kind == Expr::Kind::Call && !validateExpr(params, locals, receiver)) {
      stampFileErrorResultFailure("validate-receiver-call", typeName, {});
      return false;
    }
    stampFileErrorResultFailure("unknown-target-empty-type", typeName, {});
    return failMethodTargetResolutionDiagnostic("unknown method target for " + normalizedMethodName);
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    const std::string normalizedPointeeType =
        normalizeBindingTypeName(typeTemplateArg);
    const std::string normalizedPointeeCollectionTypePath =
        normalizeCollectionTypePath(normalizedPointeeType);
    const bool isCanonicalBorrowedSoaWrapperMethod =
        canonicalCollectionHelperName == "count" ||
        canonicalCollectionHelperName == "count_ref" ||
        canonicalCollectionHelperName == "get" ||
        canonicalCollectionHelperName == "get_ref" ||
        canonicalCollectionHelperName == "ref" ||
        canonicalCollectionHelperName == "ref_ref" ||
        canonicalCollectionHelperName == "to_aos" ||
        canonicalCollectionHelperName == "to_aos_ref";
    if (normalizedPointeeCollectionTypePath == "/map" &&
        (normalizedMethodName == "count" ||
         normalizedMethodName == "contains" ||
         normalizedMethodName == "tryAt" ||
         normalizedMethodName == "at" ||
         normalizedMethodName == "at_unsafe" ||
         normalizedMethodName == "insert")) {
      if (isRootedKeyValueHelperAliasPathForMethodTargets(explicitKeyValueHelperPath)) {
        return resolveExplicitRootKeyValueMethodPath(explicitKeyValueHelperPath, receiver,
                                                      resolvedOut, isBuiltinOut);
      }
      // TODO-4691: count/contains/tryAt/at/insert now resolve via the
      // registry-backed borrowed-variant lookup instead of a hardcoded
      // literal chain. at_unsafe -> at_unsafe_ref stays hardcoded: TODO-4690
      // deliberately left that pair out of the registry table because a
      // pre-existing stdlib-map-ownership audit test forbids the
      // "at_unsafe_ref" literal appearing in StdlibSurfaceRegistry.cpp.
      std::string borrowedHelperName = normalizedMethodName;
      if (borrowedHelperName == "at_unsafe") {
        borrowedHelperName = "at_unsafe_ref";
      } else if (const std::string_view borrowedVariant = findBorrowedVariant(
                     StdlibSurfaceId::CollectionsManifestSurface2,
                     borrowedHelperName);
                 !borrowedVariant.empty()) {
        borrowedHelperName = std::string(borrowedVariant);
      }
      return setPreferredKeyValueMethodTarget(receiver, borrowedHelperName,
                                              explicitKeyValueHelperPath, receiver,
                                              explicitRemovedMethodPath, normalizedMethodName,
                                              params, locals, resolvedOut,
                                              isBuiltinOut);
    }
    if (isInternalSoaCollectionTypePath(normalizedPointeeCollectionTypePath) &&
        isCanonicalBorrowedSoaWrapperMethod) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaHelperTargetForCollectionMethod(
              canonicalCollectionHelperName));
    }
    if (!normalizedPointeeType.empty() &&
        normalizedPointeeCollectionTypePath.empty()) {
      std::string currentNamespace;
      if (!currentValidationState_.context.definitionPath.empty()) {
        const size_t slash =
            currentValidationState_.context.definitionPath.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
          currentNamespace =
              currentValidationState_.context.definitionPath.substr(0, slash);
        }
      }
      const std::string lookupNamespace =
          !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentNamespace;
      std::string normalizedPointeeBaseType = normalizedPointeeType;
      if (!normalizedPointeeBaseType.empty() &&
          normalizedPointeeBaseType.front() == '/') {
        normalizedPointeeBaseType.erase(normalizedPointeeBaseType.begin());
      }
      if (isPrimitiveBindingTypeName(normalizedPointeeBaseType)) {
        resolvedOut = "/" + normalizedPointeeBaseType + "/" + normalizedMethodName;
        return true;
      }
      std::string resolvedPointeeType =
          resolveMethodTargetStructTypePath(normalizedPointeeType, lookupNamespace);
      if (resolvedPointeeType.empty()) {
        resolvedPointeeType =
            resolveSumTypePath(normalizedPointeeType, lookupNamespace);
      }
      if (resolvedPointeeType.empty()) {
        resolvedPointeeType =
            resolveTypePath(normalizedPointeeType, lookupNamespace);
      }
      if (!resolvedPointeeType.empty()) {
        resolvedOut = resolvedPointeeType + "/" + normalizedMethodName;
        return true;
      }
    }
    stampFileErrorResultFailure("pointer-like-type", typeName, {});
    return failMethodTargetResolutionDiagnostic("unknown method target for " + normalizedMethodName);
  }
  // See the matching comment near the end of this function (before the
  // generic resolvedType + "/" + normalizedMethodName fallback) - primitive
  // receivers (e.g. string) return earlier via the branch just below, so
  // this same guard needs to run here too.
  if (normalizedMethodName == "capacity" &&
      normalizedCollectionTypePath != "/vector" &&
      isCanonicalVectorCompatibilityPath(explicitVectorHelperPath) &&
      !hasDeclaredDefinitionPath(explicitVectorHelperPath) &&
      !hasImportedDefinitionPath(explicitVectorHelperPath)) {
    return failMethodTargetResolutionDiagnostic("capacity requires vector target");
  }
  if (isPrimitiveBindingTypeName(normalizedBaseTypeName)) {
    resolvedOut = "/" + normalizedBaseTypeName + "/" + normalizedMethodName;
    return true;
  }
  if (normalizedBaseTypeName == "args") {
    return false;
  }
  std::string resolvedType = resolveMethodTargetStructTypePath(typeName, receiver.namespacePrefix);
  if (resolvedType.empty()) {
    resolvedType = resolveSumTypePath(
        typeName.empty() || typeTemplateArg.empty() ? typeName
                                                     : typeName + "<" + typeTemplateArg + ">",
        receiver.namespacePrefix);
  }
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  }
  if (resolveDeclaredSumMethodTarget(resolvedType, normalizedMethodName, resolvedOut,
                                     isBuiltinOut)) {
    return true;
  }
  if (bool ok = maybeFailRetiredMaybeMutableHelperForType(
          typeName, typeTemplateArg, normalizedMethodName, receiver,
          handledRetiredMaybeMutableHelper);
      handledRetiredMaybeMutableHelper) {
    return ok;
  }
  if (traceFileErrorResult && receiver.kind == Expr::Kind::Name &&
      receiver.name == "FileError" && resolvedType.empty()) {
    return failMethodTargetResolutionDiagnostic(
        "resolveMethodTarget FileError-result-fallthrough receiver.kind=" +
        std::string(exprKindName(receiver.kind)) +
        " receiver.name=" + receiver.name +
        " receiver.namespace=" + receiver.namespacePrefix +
        " call.namespace=" + callNamespacePrefix +
        " typeName=" + typeName);
  }
  if ((normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
       normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
      isLegacyExperimentalVectorCompatibilityTypePath(resolvedType)) {
    if (normalizedMethodName == "count") {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
    }
    if (normalizedMethodName == "capacity") {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
    }
    return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
  }
  if (normalizedCollectionTypePath == "/vector" &&
      normalizedMethodName != "count" &&
      normalizedMethodName != "capacity" &&
      normalizedMethodName != "at" &&
      normalizedMethodName != "at_unsafe") {
    const std::string legacyVectorMethodTarget =
        rootedVectorHelperPath(normalizedMethodName);
    if (hasDeclaredDefinitionPath(legacyVectorMethodTarget)) {
      resolvedOut = legacyVectorMethodTarget;
      return true;
    }
  }
  const bool isConcreteExperimentalSoaReceiver =
      isExperimentalSoaVectorSpecializedTypePath(resolvedType);
  const bool isCanonicalSoaWrapperMethod =
      isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
  if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
    return setCollectionMethodTarget(
        preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                  internalSoaCollectionTypePath(true)));
  }
  // A call that explicitly spells out the canonical
  // /std/collections/vector/capacity path on a non-vector receiver must be
  // rejected with the same "capacity requires vector target" diagnostic
  // used elsewhere for this method, even if a same-path definition happens
  // to exist for the receiver's own (non-vector) type - that canonical path
  // is reserved for vector receivers. Falling through to the generic
  // resolvedType + "/" + normalizedMethodName composition below would
  // instead silently substitute the receiver's own type, discarding the
  // explicit path the caller wrote and producing a misleading "unknown
  // method: /<receiver type>/capacity" diagnostic.
  if (normalizedMethodName == "capacity" &&
      normalizedCollectionTypePath != "/vector" &&
      isCanonicalVectorCompatibilityPath(explicitVectorHelperPath) &&
      !hasDeclaredDefinitionPath(explicitVectorHelperPath) &&
      !hasImportedDefinitionPath(explicitVectorHelperPath)) {
    return failMethodTargetResolutionDiagnostic("capacity requires vector target");
  }
  resolvedOut = resolvedType + "/" + normalizedMethodName;
  return true;
}

std::string SemanticsValidator::explicitRemovedCollectionMethodPathForCallNamespace(
    const std::string &rawMethodName, const std::string &callNamespacePrefix) const {
  std::string candidate = rawMethodName;
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  std::string normalizedPrefix = callNamespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  std::string_view helperName;
  bool isStdNamespacedVectorHelper = false;
  bool isStdNamespacedKeyValueHelper = false;
  std::string resolvedCanonicalKeyValueHelperName;
  std::string compatibilityCollection;
  if (normalizedPrefix == "array") {
    helperName = candidate;
    compatibilityCollection = "array";
  } else if (normalizedPrefix == "vector") {
    helperName = candidate;
    compatibilityCollection = "vector";
  } else if (isCanonicalVectorCompatibilityNamespace(normalizedPrefix)) {
    helperName = candidate;
    isStdNamespacedVectorHelper = true;
    compatibilityCollection = "vector";
  } else if (isKeyValueHelperImportAliasNamespaceForMethodTargets(
                 normalizedPrefix)) {
    helperName = candidate;
    compatibilityCollection = "map";
  } else if (normalizedPrefix == canonicalKeyValueHelperNamespaceLocal()) {
    helperName = candidate;
    isStdNamespacedKeyValueHelper = true;
    compatibilityCollection = "map";
  } else if (candidate.rfind("array/", 0) == 0) {
    helperName = std::string_view(candidate).substr(std::string_view("array/").size());
    compatibilityCollection = "array";
  } else if (isUnrootedVectorHelperPath(candidate)) {
    helperName = stripUnrootedVectorHelperPrefix(candidate);
    compatibilityCollection = "vector";
  } else if (isUnrootedCanonicalVectorCompatibilityPath(candidate)) {
    helperName = stripUnrootedCanonicalVectorCompatibilityPrefix(candidate);
    isStdNamespacedVectorHelper = true;
    compatibilityCollection = "vector";
  } else if (const std::string rootAliasHelperName =
                 metadataBackedKeyValueHelperRootAliasMethodName(candidate);
             !rootAliasHelperName.empty()) {
    helperName = rootAliasHelperName;
    compatibilityCollection = "map";
  } else if (resolveCanonicalKeyValueHelperNameFromSpelling(
                 candidate, resolvedCanonicalKeyValueHelperName)) {
    helperName = resolvedCanonicalKeyValueHelperName;
    isStdNamespacedKeyValueHelper = true;
    compatibilityCollection = "map";
  }
  if (helperName.empty()) {
    return "";
  }
  if (compatibilityCollection == "map") {
    if (isStdNamespacedKeyValueHelper) {
      return "";
    }
    if (!isRemovedKeyValueCompatibilityHelper(helperName)) {
      return "";
    }
    return rootedKeyValueHelperAliasPathForMethodTargets(helperName);
  }
  if (!isRemovedVectorCompatibilityHelper(helperName)) {
    return "";
  }
  if (isStdNamespacedVectorHelper) {
    return canonicalVectorCompatibilityHelperPathOrFallback(helperName);
  }
  if (compatibilityCollection == "array") {
    return "/array/" + std::string(helperName);
  }
  if (compatibilityCollection == "vector") {
    return rootedVectorHelperPath(helperName);
  }
  return "/" + candidate;
}

// Dispatch order (TODO-4724/TODO-5275): this function tries progressively
// more general receiver-typing strategies until one resolves the method
// call's target definition path, in this order:
//   1. explicit rooted/removed-compat-helper spellings computed up front
//      (explicitRemovedMethodPath/explicitVectorHelperPath/
//      explicitKeyValueHelperPath) - an explicit spelling wins outright so
//      it can't be silently reinterpreted by a later, more general rule;
//   2. collection-vector-metadata shortcuts (count/capacity-shaped builtins
//      that don't need a full type resolution);
//   3. explicit rooted key-value / vector-family receiver special cases
//      (args-pack element access, explicit canonical vector helper
//      receivers, indexed args-pack key-value targets, direct key-value
//      constructor receivers) - each narrower than a full type inference;
//   4. resolveMethodTargetGenericFallback - the last-resort path that does
//      a full receiver type inference (inferMethodTargetReceiverType) and
//      walks File / collection / struct / sum-type candidates in turn.
// Earlier steps are checked first because they're cheaper and more
// specific; the generic fallback is only reached once every explicit or
// shape-specific shortcut has been ruled out.
bool SemanticsValidator::resolveMethodTarget(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const std::string &callNamespacePrefix,
                                             const Expr &receiver,
                                             const std::string &methodName,
                                             std::string &resolvedOut,
                                             bool &isBuiltinOut) {
  isBuiltinOut = false;
  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathText(path);
    if (defMap_.count(pathText) > 0 || definitionFamilyPathIndex().count(pathText) > 0) {
      return true;
    }
    return anyDefinitionFamilyPathStartsWith(pathText + "<") ||
           anyDefinitionFamilyPathStartsWith(pathText + "__t") ||
           anyDefinitionFamilyPathStartsWith(pathText + "__ov");
  };
  auto startsWithRootVectorMethodPrefix = [&](std::string_view path) {
    return isUnrootedVectorHelperPath(path);
  };
  auto startsWithRootedVectorMethodPrefix = [&](std::string_view path) {
    return isRootedVectorHelperPath(path);
  };
  auto stripRootVectorMethodPrefix = [&](std::string_view path) {
    return stripUnrootedVectorHelperPrefix(path);
  };
  auto stripRootedVectorMethodPrefix = [&](std::string_view path) {
    return stripRootedVectorHelperPrefix(path);
  };
  const std::string explicitRemovedMethodPath =
      explicitRemovedCollectionMethodPathForCallNamespace(methodName, callNamespacePrefix);
  const std::string explicitVectorHelperPath =
      explicitVectorMethodPath(methodName, callNamespacePrefix);
  const std::string explicitKeyValueHelperPath =
      explicitKeyValueMethodPath(methodName, callNamespacePrefix);
  std::string normalizedMethodName = methodName;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (startsWithRootVectorMethodPrefix(normalizedMethodName)) {
    normalizedMethodName = std::string(stripRootVectorMethodPrefix(normalizedMethodName));
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (std::string soaHelperName;
             splitSoaSurfaceHelperPath(normalizedMethodName,
                                       &soaHelperName,
                                       nullptr)) {
    normalizedMethodName = soaHelperName;
  } else if (isUnrootedCanonicalVectorCompatibilityPath(normalizedMethodName)) {
    normalizedMethodName = std::string(
        stripUnrootedCanonicalVectorCompatibilityPrefix(normalizedMethodName));
  } else if (std::string canonicalKeyValueHelperName;
             resolveCanonicalKeyValueHelperNameFromSpelling(
                 normalizedMethodName, canonicalKeyValueHelperName)) {
    normalizedMethodName = canonicalKeyValueHelperName;
  }
  if (resolveCollectionVectorMetadataMethodTarget(normalizedMethodName, receiver, params, locals,
                                                  resolvedOut, isBuiltinOut)) {
    return true;
  }
  std::string canonicalCollectionHelperName = normalizedMethodName;
  if (const size_t specializationSuffix =
          canonicalCollectionHelperName.find("__t");
      specializationSuffix != std::string::npos) {
    canonicalCollectionHelperName.erase(specializationSuffix);
  }
  auto exprKindName = [](Expr::Kind kind) -> const char * {
    switch (kind) {
    case Expr::Kind::Literal:
      return "Literal";
    case Expr::Kind::BoolLiteral:
      return "BoolLiteral";
    case Expr::Kind::FloatLiteral:
      return "FloatLiteral";
    case Expr::Kind::StringLiteral:
      return "StringLiteral";
    case Expr::Kind::Call:
      return "Call";
    case Expr::Kind::Name:
      return "Name";
    }
    return "Unknown";
  };
  const bool traceFileErrorResult =
      normalizedMethodName == "result" &&
      (receiver.name == "FileError" ||
       receiver.name.find("FileError") != std::string::npos ||
       receiver.namespacePrefix.find("FileError") != std::string::npos ||
       callNamespacePrefix.find("FileError") != std::string::npos);
  std::optional<std::string> rememberedMethodTargetTraceFailure;
  auto failMethodTargetResolutionDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(receiver, std::move(message));
  };
  auto rememberMethodTargetTraceFailure = [&](std::string message) {
    if (!error_.empty() || rememberedMethodTargetTraceFailure.has_value()) {
      return;
    }
    rememberedMethodTargetTraceFailure = std::move(message);
  };
  auto stampFileErrorResultFailure = [&](std::string_view site,
                                         std::string_view typeName = {},
                                         std::string_view resolvedType = {}) {
    if (!traceFileErrorResult || !error_.empty() ||
        rememberedMethodTargetTraceFailure.has_value()) {
      return;
    }
    rememberMethodTargetTraceFailure(
        "resolveMethodTarget " + std::string(site) +
        " receiver.kind=" + exprKindName(receiver.kind) +
        " receiver.name=" + receiver.name +
        " receiver.namespace=" + receiver.namespacePrefix +
        " call.namespace=" + callNamespacePrefix +
        " typeName=" + std::string(typeName) +
        " resolvedType=" + std::string(resolvedType));
  };
  if (receiver.kind == Expr::Kind::Name && receiver.name == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "eof" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    isBuiltinOut = resolvedOut == "/file_error/why";
    if (resolvedOut.empty() && error_.empty()) {
      const std::string overload1 = "/std/file/FileError/result__ov1";
      std::string programMatch = "none";
      for (const auto &def : program_.definitions) {
        if (def.fullPath.find("FileError/result") != std::string::npos) {
          programMatch = def.fullPath;
          break;
        }
      }
      std::string paramsMatch = "none";
      for (const auto &[path, paramList] : paramsByDef_) {
        (void)paramList;
        if (path.find("FileError/result") != std::string::npos) {
          paramsMatch = path;
          break;
        }
      }
      return failMethodTargetResolutionDiagnostic(
          "preferredFileErrorHelperTarget empty for " + normalizedMethodName +
          " receiver=" + receiver.name +
          " has:/std/file/FileError/result=" +
          (hasDefinitionFamilyPath("/std/file/FileError/result") ? "yes" : "no") +
          " def:/std/file/FileError/result__ov1=" +
          (defMap_.count(overload1) > 0 ? "yes" : "no") +
          " params:/std/file/FileError/result__ov1=" +
          (paramsByDef_.count(overload1) > 0 ? "yes" : "no") +
          " programMatch=" + programMatch + " paramsMatch=" + paramsMatch +
          " has:/std/file/FileError/status=" +
          (hasDefinitionFamilyPath("/std/file/FileError/status") ? "yes"
                                                                 : "no"));
    }
    return !resolvedOut.empty();
  }

  auto isStaticBinding = [&](const Expr &bindingExpr) -> bool {
    for (const auto &transform : bindingExpr.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
                                   const std::string &namespacePrefix) -> std::string {
    return this->resolveMethodTargetStructTypePath(typeName, namespacePrefix);
  };
  if (normalizedMethodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/ok";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/error";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/why";
    isBuiltinOut = true;
    return true;
  }
  if ((normalizedMethodName == "map" || normalizedMethodName == "and_then" || normalizedMethodName == "map2") &&
      receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/" + normalizedMethodName;
    isBuiltinOut = true;
    return true;
  }
  if (receiver.kind == Expr::Kind::Name &&
      findParamBinding(params, receiver.name) == nullptr &&
      locals.find(receiver.name) == locals.end()) {
    std::string resolvedReceiverPath;
    const std::string rootReceiverPath = "/" + receiver.name;
    if (defMap_.find(rootReceiverPath) != defMap_.end()) {
      resolvedReceiverPath = rootReceiverPath;
    } else {
      auto importIt = importAliases_.find(receiver.name);
      if (importIt != importAliases_.end()) {
        resolvedReceiverPath = importIt->second;
      }
    }
    if (!resolvedReceiverPath.empty() &&
        (structNames_.count(resolvedReceiverPath) > 0 ||
         defMap_.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap_.end())) {
      resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
      return true;
    }
    const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      const bool isConcreteExperimentalSoaReceiver =
          isExperimentalSoaVectorSpecializedTypePath(resolvedType);
      const bool isCanonicalSoaWrapperMethod =
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
      if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
        resolvedOut = preferredSoaHelperTargetForCollectionType(
            canonicalCollectionHelperName,
            internalSoaCollectionTypePath(true));
        isBuiltinOut = defMap_.count(resolvedOut) == 0 &&
                       !hasImportedDefinitionPath(resolvedOut);
        return true;
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }

  auto resolvesBorrowedExperimentalSoaReceiver = [&](const Expr &candidate) {
    const std::string previousError = error_;
    error_.clear();
    std::string inferredTypeText;
    const bool inferred =
        inferQueryExprTypeText(candidate, params, locals, inferredTypeText);
    error_.clear();
    error_ = previousError;
    if (!inferred) {
      return false;
    }
    std::string ignoredElemType;
    return resolveExperimentalBorrowedSoaTypeText(inferredTypeText,
                                                 ignoredElemType);
  };
  auto preferredSoaToAosHelperTargetForReceiver = [&](const Expr &receiverExpr) {
    if (normalizedMethodName == "to_aos" &&
        resolvesBorrowedExperimentalSoaReceiver(receiverExpr)) {
      return preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
    }
    return preferredSoaHelperTargetForCollectionType(
        normalizedMethodName, internalSoaCollectionTypePath(true));
  };
  std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget =
      [&](const Expr &target, std::string &elemType) -> bool {
    return this->resolveArgsPackAccessTarget(target, elemType, params, locals);
  };
  auto resolveKeyValueValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
    return this->resolveMethodTargetKeyValueValueType(target, valueTypeOut, params, locals,
                                                       resolveArgsPackAccessTarget);
  };
  std::string elemType;
  auto setCollectionMethodTarget = [&](const std::string &path) -> bool {
    return resolveExplicitOrCanonicalCollectionMethodTarget(
        path, explicitRemovedMethodPath, normalizedMethodName, receiver, params, locals,
        resolvedOut, isBuiltinOut);
  };
  auto canonicalVectorHelperTarget = [](std::string_view helperName) {
    return canonicalVectorCompatibilityHelperPathOrFallback(helperName);
  };
  auto setPreferredKeyValueMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) {
    return this->setPreferredKeyValueMethodTarget(
        receiverExpr, helperName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
        normalizedMethodName, params, locals, resolvedOut, isBuiltinOut);
  };
  auto resolveExplicitRootKeyValueMethodPath = [&]() -> bool {
    if (!isRootedKeyValueHelperAliasPathForMethodTargets(explicitKeyValueHelperPath)) {
      return false;
    }
    if (hasDeclaredDefinitionPath(explicitKeyValueHelperPath)) {
      resolvedOut = explicitKeyValueHelperPath;
      isBuiltinOut = false;
      return true;
    }
    return failMethodTargetResolutionDiagnostic("unknown method: " +
                                                explicitKeyValueHelperPath);
  };
  if (!explicitKeyValueHelperPath.empty()) {
    const bool resolvedExplicitRootKeyValueMethod =
        resolveExplicitRootKeyValueMethodPath();
    if (resolvedExplicitRootKeyValueMethod || !error_.empty()) {
      return resolvedExplicitRootKeyValueMethod;
    }
  }
  if (normalizedMethodName == "count" &&
      this->resolveArgsPackCountTarget(receiver, elemType, params, locals)) {
    return setCollectionMethodTarget("/array/count");
  }
  if (isValueSurfaceAccessMethodName(normalizedMethodName) &&
      resolveArgsPackAccessTarget(receiver, elemType)) {
    return setCollectionMethodTarget("/array/" + normalizedMethodName);
  }
  const std::function<bool(const Expr &, std::string &)> resolveSoaVectorTargetFn =
      [&](const Expr &target, std::string &elemTypeOut) -> bool {
    return this->resolveSoaVectorTarget(target, elemTypeOut, params, locals,
                                        resolveArgsPackAccessTarget);
  };
  auto resolveDirectReceiver = [&](const Expr &directCandidate,
                                   std::string &directElemTypeOut) -> bool {
    return this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
        directCandidate, params, locals, resolveSoaVectorTargetFn,
        directElemTypeOut);
  };
  const std::string explicitRemovedVectorReceiverFamily =
      classifyExplicitVectorHelperReceiver(receiver, params, locals);
  if (startsWithRootedVectorMethodPrefix(explicitVectorHelperPath) &&
      (explicitRemovedVectorReceiverFamily == "string" ||
       explicitRemovedVectorReceiverFamily == "array" ||
       explicitRemovedVectorReceiverFamily == "map")) {
    const std::string helperName =
        std::string(stripRootedVectorMethodPrefix(explicitVectorHelperPath));
    if ((explicitRemovedVectorReceiverFamily == "string" ||
         explicitRemovedVectorReceiverFamily == "array") &&
        hasDeclaredDefinitionPath(explicitVectorHelperPath) &&
        explicitVectorCompatHelperFamilyHasCompatibleReceiver(
            explicitVectorHelperPath, explicitRemovedVectorReceiverFamily)) {
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
    return failMethodTargetResolutionDiagnostic(
        "unknown method: /" + explicitRemovedVectorReceiverFamily + "/" +
        helperName);
  }
  auto isDirectKeyValueConstructorReceiverCall = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
      return false;
    }
    return isResolvedPublishedKeyValueConstructorPath(resolveCalleePath(receiverExpr));
  };
  if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
       normalizedMethodName == "size" ||
       normalizedMethodName == "contains" || normalizedMethodName == "contains_ref" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "tryAt_ref" ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       normalizedMethodName == "insert" || normalizedMethodName == "insert_ref") &&
      setIndexedArgsPackKeyValueMethodTarget(
          receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
    return true;
  }
  auto setMethodTargetFromTypeText =
      [&](const std::string &typeText, const std::string &typeNamespace) -> bool {
    const std::string normalizedType =
        normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    if (normalizedType.empty()) {
      return false;
    }
    std::string normalizedBaseType = normalizedType;
    if (!normalizedBaseType.empty() && normalizedBaseType.front() == '/') {
      normalizedBaseType.erase(normalizedBaseType.begin());
    }
    if (normalizedType == "string" &&
        (normalizedMethodName == "count" || normalizedMethodName == "at" ||
         normalizedMethodName == "at_unsafe")) {
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      base = normalizeBindingTypeName(base);
      if (base == "vector" &&
          (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
           normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
        return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
      }
      if (base == "array" &&
          (normalizedMethodName == "count" || normalizedMethodName == "at" ||
           normalizedMethodName == "at_unsafe")) {
        return setCollectionMethodTarget("/array/" + normalizedMethodName);
      }
      const bool isCanonicalSoaWrapperMethod =
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
      if ((isInternalSoaCollectionTypeName(base) ||
           (base == "vector" &&
            usesSamePathSoaHelperTargetForCollectionType(canonicalCollectionHelperName, "/vector"))) &&
          isCanonicalSoaWrapperMethod) {
        return setCollectionMethodTarget(
            preferredSoaHelperTargetForCollectionType(
                canonicalCollectionHelperName,
                isInternalSoaCollectionTypeName(base)
                    ? internalSoaCollectionTypePath(true)
                    : "/vector"));
      }
      if (base == "Buffer" &&
          (normalizedMethodName == "count" || normalizedMethodName == "empty" ||
           normalizedMethodName == "is_valid" || normalizedMethodName == "readback" ||
           normalizedMethodName == "load" || normalizedMethodName == "store")) {
        return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
      }
      if (isKeyValueSurfaceTypeName(base) &&
          (normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
           normalizedMethodName == "contains" || normalizedMethodName == "contains_ref" ||
           normalizedMethodName == "tryAt" || normalizedMethodName == "tryAt_ref" ||
           isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
           normalizedMethodName == "insert" || normalizedMethodName == "insert_ref")) {
        return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
      }
    }
    if (isPrimitiveBindingTypeName(normalizedBaseType)) {
      resolvedOut = "/" + normalizedBaseType + "/" + normalizedMethodName;
      return true;
    }
    std::string resolvedType = resolveStructTypePath(normalizedType, typeNamespace);
    if (resolvedType.empty()) {
      resolvedType = resolveTypePath(normalizedType, typeNamespace);
    }
    if (resolvedType.empty()) {
      return false;
    }
    const bool isConcreteExperimentalSoaReceiver =
        isExperimentalSoaVectorSpecializedTypePath(resolvedType);
    const bool isCanonicalSoaWrapperMethod =
        isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
    if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                    internalSoaCollectionTypePath(true)));
    }
    resolvedOut = resolvedType + "/" + normalizedMethodName;
    return true;
  };

  if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
       normalizedMethodName == "size" ||
       normalizedMethodName == "contains" || normalizedMethodName == "contains_ref" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "tryAt_ref" ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       normalizedMethodName == "insert" || normalizedMethodName == "insert_ref") &&
      isDirectKeyValueConstructorReceiverCall(receiver)) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalKeyValueTarget(receiver, keyType, valueType, params, locals)) {
      return failMethodTargetResolutionDiagnostic(
          "unknown call target: " +
          this->preferredCanonicalExperimentalKeyValueHelperTarget(
              normalizedMethodName));
    }
    return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
  }
  auto explicitVectorReceiverFamily =
      classifyExplicitVectorHelperReceiver(receiver, params, locals);
  const bool isExplicitVectorFamilyReceiver =
      explicitVectorReceiverFamily == "vector" ||
      explicitVectorReceiverFamily ==
          legacyExperimentalVectorCompatibilityFamilyName() ||
      isInternalSoaCollectionTypeName(explicitVectorReceiverFamily);
  const std::string_view explicitRootedVectorHelperName =
      startsWithRootedVectorMethodPrefix(explicitVectorHelperPath)
          ? stripRootedVectorMethodPrefix(explicitVectorHelperPath)
          : std::string_view{};
  const bool isExplicitRootedVectorMethod =
      !explicitRootedVectorHelperName.empty() &&
      isRemovedVectorCompatibilityHelper(explicitRootedVectorHelperName);
  if (isExplicitRootedVectorMethod && isExplicitVectorFamilyReceiver) {
    if (hasReceiverCompatibleExplicitVectorHelperPath(
            explicitVectorHelperPath, receiver, params, locals)) {
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
    return failMethodTargetResolutionDiagnostic(
        "unknown method: " + explicitVectorHelperPath);
  }
  // receiver.isMethodCall here means "was this call written with dot-call
  // syntax" (see Ast.h), not "does this resolve to a method" - it does not
  // rule out a bare-call-syntax vector-compatibility-helper call below.
  if (receiver.isMethodCall && !explicitVectorHelperPath.empty() && !isExplicitVectorFamilyReceiver &&
      isVectorCompatibilityHelperName(normalizedMethodName)) {
    if (hasDeclaredDefinitionPath(explicitVectorHelperPath) ||
        hasImportedDefinitionPath(explicitVectorHelperPath)) {
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
    const std::string preferredExplicitVectorHelperPath =
        preferVectorStdlibHelperPath(explicitVectorHelperPath);
    if (normalizedMethodName == "capacity") {
      return failMethodTargetResolutionDiagnostic("capacity requires vector target");
    }
    return failMethodTargetResolutionDiagnostic(receiver.isMethodCall
                                                   ? "unknown method: " +
                                                         preferredExplicitVectorHelperPath
                                                   : "unknown call target: " +
                                                         preferredExplicitVectorHelperPath);
  }
  const bool usesBuiltinVectorMethodSemantics =
      normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
      normalizedMethodName == "at" || normalizedMethodName == "at_unsafe";
  if (!usesBuiltinVectorMethodSemantics &&
      preferExplicitCanonicalVectorHelperForReceiver(
          receiver, explicitVectorHelperPath, params, locals)) {
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (normalizedMethodName == "count" || normalizedMethodName == "count_ref" ||
      normalizedMethodName == "size") {
    if (normalizedMethodName == "count" &&
        this->resolveArgsPackCountTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget("/array/count");
    }
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    "/vector"));
    }
    if (normalizedMethodName == "count" &&
        this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
    }
    if (normalizedMethodName == "count" &&
        this->resolveCollectionVectorValueTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
    }
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa"));
    }
    if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaAccessHelperTarget(normalizedMethodName));
    }
    if (normalizedMethodName == "count" &&
        this->resolveArrayTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
              receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
              resolvedOut, isBuiltinOut);
          explicitTarget.has_value()) {
        return *explicitTarget;
      }
      return setCollectionMethodTarget("/array/count");
    }
    if (normalizedMethodName == "count" &&
        this->resolveStringTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
              receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
              resolvedOut, isBuiltinOut);
          explicitTarget.has_value()) {
        return *explicitTarget;
      }
      return setCollectionMethodTarget("/string/count");
    }
    if (normalizedMethodName == "count" &&
        setIndexedArgsPackKeyValueMethodTarget(
            receiver, "count", explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
      return true;
    }
    if (this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      if (normalizedMethodName == "count") {
        if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
              receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
              resolvedOut, isBuiltinOut);
            explicitTarget.has_value()) {
          return *explicitTarget;
        }
      }
      return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
    }
  }
  if (normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
      normalizedMethodName == "insert") {
    if (setIndexedArgsPackKeyValueMethodTarget(
            receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
      return true;
    }
    if (normalizedMethodName != "insert" &&
        this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
    }
  }
  if (normalizedMethodName == "insert") {
    if (this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      return setPreferredKeyValueMethodTarget(receiver, "insert");
    }
  }
  if (normalizedMethodName == "capacity") {
    if (this->resolveArrayTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        (hasDeclaredDefinitionPath("/array/capacity") ||
         hasImportedDefinitionPath("/array/capacity"))) {
      return setCollectionMethodTarget("/array/capacity");
    }
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
    }
    if (this->resolveCollectionVectorValueTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
    }
  }
  if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
    if (resolveArgsPackAccessTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/" + normalizedMethodName);
    }
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
    if (this->resolveCollectionVectorValueTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
    if (this->resolveArrayTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget("/array/" + normalizedMethodName);
    }
    if (this->resolveStringTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    }
  }
  if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
      setIndexedArgsPackKeyValueMethodTarget(
          receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
    return true;
  }
  if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
      this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
    return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
  }
  if (normalizedMethodName == "get" || normalizedMethodName == "get_ref") {
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                     "/vector")) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    "/vector"));
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        resolveBorrowedVectorReceiver(receiver, elemType, params, locals) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                     "/vector")) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    "/vector"));
    }
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    internalSoaCollectionTypePath(true)));
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaAccessHelperTarget(normalizedMethodName));
    }
  }
  if (normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") {
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector"));
    }
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(
              normalizedMethodName, internalSoaCollectionTypePath(true)));
    }
    if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredSoaToAosHelperTargetForReceiver(receiver));
    }
  }
  if (normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") {
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector"));
    }
    // Direct (owned) soa receivers must be tried before the OR-combined
    // resolveSoaVectorOrExperimentalBorrowedReceiver check below: that
    // check's "direct" branch also matches an owned receiver (it is an OR
    // of "direct" and "borrowed"), but its result unconditionally builds
    // the borrowed *_ref target name, so checking it first would route an
    // owned receiver to the borrowed helper. get/ref above already use
    // this same ordering (direct soa check before the OR-combined check).
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(
              normalizedMethodName, internalSoaCollectionTypePath(true)));
    }
    if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaAccessHelperTarget(normalizedMethodName));
    }
  }
  if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
          receiver, params, locals, resolveDirectReceiver, elemType)) {
    const std::string normalizedElemType = normalizeBindingTypeName(elemType);
    std::string currentNamespace;
    if (!currentValidationState_.context.definitionPath.empty()) {
      const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace = currentValidationState_.context.definitionPath.substr(0, slash);
      }
    }
    const std::string lookupNamespace =
        !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentNamespace;
    const std::string elementStructPath = resolveStructTypePath(normalizedElemType, lookupNamespace);
    if (!elementStructPath.empty()) {
      auto structIt = defMap_.find(elementStructPath);
      if (structIt != defMap_.end() && structIt->second != nullptr) {
        for (const auto &stmt : structIt->second->statements) {
          if (!stmt.isBinding || isStaticBinding(stmt) || stmt.name != normalizedMethodName) {
            continue;
          }
          if (hasVisibleSoaHelperTargetForCurrentImports(normalizedMethodName)) {
            resolvedOut =
                preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
            isBuiltinOut = false;
          } else {
            resolvedOut = soaFieldViewHelperPath(normalizedMethodName);
            isBuiltinOut = true;
          }
          return true;
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    const std::string removedVectorMethodCompatibilityPath =
        receiver.isMethodCall
            ? this->explicitRemovedCollectionMethodPath(receiver.name, receiver.namespacePrefix)
            : std::string();
    std::string accessHelperName;
    if (getBuiltinArrayAccessName(receiver, accessHelperName) && !receiver.args.empty()) {
      const std::string removedKeyValueCompatibilityPath =
          getDirectKeyValueHelperCompatibilityPath(receiver, params, locals,
                                                   resolveArgsPackAccessTarget);
      if (!removedKeyValueCompatibilityPath.empty()) {
        return failMethodTargetResolutionDiagnostic("unknown call target: " +
                                                    removedKeyValueCompatibilityPath);
      }
      size_t accessReceiverIndex = 0;
      if (!receiver.isMethodCall && hasNamedArguments(receiver.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < receiver.args.size(); ++i) {
          if (i < receiver.argNames.size() && receiver.argNames[i].has_value() &&
              *receiver.argNames[i] == "values") {
            accessReceiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          accessReceiverIndex = 0;
        }
      }
      if (accessReceiverIndex < receiver.args.size()) {
        const Expr &accessReceiver = receiver.args[accessReceiverIndex];
        const std::string removedVectorAccessCompatibilityPath =
            receiver.isMethodCall
                ? this->explicitRemovedCollectionMethodPath(receiver.name, receiver.namespacePrefix)
                : std::string();
        const bool hasSamePathRemovedVectorAccessHelper =
            !removedVectorAccessCompatibilityPath.empty() &&
            (hasDefinitionPath(removedVectorAccessCompatibilityPath) ||
             hasImportedDefinitionPath(removedVectorAccessCompatibilityPath));
        if ((removedVectorAccessCompatibilityPath == "/array/at" ||
             removedVectorAccessCompatibilityPath == "/array/at_unsafe") &&
            !hasSamePathRemovedVectorAccessHelper) {
          std::string vectorElemType;
          if (this->resolveVectorTarget(accessReceiver, vectorElemType, params, locals,
                                        resolveArgsPackAccessTarget)) {
            return failMethodTargetResolutionDiagnostic("unknown method: " +
                                                        removedVectorAccessCompatibilityPath);
          }
        }
        std::string indexedArgsPackElemType;
        if (!resolveArgsPackAccessTarget(accessReceiver, indexedArgsPackElemType)) {
          auto accessDefIt = defMap_.find(resolveCalleePath(receiver));
          if (accessDefIt != defMap_.end() && accessDefIt->second != nullptr) {
            BindingInfo inferredReturn;
            if (inferDefinitionReturnBinding(*accessDefIt->second, inferredReturn)) {
              const std::string inferredTypeText =
                  inferredReturn.typeTemplateArg.empty()
                      ? inferredReturn.typeName
                      : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
              if (setMethodTargetFromTypeText(
                      inferredTypeText,
                      accessDefIt->second->namespacePrefix)) {
                return true;
              }
            }
          }
        }
        std::string accessElemType;
        std::string accessValueType;
        if (resolveArgsPackAccessTarget(accessReceiver, accessElemType) ||
            this->resolveVectorTarget(accessReceiver, accessElemType, params, locals,
                                      resolveArgsPackAccessTarget) ||
            this->resolveArrayTarget(accessReceiver, accessElemType, params, locals,
                                     resolveArgsPackAccessTarget)) {
          const std::string normalizedElemType =
              normalizeBindingTypeName(unwrapReferencePointerTypeText(accessElemType));
          std::string normalizedElemBaseType = normalizedElemType;
          if (!normalizedElemBaseType.empty() && normalizedElemBaseType.front() == '/') {
            normalizedElemBaseType.erase(normalizedElemBaseType.begin());
          }
          if (normalizedElemType == "string" || normalizedElemBaseType == "string") {
            return setCollectionMethodTarget("/string/" + normalizedMethodName);
          }
          std::string elemBase;
          std::string elemArgText;
          if (splitTemplateTypeName(normalizedElemType, elemBase, elemArgText)) {
            elemBase = normalizeBindingTypeName(elemBase);
            if (elemBase == "vector" || elemBase == "array" ||
                isInternalSoaCollectionTypeName(elemBase)) {
              return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
            }
            if (elemBase == "Buffer" &&
                (normalizedMethodName == "count" || normalizedMethodName == "empty" ||
                 normalizedMethodName == "is_valid" || normalizedMethodName == "readback" ||
                 normalizedMethodName == "load" || normalizedMethodName == "store")) {
              return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
            }
            if (isKeyValueSurfaceTypeName(elemBase)) {
              if (setIndexedArgsPackKeyValueMethodTarget(
                      receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
                return true;
              }
              return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
            }
            if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
              resolvedOut = preferredFileHelperTarget(normalizedMethodName,
                                                     currentValidationState_.context.definitionPath);
              isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
              return true;
            }
          }
          if (normalizedElemType == "FileError" &&
              (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
               normalizedMethodName == "status" || normalizedMethodName == "result")) {
            resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
            isBuiltinOut = resolvedOut == "/file_error/why";
            return !resolvedOut.empty();
          }
          if (isPrimitiveBindingTypeName(normalizedElemBaseType)) {
            resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
            return true;
          }
          std::string resolvedElemType = resolveStructTypePath(normalizedElemType, receiver.namespacePrefix);
          if (resolvedElemType.empty()) {
            resolvedElemType = resolveTypePath(normalizedElemType, receiver.namespacePrefix);
          }
          if (!resolvedElemType.empty()) {
            resolvedOut = resolvedElemType + "/" + normalizedMethodName;
            return true;
          }
        }
        if (this->resolveStringTarget(accessReceiver, params, locals, resolveArgsPackAccessTarget)) {
          resolvedOut = "/i32/" + normalizedMethodName;
          return true;
        }
        if (resolveKeyValueValueType(accessReceiver, accessValueType)) {
          std::string normalizedAccessName = receiver.name;
          if (!normalizedAccessName.empty() &&
              normalizedAccessName.front() == '/') {
            normalizedAccessName.erase(normalizedAccessName.begin());
          }
          const size_t accessTemplateSuffix = normalizedAccessName.find("__t");
          if (accessTemplateSuffix != std::string::npos) {
            normalizedAccessName.erase(accessTemplateSuffix);
          }
          const bool isExplicitAccessAlias =
              normalizedAccessName.find('/') != std::string::npos;
          const std::string preferredAccessPath =
              preferredKeyValueMethodTarget(receiver, accessHelperName, explicitKeyValueHelperPath,
                                            params, locals, resolveArgsPackAccessTarget);
          auto defIt = defMap_.find(preferredAccessPath);
          if (defIt != defMap_.end() && defIt->second != nullptr) {
            BindingInfo inferredReturn;
            if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
              const std::string inferredReturnTypeText =
                  inferredReturn.typeTemplateArg.empty()
                      ? inferredReturn.typeName
                      : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
              const std::string normalizedReturnType =
                  normalizeBindingTypeName(inferredReturnTypeText);
              std::string normalizedReturnBaseType = normalizedReturnType;
              if (!normalizedReturnBaseType.empty() &&
                  normalizedReturnBaseType.front() == '/') {
                normalizedReturnBaseType.erase(normalizedReturnBaseType.begin());
              }
              if (isPrimitiveBindingTypeName(normalizedReturnBaseType)) {
                resolvedOut = "/" + normalizedReturnBaseType + "/" +
                              normalizedMethodName;
                return true;
              }
              std::string resolvedReturnType = resolveStructTypePath(
                  normalizedReturnType, defIt->second->namespacePrefix);
              if (resolvedReturnType.empty()) {
                resolvedReturnType =
                    resolveTypePath(normalizedReturnType,
                                    defIt->second->namespacePrefix);
              }
              if (!resolvedReturnType.empty()) {
                if (tryRedirectConcreteExperimentalSoaMethodTarget(
                        resolvedReturnType, canonicalCollectionHelperName, receiver,
                        explicitRemovedMethodPath, normalizedMethodName, params, locals,
                        resolvedOut, isBuiltinOut)) {
                  return true;
                }
                resolvedOut = resolvedReturnType + "/" + normalizedMethodName;
                return true;
              }
            }
          }

          if (!isExplicitAccessAlias ||
              ((defIt != defMap_.end() && defIt->second != nullptr) ||
               hasImportedDefinitionPath(preferredAccessPath))) {
            const std::string normalizedValueType =
                normalizeBindingTypeName(accessValueType);
            std::string normalizedValueBaseType = normalizedValueType;
            if (!normalizedValueBaseType.empty() &&
                normalizedValueBaseType.front() == '/') {
              normalizedValueBaseType.erase(normalizedValueBaseType.begin());
            }
            if (isPrimitiveBindingTypeName(normalizedValueBaseType)) {
              resolvedOut = "/" + normalizedValueBaseType + "/" +
                            normalizedMethodName;
              return true;
            }
            std::string resolvedValueType =
                resolveStructTypePath(normalizedValueType, receiver.namespacePrefix);
            if (resolvedValueType.empty()) {
              resolvedValueType =
                  resolveTypePath(normalizedValueType, receiver.namespacePrefix);
            }
            if (!resolvedValueType.empty()) {
              resolvedOut = resolvedValueType + "/" + normalizedMethodName;
              return true;
            }
          }
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string dereferencedElemType;
    if (resolveDereferencedIndexedArgsPackElementType(receiver, dereferencedElemType,
                                                       resolveArgsPackAccessTarget) &&
        resolveArgsPackElementMethodTarget(
            dereferencedElemType, receiver, normalizedMethodName, setCollectionMethodTarget,
            setPreferredKeyValueMethodTarget, resolvedOut, isBuiltinOut)) {
      return true;
    }
  }
  {
    bool handledRetiredMaybeMutableHelper = false;
    if (bool ok = resolveRetiredMaybeMutableHelperMethodTarget(
            receiver, normalizedMethodName, params, locals,
            handledRetiredMaybeMutableHelper);
        handledRetiredMaybeMutableHelper) {
      return ok;
    }
  }
  if (isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsManifestSurface0, normalizedMethodName)) {
    std::string vectorMethodTarget;
    if (resolveVectorHelperMethodTarget(params, locals, receiver, normalizedMethodName,
                                        vectorMethodTarget)) {
      const bool isVectorFamilyTarget =
          splitSoaSurfaceHelperPath(vectorMethodTarget, nullptr, nullptr) ||
          startsWithRootedVectorMethodPrefix(vectorMethodTarget) ||
          isCanonicalVectorCompatibilityPath(vectorMethodTarget) ||
          isLegacyExperimentalVectorCompatibilityPath(vectorMethodTarget);
      if (!isVectorFamilyTarget) {
        vectorMethodTarget.clear();
      }
    }
    if (!vectorMethodTarget.empty()) {
      if (isLegacyExperimentalVectorCompatibilityPath(vectorMethodTarget) &&
          !hasDeclaredDefinitionPath(vectorMethodTarget) &&
          !hasImportedDefinitionPath(vectorMethodTarget)) {
        const std::string canonicalVectorMethodTarget =
            canonicalVectorHelperTarget(normalizedMethodName);
        if (isPublishedVectorMutatorHelperName(normalizedMethodName) ||
            hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
            hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
          vectorMethodTarget = canonicalVectorMethodTarget;
        }
      }
      return setCollectionMethodTarget(vectorMethodTarget);
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    const std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      std::string experimentalElemType;
      BindingInfo receiverBinding;
      receiverBinding.typeName = resolvedType;
      const bool isConcreteExperimentalSoaReceiver =
          isExperimentalSoaVectorSpecializedTypePath(resolvedType);
      const bool isCanonicalSoaWrapperMethod =
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
      if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
        return setCollectionMethodTarget(
            preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                      internalSoaCollectionTypePath(true)));
      }
      if ((((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
            usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) ||
           normalizedMethodName == "capacity" ||
           normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
          extractCollectionVectorElementType(receiverBinding, experimentalElemType)) {
        if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
            usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
          return setCollectionMethodTarget(
              preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                        "/vector"));
        }
        if (normalizedMethodName == "count") {
          return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
        }
        if (normalizedMethodName == "capacity") {
          return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
        }
        return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
      }
      if (extractExperimentalSoaVectorElementType(receiverBinding, experimentalElemType) &&
          resolveCollectionMethodFromTypePath(
              internalSoaCollectionTypePath(true), normalizedMethodName, receiver,
              explicitVectorHelperPath, explicitKeyValueHelperPath, explicitRemovedMethodPath,
              params, locals,
              resolvedOut, isBuiltinOut)) {
        return true;
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
    if (resolveExplicitDirectCallReturnMethodTarget(
            receiver, canonicalCollectionHelperName, normalizedMethodName, receiver,
            explicitRemovedMethodPath, params, locals, resolvedOut, isBuiltinOut)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
       normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        receiverCollectionTypePath == "/vector") {
      if (normalizedMethodName == "count") {
        return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
      }
      if (normalizedMethodName == "capacity") {
        return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
      }
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
  }
  if (receiver.kind == Expr::Kind::Call) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals,
                                      receiverCollectionTypePath)) {
      if (resolveCollectionMethodFromTypePath(
              receiverCollectionTypePath, normalizedMethodName, receiver,
              explicitVectorHelperPath, explicitKeyValueHelperPath, explicitRemovedMethodPath,
              params, locals,
              resolvedOut, isBuiltinOut)) {
        return true;
      }
    }
  }

  return resolveMethodTargetGenericFallback(
      params, locals, callNamespacePrefix, receiver, normalizedMethodName,
      canonicalCollectionHelperName, explicitVectorHelperPath, explicitKeyValueHelperPath,
      explicitRemovedMethodPath, traceFileErrorResult,
      failMethodTargetResolutionDiagnostic, stampFileErrorResultFailure, resolvedOut,
      isBuiltinOut);
}

} // namespace primec::semantics

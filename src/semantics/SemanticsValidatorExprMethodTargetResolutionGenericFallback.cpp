// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorMethodTargetResolutionDetail.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/ReceiverElementFamilyClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
using namespace method_target_detail;

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
      normalizedCollectionTypePath == collection_helpers::kRootedVector) {
    return setCollectionMethodTarget("/to_soa");
  }
  if ((collection_helpers::isToAosHelperName(normalizedMethodName)) &&
      (isInternalSoaCollectionTypePath(normalizedCollectionTypePath) ||
       normalizedCollectionTypePath == collection_helpers::kRootedVector)) {
    return setCollectionMethodTarget(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(normalizedCollectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : collection_helpers::kRootedVector));
  }
  if (isKeyValueSurfaceTypeName(normalizeBindingTypeName(typeName)) &&
      (collection_helpers::isCountHelperName(normalizedMethodName) ||
       normalizedMethodName == "size" ||
       collection_helpers::isContainsHelperName(normalizedMethodName) ||
       collection_helpers::isTryAtHelperName(normalizedMethodName) ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       collection_helpers::isInsertHelperName(normalizedMethodName))) {
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
      (collection_helpers::isCountHelperName(normalizedMethodName) ||
       collection_helpers::isContainsHelperName(normalizedMethodName) ||
       collection_helpers::isTryAtHelperName(normalizedMethodName) ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       collection_helpers::isInsertHelperName(normalizedMethodName))) {
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
    return setCollectionMethodTarget(collection_helpers::kRootedStringPrefix + normalizedMethodName);
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
        collection_helpers::isCountHelperName(canonicalCollectionHelperName) ||
        collection_helpers::isGetHelperName(canonicalCollectionHelperName) ||
        collection_helpers::isRefHelperName(canonicalCollectionHelperName) ||
        collection_helpers::isToAosHelperName(canonicalCollectionHelperName);
    if (normalizedPointeeCollectionTypePath == collection_helpers::kRootedMap &&
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
        borrowedHelperName = collection_helpers::kAtUnsafeRef;
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
    if (typeName == "Reference" &&
        normalizedPointeeCollectionTypePath == collection_helpers::kRootedVector) {
      // TODO-5375: a borrowed vector resolves the vector helper spellings to
      // the canonical borrowed-vector helpers.
      const std::string_view leaf =
          collection_helpers::borrowedVectorHelperLeaf(normalizedMethodName);
      const std::string borrowedPath =
          std::string(collection_helpers::kCanonicalVectorPrefix) + std::string(leaf);
      if (!leaf.empty() &&
          (hasDeclaredDefinitionPath(borrowedPath) || hasImportedDefinitionPath(borrowedPath))) {
        resolvedOut = borrowedPath;
        isBuiltinOut = false;
        return true;
      }
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
      normalizedCollectionTypePath != collection_helpers::kRootedVector &&
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
  if (normalizedCollectionTypePath == collection_helpers::kRootedVector &&
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
      normalizedCollectionTypePath != collection_helpers::kRootedVector &&
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
    return collection_helpers::kRootedArrayPrefix + std::string(helperName);
  }
  if (compatibilityCollection == "vector") {
    return rootedVectorHelperPath(helperName);
  }
  return "/" + candidate;
}


} // namespace primec::semantics

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
        collection_helpers::isRootedArrayPath(explicitRemovedMethodPath);
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
    if (normalizedMethodName == collection_helpers::kCountRef) {
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
      collection_helpers::isRootedStringPath(path) &&
      isValueSurfaceAccessMethodName(normalizedMethodName)) {
    resolvedOut = explicitRemovedMethodPath;
    isBuiltinOut = false;
    return true;
  }
  if (!explicitRemovedMethodPath.empty() &&
      collection_helpers::isRootedArrayPath(path)) {
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
  if (!explicitRemovedMethodPath.empty() && !collection_helpers::isRootedStringPath(path)) {
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
  if (collection_helpers::isRootedArrayPath(resolvedOut) &&
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
      isCanonicalStdlibSoaHelperPath(resolvedOut, collection_helpers::kToAosRef);
  const bool matchesBuiltinSoaCollectionHelper =
      isCanonicalSoaHelperPath(resolvedSoaCountCanonical, "count") ||
      isCanonicalSoaHelperPath(resolvedSoaCountCanonical, collection_helpers::kCountRef) ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical, "get") ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical,
                                       collection_helpers::kGetRef) ||
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

std::optional<std::string> SemanticsValidator::noImportSoaHelperCallDiagnostic(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  // TODO-5318: the public soa<T> helpers only exist as
  // /std/collections/soa/* wrappers. Without that module in scope the
  // builtin fallback routes some helpers to the retired soa_vector family
  // (which then fails in IR lowering) and rejects others as unknown call
  // targets. Reject every public helper uniformly here instead.
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isFieldAccess ||
      expr.args.empty()) {
    return std::nullopt;
  }
  if (currentDefinitionContext_ != nullptr &&
      currentDefinitionContext_->fullPath.rfind("/std/", 0) == 0) {
    return std::nullopt;
  }
  std::string helperName = expr.name;
  constexpr std::string_view RootedSoaPrefix = collection_helpers::kRootedSoaPrefix;
  // Earlier rewrites may already have canonicalized method sugar on an
  // explicit SoaVector<T> receiver to /std/collections/soa/<name> even
  // though that wrapper was never imported.
  constexpr std::string_view CanonicalSoaPrefix = collection_helpers::kCanonicalSoaPrefix;
  if (helperName.rfind(RootedSoaPrefix, 0) == 0) {
    helperName.erase(0, RootedSoaPrefix.size());
  } else if (helperName.rfind(CanonicalSoaPrefix, 0) == 0) {
    helperName.erase(0, CanonicalSoaPrefix.size());
    const size_t specializationSuffix = helperName.find("__");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
  }
  if (helperName.find('/') != std::string::npos) {
    return std::nullopt;
  }
  if (!expr.isMethodCall && !expr.namespacePrefix.empty() &&
      expr.namespacePrefix != "/" && !collection_helpers::isCollectionFamilyRoot(expr.namespacePrefix, collection_helpers::CollectionFamily::Soa)) {
    return std::nullopt;
  }
  const bool isPublicSoaHelper =
      collection_helpers::isCountHelperName(helperName) ||
      collection_helpers::isGetHelperName(helperName) ||
      collection_helpers::isRefHelperName(helperName) ||
      collection_helpers::isToAosHelperName(helperName) ||
      helperName == "push" || helperName == "reserve";
  if (!isPublicSoaHelper) {
    return std::nullopt;
  }
  const std::string publicPath = collection_helpers::kCanonicalSoaPrefix + helperName;
  if (hasVisibleDefinitionPathForCurrentImports(publicPath)) {
    return std::nullopt;
  }
  // User same-path shadows (`/soa/<name>`, root `/<name>`) keep winning.
  const std::string samePath = collection_helpers::kRootedSoaPrefix + helperName;
  const std::string rootPath = "/" + helperName;
  if (hasVisibleDefinitionPathForCurrentImports(samePath) ||
      hasDefinitionFamilyPath(samePath) ||
      hasVisibleDefinitionPathForCurrentImports(rootPath) ||
      hasDefinitionFamilyPath(rootPath)) {
    return std::nullopt;
  }
  const std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTargetFn =
      [this, &params, &locals](const Expr &target, std::string &elemType) -> bool {
    return this->resolveArgsPackAccessTarget(target, elemType, params, locals);
  };
  std::string ignoredElemType;
  bool receiverIsSoa = false;
  withPreservedError([&]() {
    receiverIsSoa = resolveSoaVectorTarget(expr.args.front(), ignoredElemType,
                                           params, locals,
                                           resolveArgsPackAccessTargetFn);
    return receiverIsSoa;
  });
  if (!receiverIsSoa) {
    return std::nullopt;
  }
  return "soa helper requires import /std/collections/soa/*: " + helperName;
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
      if (collection_helpers::isCollectionFamilyRoot(normalizedStruct, collection_helpers::CollectionFamily::Map) ||
          isSpecializedExperimentalKeyValueBackingTypeForMethodTargets(normalizedStruct)) {
        typeNameOut = collection_helpers::kRootedMap;
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
          collection_helpers::isCountHelperName(normalizedMethodName) ||
          collection_helpers::isGetHelperName(normalizedMethodName) ||
          collection_helpers::isRefHelperName(normalizedMethodName) ||
          collection_helpers::isToAosHelperName(normalizedMethodName);
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
         helperName == collection_helpers::kAtRef || helperName == collection_helpers::kAtUnsafeRef;
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
  if (collection_helpers::isCountHelperName(normalizedMethodName)) {
    if (normalizedMethodName == "count" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Array)) {
      return setCollectionMethodTargetLocal(collection_helpers::kRootedArrayCount);
    }
    if (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      return setCollectionMethodTargetLocal(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector));
    }
    if (normalizedMethodName == "count" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector)) {
      return setCollectionMethodTargetLocal(
          canonicalVectorCompatibilityHelperPathOrFallback("count"));
    }
    if (isInternalSoaCollectionTypePath(collectionTypePath)) {
      return setCollectionMethodTargetLocal(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    internalSoaCollectionTypePath(true)));
    }
    if (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Soa)) {
      return setCollectionMethodTargetLocal(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedSoa));
    }
    if (normalizedMethodName == "count" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::String)) {
      return setCollectionMethodTargetLocal(collection_helpers::kRootedStringCount);
    }
    if (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Map)) {
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
  if (normalizedMethodName == "capacity" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Array) &&
      (hasDeclaredDefinitionPath(collection_helpers::kRootedArrayCapacity) ||
       hasImportedDefinitionPath(collection_helpers::kRootedArrayCapacity))) {
    return setCollectionMethodTargetLocal(collection_helpers::kRootedArrayCapacity);
  }
  if (normalizedMethodName == "capacity" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector)) {
    return setCollectionMethodTargetLocal(
        canonicalVectorCompatibilityHelperPathOrFallback("capacity"));
  }
  if ((normalizedMethodName == "empty" || normalizedMethodName == "is_valid" ||
       normalizedMethodName == "readback" || normalizedMethodName == "load" ||
       normalizedMethodName == "store") &&
      collectionTypePath == "/Buffer") {
    return setCollectionMethodTargetLocal(preferredBufferMethodTarget(normalizedMethodName));
  }
  if (normalizedMethodName == "contains" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Map)) {
    return setPreferredKeyValueMethodTargetLocal(receiver, "contains");
  }
  if (normalizedMethodName == "tryAt" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Map)) {
    return setPreferredKeyValueMethodTargetLocal(receiver, "tryAt");
  }
  if (normalizedMethodName == "insert" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Map)) {
    return setPreferredKeyValueMethodTargetLocal(receiver, "insert");
  }
  if (normalizedMethodName == "size" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Map)) {
    return setPreferredKeyValueMethodTargetLocal(receiver, "size");
  }
  if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
    if (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Array)) {
      return setCollectionMethodTargetLocal(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
    }
    if (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector)) {
      return setCollectionMethodTargetLocal(
          canonicalVectorCompatibilityHelperPathOrFallback(normalizedMethodName));
    }
    if (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::String)) {
      return setCollectionMethodTargetLocal(collection_helpers::kRootedStringPrefix + normalizedMethodName);
    }
  }
  if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
      collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Map)) {
    return setPreferredKeyValueMethodTargetLocal(receiver, normalizedMethodName);
  }
  if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : collection_helpers::kRootedVector));
  }
  if ((collection_helpers::isRefHelperName(normalizedMethodName)) &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : collection_helpers::kRootedVector));
  }
  if ((normalizedMethodName == "push" || normalizedMethodName == "reserve") &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       (collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                     collection_helpers::kRootedVector)))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : collection_helpers::kRootedVector));
  }
  if (normalizedMethodName == "to_soa" && collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector)) {
    return setCollectionMethodTargetLocal("/to_soa");
  }
  if ((collection_helpers::isToAosHelperName(normalizedMethodName)) &&
      (isInternalSoaCollectionTypePath(collectionTypePath) ||
       collection_helpers::isCollectionFamilyRoot(collectionTypePath, collection_helpers::CollectionFamily::Vector))) {
    return setCollectionMethodTargetLocal(
        preferredSoaHelperTargetForCollectionType(
            normalizedMethodName,
            isInternalSoaCollectionTypePath(collectionTypePath)
                ? internalSoaCollectionTypePath(true)
                : collection_helpers::kRootedVector));
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


} // namespace primec::semantics

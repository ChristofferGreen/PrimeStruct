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

bool SemanticsValidator::extractExperimentalKeyValueFieldTypes(
    const BindingInfo &binding, std::string &keyTypeOut, std::string &valueTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) -> bool {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        if (isUnspecializedExperimentalKeyValueBackingTypeForMethodTargets(base)) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
            return false;
          }
          keyTypeOut = args[0];
          valueTypeOut = args[1];
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!isSpecializedExperimentalKeyValueBackingTypeForMethodTargets(
              normalizedResolvedPath)) {
        return false;
      }
      return extractExperimentalKeyValueFieldTypesFromStructPath(resolvedPath, keyTypeOut, valueTypeOut);
    }
  };

  keyTypeOut.clear();
  valueTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool SemanticsValidator::isWrappedKeyValueTypeText(const std::string &typeText) const {
  std::string pointeeType;
  std::string keyType;
  std::string valueType;
  return extractWrappedPointeeType(typeText, pointeeType) &&
         extractKeyValueCollectionTypesFromTypeText(pointeeType, keyType, valueType);
}

bool SemanticsValidator::extractAnyKeyValueTypes(const BindingInfo &binding,
                                                 std::string &keyTypeOut,
                                                 std::string &valueTypeOut) const {
  return extractKeyValueCollectionTypes(binding, keyTypeOut, valueTypeOut) ||
         extractExperimentalKeyValueFieldTypes(binding, keyTypeOut, valueTypeOut);
}

bool SemanticsValidator::isWrappedKeyValueReceiver(
    const Expr &receiverExpr, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto isWrappedBinding = [&](const BindingInfo &binding) {
    return isWrappedKeyValueTypeText(bindingTypeText(binding));
  };
  if (receiverExpr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
      return isWrappedBinding(*paramBinding);
    }
    auto it = locals.find(receiverExpr.name);
    return it != locals.end() && isWrappedBinding(it->second);
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, receiverExpr, fieldBinding)) {
    return isWrappedBinding(fieldBinding);
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    std::string indexedElemType;
    return resolveIndexedArgsPackElementType(receiverExpr, indexedElemType,
                                             resolveArgsPackAccessTarget) &&
           isWrappedKeyValueTypeText(indexedElemType);
  }
  return false;
}

bool SemanticsValidator::isCanonicalKeyValueReceiver(
    const Expr &receiverExpr, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  std::string keyType;
  std::string valueType;
  if (receiverExpr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
      return extractAnyKeyValueTypes(*paramBinding, keyType, valueType);
    }
    auto it = locals.find(receiverExpr.name);
    return it != locals.end() &&
           extractAnyKeyValueTypes(it->second, keyType, valueType);
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, receiverExpr, fieldBinding)) {
    return extractAnyKeyValueTypes(fieldBinding, keyType, valueType);
  }
  if (receiverExpr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string collectionTypePath;
  if (resolveCallCollectionTypePath(receiverExpr, params, locals, collectionTypePath) &&
      collectionTypePath == "/map") {
    return true;
  }
  const std::string resolvedTarget = resolveCalleePath(receiverExpr);
  auto defIt = defMap_.find(resolvedTarget);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  BindingInfo inferredReturn;
  return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
         extractAnyKeyValueTypes(inferredReturn, keyType, valueType);
}

bool SemanticsValidator::resolveExperimentalKeyValueTarget(
    const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  keyTypeOut.clear();
  valueTypeOut.clear();
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractExperimentalKeyValueFieldTypes(*paramBinding, keyTypeOut, valueTypeOut);
    }
    auto it = locals.find(target.name);
    return it != locals.end() &&
           extractExperimentalKeyValueFieldTypes(it->second, keyTypeOut, valueTypeOut);
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding)) {
    return extractExperimentalKeyValueFieldTypes(fieldBinding, keyTypeOut, valueTypeOut);
  }
  if (target.kind != Expr::Kind::Call) {
    return false;
  }
  const std::string resolvedTarget = resolveCalleePath(target);
  if (isResolvedPublishedKeyValueConstructorPath(resolvedTarget)) {
    std::vector<std::string> args;
    if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) &&
        args.size() == 2) {
      keyTypeOut = args[0];
      valueTypeOut = args[1];
      return true;
    }
  }
  auto defIt = defMap_.find(resolvedTarget);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  BindingInfo inferredReturn;
  return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
         extractExperimentalKeyValueFieldTypes(inferredReturn, keyTypeOut, valueTypeOut);
}

std::string SemanticsValidator::borrowedKeyValueHelperNameForReceiver(
    const Expr &receiverExpr, const std::string &helperName,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  if (!isWrappedKeyValueReceiver(receiverExpr, params, locals, resolveArgsPackAccessTarget)) {
    return helperName;
  }
  // TODO-4691: count/contains/tryAt/at/insert now resolve via the
  // registry-backed borrowed-variant lookup instead of a hardcoded
  // literal chain. at_unsafe -> at_unsafe_ref stays hardcoded: TODO-4690
  // deliberately left that pair out of the registry table because a
  // pre-existing stdlib-map-ownership audit test forbids the
  // "at_unsafe_ref" literal appearing in StdlibSurfaceRegistry.cpp.
  if (helperName == "at_unsafe") {
    return std::string("at_unsafe_ref");
  }
  if (const std::string_view borrowedVariant = findBorrowedVariant(
          StdlibSurfaceId::CollectionsManifestSurface2, helperName);
      !borrowedVariant.empty()) {
    return std::string(borrowedVariant);
  }
  return helperName;
}

std::string SemanticsValidator::preferredKeyValueMethodTarget(
    const Expr &receiverExpr, const std::string &helperName,
    const std::string &explicitKeyValueHelperPath,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  const std::string resolvedHelperName =
      explicitKeyValueHelperPath.empty()
          ? borrowedKeyValueHelperNameForReceiver(receiverExpr, helperName, params, locals,
                                                  resolveArgsPackAccessTarget)
          : helperName;
  std::string keyType;
  std::string valueType;
  const std::string canonical = canonicalKeyValueHelperPathLocal(resolvedHelperName);
  const bool receiverIsCompatibleExperimentalKeyValueTarget =
      resolveExperimentalKeyValueTarget(receiverExpr, keyType, valueType, params, locals);
  const bool receiverIsCanonicalKeyValueTarget =
      isCanonicalKeyValueReceiver(receiverExpr, params, locals);
  if (receiverIsCompatibleExperimentalKeyValueTarget) {
    if (hasDeclaredDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
      return canonical;
    }
    return preferredCanonicalExperimentalKeyValueHelperTarget(resolvedHelperName);
  }
  std::string explicitCanonicalKeyValueHelperName;
  if (resolveCanonicalKeyValueHelperNameFromSpelling(
          explicitKeyValueHelperPath, explicitCanonicalKeyValueHelperName)) {
    if (receiverIsCompatibleExperimentalKeyValueTarget ||
        receiverIsCanonicalKeyValueTarget ||
        isResolvedPublishedKeyValueConstructorPath(resolveCalleePath(receiverExpr))) {
      return canonical;
    }
    return std::string{};
  }
  if (hasDeclaredDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    if (receiverIsCompatibleExperimentalKeyValueTarget ||
        receiverIsCanonicalKeyValueTarget) {
      return canonical;
    }
  }
  return std::string{};
}

bool SemanticsValidator::setPreferredKeyValueMethodTarget(
    const Expr &receiverExpr, const std::string &helperName,
    const std::string &explicitKeyValueHelperPath, const Expr &receiver,
    const std::string &explicitRemovedMethodPath, const std::string &normalizedMethodName,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string &resolvedOut, bool &isBuiltinOut) {
  const std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTargetFn =
      [this, &params, &locals](const Expr &target, std::string &elemType) -> bool {
    return this->resolveArgsPackAccessTarget(target, elemType, params, locals);
  };
  const std::string preferredKeyValueHelper = preferredKeyValueMethodTarget(
      receiverExpr, helperName, explicitKeyValueHelperPath, params, locals,
      resolveArgsPackAccessTargetFn);
  if (preferredKeyValueHelper.empty()) {
    const std::string directPath =
        explicitKeyValueHelperPath.empty()
            ? canonicalKeyValueHelperPathLocal(helperName)
            : explicitKeyValueHelperPath;
    const bool isBareBareMapCall =
        !receiver.isMethodCall &&
        (helperName == "count" || helperName == "count_ref");
    const std::string errorTargetPath = isBareBareMapCall ? helperName : directPath;
    return failExprDiagnostic(receiver,
        receiver.isMethodCall ? "unknown method: " + directPath
                              : "unknown call target: " + errorTargetPath);
  }
  if (hasDeclaredDefinitionPath(preferredKeyValueHelper) ||
      hasDefinitionFamilyPath(preferredKeyValueHelper)) {
    resolvedOut = preferredKeyValueHelper;
    isBuiltinOut = false;
    return true;
  }
  return resolveExplicitOrCanonicalCollectionMethodTarget(
      preferredKeyValueHelper, explicitRemovedMethodPath, normalizedMethodName, receiver,
      params, locals, resolvedOut, isBuiltinOut);
}

bool SemanticsValidator::resolveKeyValueTarget(
    const Expr &target, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  std::string keyType;
  std::string valueType;
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractAnyKeyValueTypes(*paramBinding, keyType, valueType);
    }
    auto it = locals.find(target.name);
    return it != locals.end() && extractAnyKeyValueTypes(it->second, keyType, valueType);
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding)) {
    return extractAnyKeyValueTypes(fieldBinding, keyType, valueType);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string elemType;
    if ((resolveIndexedArgsPackElementType(target, elemType, resolveArgsPackAccessTarget) ||
         resolveDereferencedIndexedArgsPackElementType(target, elemType,
                                                        resolveArgsPackAccessTarget) ||
         resolveWrappedIndexedArgsPackElementType(target, elemType, resolveArgsPackAccessTarget)) &&
        extractKeyValueCollectionTypesFromTypeText(elemType, keyType, valueType)) {
      return true;
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
        if (resolveArgsPackAccessTarget(*accessReceiver, elemType) &&
            extractKeyValueCollectionTypesFromTypeText(elemType, keyType, valueType)) {
          return true;
        }
      }
    }
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        collectionTypePath == "/map") {
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) &&
          args.size() == 2) {
        return true;
      }
      std::string collectionName;
      if (getBuiltinCollectionName(target, collectionName) &&
          collectionName == "map" &&
          target.templateArgs.size() == 2) {
        return true;
      }
      return true;
    }
    auto defIt = defMap_.find(resolveCalleePath(target));
    if (defIt == defMap_.end() || !defIt->second) {
      return false;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
        extractAnyKeyValueTypes(inferredReturn, keyType, valueType)) {
      return true;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        return returnsKeyValueCollectionType(transform.templateArgs.front());
      }
    }
  }
  return false;
}

bool SemanticsValidator::resolveMethodTargetKeyValueValueType(
    const Expr &target, std::string &valueTypeOut, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  valueTypeOut.clear();
  std::string keyType;
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractAnyKeyValueTypes(*paramBinding, keyType, valueTypeOut);
    }
    auto it = locals.find(target.name);
    return it != locals.end() && extractAnyKeyValueTypes(it->second, keyType, valueTypeOut);
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding)) {
    return extractAnyKeyValueTypes(fieldBinding, keyType, valueTypeOut);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string elemType;
    if ((resolveIndexedArgsPackElementType(target, elemType, resolveArgsPackAccessTarget) ||
         resolveWrappedIndexedArgsPackElementType(target, elemType, resolveArgsPackAccessTarget) ||
         resolveDereferencedIndexedArgsPackElementType(target, elemType,
                                                        resolveArgsPackAccessTarget)) &&
        extractKeyValueCollectionTypesFromTypeText(elemType, keyType, valueTypeOut)) {
      return true;
    }
    std::string collectionTypePath;
    if (!resolveCallCollectionTypePath(target, params, locals, collectionTypePath) ||
        collectionTypePath != "/map") {
      return false;
    }
    std::vector<std::string> args;
    if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) &&
        args.size() == 2) {
      valueTypeOut = args[1];
      return true;
    }
    std::string collectionName;
    if (getBuiltinCollectionName(target, collectionName) &&
        collectionName == "map" &&
        target.templateArgs.size() == 2) {
      valueTypeOut = target.templateArgs[1];
      return true;
    }
    return false;
  }
  return false;
}

std::string SemanticsValidator::getDirectKeyValueHelperCompatibilityPath(
    const Expr &candidate, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
    return "";
  }
  std::string helperName;
  helperName = rootAliasKeyValueHelperNameForMethodTargets(
      candidate.name, candidate.namespacePrefix);
  if (helperName.empty() || !isRemovedKeyValueCompatibilityHelper(helperName)) {
    return "";
  }
  const std::string removedPath =
      rootedKeyValueHelperAliasPathForMethodTargets(helperName);
  if (removedPath.empty()) {
    return "";
  }
  if (defMap_.find(removedPath) != defMap_.end() || candidate.args.empty()) {
    return "";
  }
  if (isCanonicalKeyValueAccessMethodName(helperName)) {
    const std::string canonicalPath = canonicalKeyValueHelperPathLocal(helperName);
    auto defIt = defMap_.find(canonicalPath);
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" ||
            transform.templateArgs.size() != 1) {
          continue;
        }
        std::string returnType =
            normalizeBindingTypeName(transform.templateArgs.front());
        if (!returnType.empty() && returnType.front() == '/') {
          returnType.erase(returnType.begin());
        }
        if (!returnType.empty() && !isRootBuiltinName(returnType) &&
            returnType != "string" && returnType != "map" &&
            returnType != "vector" && returnType != "array") {
          return "";
        }
      }
    }
  }
  if (isCanonicalKeyValueAccessMethodName(helperName)) {
    return removedPath;
  }
  size_t receiverIndex = 0;
  if (hasNamedArguments(candidate.argNames)) {
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        receiverIndex = i;
        break;
      }
    }
  }
  return receiverIndex < candidate.args.size() &&
             resolveKeyValueTarget(candidate.args[receiverIndex], params, locals,
                                   resolveArgsPackAccessTarget)
             ? removedPath
             : "";
}

std::string SemanticsValidator::explicitKeyValueMethodPath(
    const std::string &rawMethodName, const std::string &callNamespacePrefix) const {
  std::string candidate = rawMethodName;
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  std::string normalizedPrefix = callNamespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  if (isKeyValueHelperImportAliasNamespaceForMethodTargets(normalizedPrefix)) {
    return rootedKeyValueHelperAliasPathForMethodTargets(candidate);
  }
  if (normalizedPrefix == canonicalKeyValueHelperNamespaceLocal()) {
    return canonicalKeyValueHelperPathLocal(candidate);
  }
  if (!metadataBackedKeyValueHelperRootAliasMethodName(candidate).empty()) {
    return "/" + candidate;
  }
  std::string canonicalKeyValueHelperName;
  if (resolveCanonicalKeyValueHelperNameFromSpelling(candidate,
                                                canonicalKeyValueHelperName)) {
    return canonicalKeyValueHelperPathLocal(canonicalKeyValueHelperName);
  }
  return "";
}

bool SemanticsValidator::setIndexedArgsPackKeyValueMethodTarget(
    const Expr &receiverExpr, const std::string &helperName,
    const std::string &explicitKeyValueHelperPath, const Expr &receiver,
    const std::string &explicitRemovedMethodPath, const std::string &normalizedMethodName,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string &resolvedOut, bool &isBuiltinOut) {
  if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.args.size() != 2) {
    return false;
  }
  const std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTargetFn =
      [this, &params, &locals](const Expr &target, std::string &elemType) -> bool {
    return this->resolveArgsPackAccessTarget(target, elemType, params, locals);
  };
  std::string indexedElemType;
  std::string keyType;
  std::string valueType;
  auto isKeyValueElementType = [&](const std::string &typeText) {
    const std::string unwrappedType =
        normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    const std::string keyValueTypeText =
        unwrappedType.empty() ? typeText : unwrappedType;
    return extractKeyValueCollectionTypesFromTypeText(keyValueTypeText, keyType, valueType);
  };
  const bool resolvedIndexedKeyValueType =
      ((resolveIndexedArgsPackElementType(receiverExpr, indexedElemType,
                                          resolveArgsPackAccessTargetFn) ||
        resolveWrappedIndexedArgsPackElementType(receiverExpr, indexedElemType,
                                                 resolveArgsPackAccessTargetFn) ||
        resolveDereferencedIndexedArgsPackElementType(receiverExpr, indexedElemType,
                                                       resolveArgsPackAccessTargetFn)) &&
       isKeyValueElementType(indexedElemType));
  const bool resolvedReceiverPackType = [&]() {
    std::string accessName;
    if (!getBuiltinArrayAccessName(receiverExpr, accessName)) {
      return false;
    }
    const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(receiverExpr);
    return accessReceiver != nullptr &&
           resolveArgsPackAccessTargetFn(*accessReceiver, indexedElemType) &&
           isKeyValueElementType(indexedElemType);
  }();
  if (!resolvedIndexedKeyValueType && !resolvedReceiverPackType) {
    return false;
  }
  return setPreferredKeyValueMethodTarget(receiverExpr, helperName, explicitKeyValueHelperPath,
                                          receiver, explicitRemovedMethodPath,
                                          normalizedMethodName, params, locals,
                                          resolvedOut, isBuiltinOut);
}

bool SemanticsValidator::resolveExplicitRootKeyValueMethodPath(
    const std::string &explicitKeyValueHelperPath, const Expr &receiver,
    std::string &resolvedOut, bool &isBuiltinOut) {
  if (!isRootedKeyValueHelperAliasPathForMethodTargets(explicitKeyValueHelperPath)) {
    return false;
  }
  if (hasDeclaredDefinitionPath(explicitKeyValueHelperPath)) {
    resolvedOut = explicitKeyValueHelperPath;
    isBuiltinOut = false;
    return true;
  }
  return failExprDiagnostic(receiver, "unknown method: " + explicitKeyValueHelperPath);
}

} // namespace primec::semantics

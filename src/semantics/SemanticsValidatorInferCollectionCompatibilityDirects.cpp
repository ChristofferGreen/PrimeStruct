// soa-surface-audit: exempt
#include "SemanticsValidator.h"

#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CompileArena.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorInferCollectionCompatibilityHelpers.h"

namespace primec::semantics {
using namespace collectionCompatibilityHelpers;

std::string SemanticsValidator::directKeyValueHelperCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
    return "";
  }
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyKeyValueTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveKeyValueTarget(target, keyType, valueType);
  };
  const std::string resolvedPath = [&]() {
    const std::string resolved = resolveCalleePath(candidate);
    if (!resolved.empty()) {
      return resolved;
    }
    return explicitCallPathForCandidate(candidate);
  }();
  const std::string explicitPath = explicitCallPathForCandidate(candidate);
  std::string helperName;
  const bool resolvedCompatibilityHelper =
      resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
          resolvedPath, helperName);
  bool resolvedBareKeyValueHelper = false;
  std::string explicitSurfaceHelperName;
  const bool spellsCurrentKeyValueWrapperSurface =
      candidate.namespacePrefix.empty() &&
      resolvePublishedKeyValueHelperMemberTokenLocal(candidate.name,
                                                     explicitSurfaceHelperName) &&
      explicitSurfaceHelperName == helperName &&
      trimLeadingSlash(candidate.name) != helperName &&
      metadataBackedKeyValueHelperRootAliasMethodName(explicitPath).empty() &&
      !isCanonicalKeyValueHelperResolvedPathLocal(explicitPath);
  if (!resolvedCompatibilityHelper &&
      !resolveExplicitPublishedKeyValueHelperExprMemberName(
          candidate.name, candidate.namespacePrefix, helperName)) {
    if (candidate.namespacePrefix.empty() &&
        resolvePublishedKeyValueHelperMemberTokenLocal(candidate.name, helperName) &&
        isPublishedKeyValueBaseHelperName(helperName)) {
      resolvedBareKeyValueHelper = true;
    } else {
      return "";
    }
  }
  const std::string canonicalPath = canonicalKeyValueHelperPathLocal(helperName);
  std::string resolvedExperimentalHelperName;
  if (isCanonicalKeyValueHelperResolvedPathLocal(
          currentValidationState_.context.definitionPath) &&
      resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
      resolvePublishedKeyValueHelperResolvedPathLocal(resolvedPath,
                                                      resolvedExperimentalHelperName)) {
    return "";
  }
  if (resolvedCompatibilityHelper && spellsCurrentKeyValueWrapperSurface) {
    return "";
  }
  const std::string removedPath = rootedKeyValueCompatibilityHelperPath(helperName);
  if (removedPath.empty()) {
    return "";
  }
  if (hasExplicitDefinitionFamilyPath(program_, defMap_, removedPath) ||
      candidate.args.empty()) {
    return "";
  }
  if (!metadataBackedKeyValueHelperRootAliasMethodName(explicitPath).empty()) {
    return removedPath;
  }
  if (matchesResolvedPath(resolvedPath, canonicalPath)) {
    return "";
  }
  auto canonicalAccessHelperReturnsStruct = [&]() {
    if (helperName != "at" && helperName != "at_unsafe" &&
        helperName != collection_helpers::kAtRef && helperName != collection_helpers::kAtUnsafeRef) {
      return false;
    }
    const std::string canonicalPath = canonicalKeyValueHelperPathLocal(helperName);
    auto defIt = defMap_.find(canonicalPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string returnType = normalizeBindingTypeName(transform.templateArgs.front());
      if (!returnType.empty() && returnType.front() == '/') {
        returnType.erase(returnType.begin());
      }
      return !returnType.empty() && !isRootBuiltinName(returnType) &&
             returnType != "string" && returnType != "map" &&
             returnType != "vector" && returnType != "array";
    }
    return false;
  };
  if (canonicalAccessHelperReturnsStruct()) {
    return "";
  }
  auto resolveReceiverIndex = [&]() -> size_t {
    if (!hasNamedArguments(candidate.argNames)) {
      return 0;
    }
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        return i;
      }
    }
    return 0;
  };
  auto hasKeyValueReceiver = [&]() {
    const size_t receiverIndex = resolveReceiverIndex();
    return receiverIndex < candidate.args.size() &&
           resolveAnyKeyValueTarget(candidate.args[receiverIndex]);
  };
  if (resolvedBareKeyValueHelper) {
    if (hasKeyValueReceiver() &&
        !hasDeclaredDefinitionPath(canonicalPath) &&
        !hasImportedDefinitionPath(canonicalPath)) {
      return canonicalPath;
    }
    return "";
  }
  if (helperName == "at" || helperName == "at_unsafe") {
    return removedPath;
  }
  const size_t receiverIndex = resolveReceiverIndex();
  if (receiverIndex >= candidate.args.size() || !resolveAnyKeyValueTarget(candidate.args[receiverIndex])) {
    return "";
  }
  return removedPath;
}

std::string SemanticsValidator::explicitRemovedCollectionMethodPath(std::string_view rawMethodName,
                                                                    std::string_view namespacePrefix) const {
  std::string rawName = std::string(trimLeadingSlash(rawMethodName));
  std::string rawNamespace = std::string(trimLeadingSlash(namespacePrefix));
  auto declaredRootVectorAliasPath = [&]() -> std::string {
    std::string_view helperName = rawName;
    if (rawNamespace == "vector") {
      helperName = rawName;
    } else if (rawNamespace.empty() && helperName.rfind(unrootedVectorHelperPrefix(), 0) == 0) {
      helperName.remove_prefix(unrootedVectorHelperPrefix().size());
    } else {
      return "";
    }
    if (!isPublishedVectorMutatorHelperName(helperName)) {
      return "";
    }
    std::string aliasPath = rootedVectorHelperPath(helperName);
    return hasDefinitionPath(aliasPath) ? aliasPath : "";
  };
  if (std::string aliasPath = declaredRootVectorAliasPath(); !aliasPath.empty()) {
    return aliasPath;
  }

  RemovedCollectionHelperFamily family = RemovedCollectionHelperFamily::VectorLike;
  std::string helperName;
  bool preserveArrayPath = false;
  if (!resolveRemovedCollectionHelperReference(rawMethodName, namespacePrefix, family, helperName, preserveArrayPath)) {
    return "";
  }
  return removedCollectionMethodPath(family, helperName, preserveArrayPath);
}

std::string SemanticsValidator::methodRemovedCollectionCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
      candidate.args.empty()) {
    return "";
  }

  RemovedCollectionHelperFamily family = RemovedCollectionHelperFamily::VectorLike;
  std::string helperName;
  bool preserveArrayPath = false;
  if (!resolveRemovedCollectionHelperReference(
          candidate.name, candidate.namespacePrefix, family, helperName, preserveArrayPath)) {
    return "";
  }

  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  if (family == RemovedCollectionHelperFamily::Map) {
    const std::string removedPath = removedCollectionMethodPath(family, helperName, preserveArrayPath);
    if (removedPath.empty() ||
        hasExplicitDefinitionFamilyPath(program_, defMap_, removedPath)) {
      return "";
    }
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(candidate.args.front(), keyType, valueType) ? removedPath : "";
  }

  std::string elemType;
  if ((helperName == "at" || helperName == "at_unsafe") &&
      dispatchResolvers.resolveVectorTarget(candidate.args.front(), elemType)) {
    return "";
  }
  if (!dispatchResolvers.resolveVectorTarget(candidate.args.front(), elemType) &&
      !dispatchResolvers.resolveArrayTarget(candidate.args.front(), elemType) &&
      !dispatchResolvers.resolveSoaVectorTarget(candidate.args.front(), elemType)) {
    return "";
  }
  return removedCollectionMethodPath(family, helperName, preserveArrayPath);
}

bool SemanticsValidator::getVectorMutatorHelperName(const Expr &candidate,
                                                    std::string &helperNameOut) const {
  helperNameOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }

  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  if (isPublishedVectorMutatorHelperName(normalizedName)) {
    helperNameOut = normalizedName;
    return true;
  }
  const std::string oldExplicitSoaPath = explicitOldSoaHelperPath(candidate);
  if (!oldExplicitSoaPath.empty()) {
    const std::string oldExplicitSoaHelperName =
        oldExplicitSoaPath.substr(oldExplicitSoaPath.find_last_of('/') + 1);
    // explicitOldSoaHelperPath()/isSoaSamePathHelperName() cover the whole
    // same-path-shadow family (count/get/ref/to_aos/push/reserve), not just
    // the genuine statement-only mutators - only push/reserve actually
    // require statement position, matching the canonical-path branch below.
    if (oldExplicitSoaHelperName != "push" && oldExplicitSoaHelperName != "reserve") {
      return false;
    }
    helperNameOut = oldExplicitSoaHelperName;
    return true;
  }
  auto canonicalSoaMutatorHelperName = [&]() -> std::string {
    std::string normalizedCanonicalName = normalizedName;
    std::string normalizedCanonicalPrefix =
        std::string(trimLeadingSlash(candidate.namespacePrefix));
    if (const size_t specializationSuffix = normalizedCanonicalName.find("__");
        specializationSuffix != std::string::npos) {
      normalizedCanonicalName.erase(specializationSuffix);
    }
    if (const size_t specializationSuffix = normalizedCanonicalPrefix.find("__");
        specializationSuffix != std::string::npos) {
      normalizedCanonicalPrefix.erase(specializationSuffix);
    }
    constexpr std::string_view kCanonicalSoaPrefix = "std/collections/soa/";
    if (normalizedCanonicalPrefix == "std/collections/soa" &&
        (normalizedCanonicalName == "push" || normalizedCanonicalName == "reserve")) {
      return normalizedCanonicalName;
    }
    if (normalizedCanonicalName.rfind(kCanonicalSoaPrefix, 0) != 0) {
      return {};
    }
    std::string helperName =
        normalizedCanonicalName.substr(kCanonicalSoaPrefix.size());
    if (helperName == "push" || helperName == "reserve") {
      return helperName;
    }
    return {};
  }();
  if (!canonicalSoaMutatorHelperName.empty()) {
    helperNameOut = canonicalSoaMutatorHelperName;
    return true;
  }

  std::string removedPath = explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(resolveCalleePath(candidate), "");
  }
  if (!isCanonicalVectorCompatibilityPath(removedPath) &&
      !isRootedVectorHelperPath(removedPath) &&
      !collection_helpers::isRootedArrayPath(removedPath)) {
    return false;
  }

  const std::string helperName = removedPath.substr(removedPath.find_last_of('/') + 1);
  if (collection_helpers::isRootedArrayPath(removedPath)) {
    const std::string canonicalPath = canonicalVectorCompatibilityHelperPathOrFallback(helperName);
    if (hasDefinitionPath(canonicalPath) || hasImportedDefinitionPath(canonicalPath)) {
      return false;
    }
  }
  if (!isPublishedVectorMutatorHelperName(helperName)) {
    return false;
  }
  helperNameOut = helperName;
  return true;
}

std::string SemanticsValidator::getRemovedRootedVectorDirectCallPath(
    const Expr &candidate) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
    return "";
  }

  const std::string explicitPath = explicitCallPathForCandidate(candidate);
  if (!isRootedVectorHelperPath(explicitPath)) {
    return "";
  }

  std::string removedPath =
      explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(explicitPath, "");
  }
  if (!isRootedVectorHelperPath(removedPath)) {
    return "";
  }
  return (hasDefinitionPath(removedPath) || hasImportedDefinitionPath(removedPath))
             ? ""
             : removedPath;
}

std::string SemanticsValidator::getRemovedRootedVectorDirectCallDiagnostic(
    const Expr &candidate) const {
  const std::string removedPath = getRemovedRootedVectorDirectCallPath(candidate);
  if (removedPath.empty()) {
    return "";
  }
  return "unknown call target: " + removedPath;
}

bool SemanticsValidator::shouldPreserveRemovedCollectionHelperPath(const std::string &path) const {
  RemovedCollectionHelperFamily family = RemovedCollectionHelperFamily::VectorLike;
  std::string helperName;
  bool preserveArrayPath = false;
  if (!resolveRemovedCollectionHelperReference(path, "", family, helperName, preserveArrayPath)) {
    return false;
  }
  return !removedCollectionMethodPath(family, helperName, preserveArrayPath).empty();
}

bool SemanticsValidator::isUnnamespacedMapCountBuiltinFallbackCall(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyKeyValueTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveKeyValueTarget(target, keyType, valueType);
  };
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }
  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const bool spellsCount = normalized == "count";
  const bool resolvesCount = resolveCalleePath(candidate) == "/count";
  if (!spellsCount && !resolvesCount) {
    return false;
  }
  if (defMap_.find("/count") != defMap_.end() || candidate.args.empty()) {
    return false;
  }
  size_t receiverIndex = 0;
  if (hasNamedArguments(candidate.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        receiverIndex = i;
        foundValues = true;
        break;
      }
    }
    if (!foundValues) {
      receiverIndex = 0;
    }
  }
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  return resolveAnyKeyValueTarget(candidate.args[receiverIndex]);
}

bool SemanticsValidator::resolveRemovedMapBodyArgumentTarget(const Expr &candidate,
                                                             const std::string &resolvedPath,
                                                             const std::vector<ParameterInfo> &params,
                                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                                             const BuiltinCollectionDispatchResolverAdapters &adapters,
                                                             std::string &targetPathOut) {
  targetPathOut.clear();
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyKeyValueTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveKeyValueTarget(target, keyType, valueType);
  };

  auto preferredRemovedKeyValueHelperPath = [&](std::string_view helperName) {
    return canonicalKeyValueHelperPathLocal(helperName);
  };

  if (candidate.kind != Expr::Kind::Call || candidate.name.empty() || candidate.args.empty()) {
    return false;
  }

  if (!candidate.isMethodCall) {
    std::string helperName;
    if (!resolveExplicitPublishedKeyValueHelperExprMemberName(
            candidate.name, "", helperName) &&
        !resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
            resolvedPath, helperName)) {
      return false;
    }
    if (!isPublishedKeyValueBaseHelperName(helperName) ||
        defMap_.count("/" + helperName) > 0) {
      return false;
    }

    auto tryResolveReceiverIndex = [&](size_t index) -> bool {
      if (index >= candidate.args.size()) {
        return false;
      }
      if (!resolveAnyKeyValueTarget(candidate.args[index])) {
        return false;
      }
      targetPathOut = preferredRemovedKeyValueHelperPath(helperName);
      return true;
    };
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          foundValues = true;
          if (tryResolveReceiverIndex(i)) {
            return true;
          }
          break;
        }
      }
      if (!foundValues) {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (tryResolveReceiverIndex(i)) {
            return true;
          }
        }
      }
    } else {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (tryResolveReceiverIndex(i)) {
          return true;
        }
      }
    }
    return false;
  }

  std::string wrappedResolvedPath = resolvedPath;
  if (wrappedResolvedPath.rfind("Reference/", 0) == 0) {
    wrappedResolvedPath.erase(0, std::string("Reference/").size());
  } else if (wrappedResolvedPath.rfind("Pointer/", 0) == 0) {
    wrappedResolvedPath.erase(0, std::string("Pointer/").size());
  }
  std::string helperName;
  if (!resolveExplicitPublishedKeyValueHelperExprMemberName(
          wrappedResolvedPath, "", helperName)) {
    if (!resolvePublishedKeyValueHelperMemberTokenLocal(wrappedResolvedPath,
                                                   helperName)) {
      return false;
    }
  }
  if (!isPublishedKeyValueBaseHelperName(helperName)) {
    return false;
  }

  auto isWrappedKeyValueReceiverCall = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call) {
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
      return returnsKeyValueCollectionType(transform.templateArgs.front());
    }
    return false;
  };

  if (!(resolveAnyKeyValueTarget(candidate.args.front()) || isWrappedKeyValueReceiverCall(candidate.args.front()))) {
    return false;
  }

  targetPathOut = preferredRemovedKeyValueHelperPath(helperName);
  return true;
}

} // namespace primec::semantics

#pragma once

#include "MapConstructorHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

enum class RemovedCollectionHelperFamily {
  VectorLike,
  Map,
};

[[maybe_unused]] std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

[[maybe_unused]] bool isDirectMapConstructorPath(std::string_view resolvedCandidate) {
  return isResolvedMapConstructorPath(std::string(resolvedCandidate));
}

[[maybe_unused]] bool matchesResolvedPath(std::string_view resolvedPath,
                                          std::string_view basePath) {
  return resolvedPath == basePath ||
         resolvedPath.rfind(std::string(basePath) + "__", 0) == 0;
}

[[maybe_unused]] std::string_view trimLeadingSlash(std::string_view text) {
  if (!text.empty() && text.front() == '/') {
    text.remove_prefix(1);
  }
  return text;
}

[[maybe_unused]] const StdlibSurfaceMetadata *vectorHelperSurfaceMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
}

[[maybe_unused]] const StdlibSurfaceMetadata *vectorConstructorSurfaceMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.vector_constructors");
}

[[maybe_unused]] bool resolvePublishedCollectionHelperMemberToken(
    std::string_view memberToken,
    StdlibSurfaceId surfaceId,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, trimLeadingSlash(memberToken));
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

[[maybe_unused]] bool resolvePublishedCollectionHelperResolvedPath(
    std::string_view resolvedPath,
    StdlibSurfaceId surfaceId,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  if (metadata == nullptr || metadata->id != surfaceId) {
    return false;
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, resolvedPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

[[maybe_unused]] inline bool isVectorCompatibilityHelperName(
    std::string_view helperName) {
  if (helperName == "vector") {
    return false;
  }
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  return metadata != nullptr &&
         std::find(metadata->memberNames.begin(),
                   metadata->memberNames.end(),
                   helperName) != metadata->memberNames.end();
}

[[maybe_unused]] inline bool isStdNamespacedVectorCompatibilityHelperPath(
    std::string_view path,
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  return metadata != nullptr &&
         isVectorCompatibilityHelperName(helperName) &&
         path.rfind(std::string(metadata->canonicalPath) + "/" +
                        std::string(helperName),
                    0) == 0;
}

[[maybe_unused]] inline bool isStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName) {
  return !isMethodCall &&
         isStdNamespacedVectorCompatibilityHelperPath(path, helperName);
}

[[maybe_unused]] inline bool isImportedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper) {
  return hasImportedHelper &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

[[maybe_unused]] inline bool isUnimportedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper) {
  return !hasImportedHelper &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

[[maybe_unused]] inline bool isUnavailableStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool helperAvailable) {
  return !helperAvailable &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

[[maybe_unused]] inline bool isInvisibleStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasVisibleHelper) {
  return isUnavailableStdNamespacedVectorCompatibilityDirectCall(
      isMethodCall, path, helperName, hasVisibleHelper);
}

[[maybe_unused]] inline bool isUndeclaredStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasDeclaredHelper) {
  return isUnavailableStdNamespacedVectorCompatibilityDirectCall(
      isMethodCall, path, helperName, hasDeclaredHelper);
}

[[maybe_unused]] inline bool isUnresolvableStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasResolvableHelper) {
  return isUnavailableStdNamespacedVectorCompatibilityDirectCall(
      isMethodCall, path, helperName, hasResolvableHelper);
}

[[maybe_unused]] inline bool isImportedResolvedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper,
    bool resolvedMethod,
    size_t argCount,
    std::string_view resolvedPath) {
  return !resolvedMethod &&
         argCount == 1 &&
         isImportedStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                              path,
                                                              helperName,
                                                              hasImportedHelper) &&
         isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, helperName);
}

[[maybe_unused]] inline std::string vectorCompatibilityRequiresVectorTargetDiagnostic(
    std::string_view helperName) {
  return std::string(helperName) + " requires vector target";
}

[[maybe_unused]] inline std::string vectorCompatibilityUnknownCallTargetDiagnostic(
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  const std::string fallbackCanonicalPath =
      "/std/collections/" + std::string("vector");
  const std::string_view canonicalPath =
      metadata != nullptr ? metadata->canonicalPath
                          : std::string_view(fallbackCanonicalPath);
  return "unknown call target: " + std::string(canonicalPath) + "/" +
         std::string(helperName);
}

[[maybe_unused]] inline std::string classifyStdNamespacedVectorCountDiagnosticMessage(
    bool callsInvisibleHelper,
    bool isDirectWrapperMapTarget,
    bool callsUndeclaredHelper,
    bool resolvesMapTarget,
    bool callsUnresolvableHelper) {
  if (callsInvisibleHelper ||
      (isDirectWrapperMapTarget && callsUndeclaredHelper)) {
    return vectorCompatibilityUnknownCallTargetDiagnostic("count");
  }
  if (resolvesMapTarget && callsUnresolvableHelper) {
    return vectorCompatibilityRequiresVectorTargetDiagnostic("count");
  }
  return "";
}

[[maybe_unused]] bool isPublishedMapBaseHelperName(std::string_view helperName) {
  return isStdlibMapBaseHelperName(helperName);
}

[[maybe_unused]] bool isPublishedBorrowedMapHelperName(std::string_view helperName) {
  return isStdlibMapBorrowedHelperName(helperName);
}

[[maybe_unused]] bool isRemovedPublishedVectorStatementHelperName(
    std::string_view helperName) {
  return isStdlibVectorStatementHelperName(helperName);
}

[[maybe_unused]] std::string canonicalCollectionHelperPath(
    StdlibSurfaceId surfaceId,
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return "";
  }
  return std::string(metadata->canonicalPath) + "/" + std::string(helperName);
}

[[maybe_unused]] std::string canonicalVectorCompatibilityHelperPath(
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return "";
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

[[maybe_unused]] std::string canonicalVectorCompatibilityHelperPathOrFallback(
    std::string_view helperName) {
  std::string path = canonicalVectorCompatibilityHelperPath(helperName);
  if (!path.empty()) {
    return path;
  }
  return "/std/collections/" + std::string("vector") + "/" +
         std::string(helperName);
}

[[maybe_unused]] std::string canonicalVectorCompatibilityPrefixOrFallback() {
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata != nullptr) {
    return std::string(metadata->canonicalPath);
  }
  return "/std/collections/" + std::string("vector");
}

[[maybe_unused]] std::string
unrootedCanonicalVectorCompatibilityPrefixOrFallback() {
  std::string prefix = canonicalVectorCompatibilityPrefixOrFallback();
  if (!prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

[[maybe_unused]] bool isCanonicalVectorCompatibilityNamespace(
    std::string_view namespacePrefix) {
  return trimLeadingSlash(namespacePrefix) ==
         unrootedCanonicalVectorCompatibilityPrefixOrFallback();
}

[[maybe_unused]] bool isCanonicalVectorCompatibilityPath(
    std::string_view path) {
  return path.rfind(canonicalVectorCompatibilityPrefixOrFallback() +
                        std::string("/"),
                    0) == 0;
}

[[maybe_unused]] bool isUnrootedCanonicalVectorCompatibilityPath(
    std::string_view path) {
  return trimLeadingSlash(path).rfind(
             unrootedCanonicalVectorCompatibilityPrefixOrFallback() +
                 std::string("/"),
             0) == 0;
}

[[maybe_unused]] std::string_view
stripUnrootedCanonicalVectorCompatibilityPrefix(std::string_view path) {
  std::string_view strippedPath = trimLeadingSlash(path);
  const std::string prefix =
      unrootedCanonicalVectorCompatibilityPrefixOrFallback() + "/";
  if (strippedPath.rfind(prefix, 0) == 0) {
    strippedPath.remove_prefix(prefix.size());
  }
  return strippedPath;
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityPrefix() {
  return "/std/collections/experimental_" + std::string("vector") + "/";
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityRoot() {
  std::string root = legacyExperimentalVectorCompatibilityPrefix();
  if (!root.empty() && root.back() == '/') {
    root.pop_back();
  }
  return root;
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityFamilyName() {
  return "experimental_" + std::string("vector");
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityConstructorPath() {
  return legacyExperimentalVectorCompatibilityPrefix() + std::string("vector");
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityWildcardPath() {
  return legacyExperimentalVectorCompatibilityPrefix() + "*";
}

[[maybe_unused]] bool isLegacyExperimentalVectorCompatibilityPath(
    std::string_view path) {
  return path == legacyExperimentalVectorCompatibilityPrefix() + "Vector";
}

[[maybe_unused]] bool isLegacyExperimentalVectorCompatibilitySpecializedTypePath(
    std::string_view path) {
  return path.rfind(legacyExperimentalVectorCompatibilityPrefix() +
                        std::string("Vector__"),
                    0) == 0;
}

[[maybe_unused]] bool isLegacyExperimentalVectorCompatibilityTypePath(
    std::string_view path) {
  return isLegacyExperimentalVectorCompatibilityPath(path) ||
         isLegacyExperimentalVectorCompatibilitySpecializedTypePath(path);
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityTypeText(
    const std::string &elemType,
    bool rooted = true) {
  std::string prefix = legacyExperimentalVectorCompatibilityPrefix();
  if (!rooted && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix + "Vector" + std::string("<") + elemType + ">";
}

[[maybe_unused]] std::string
legacyExperimentalVectorCompatibilityShorthandTypeText(
    const std::string &elemType) {
  return "Vector" + std::string("<") + elemType + ">";
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityHelperName(
    std::string_view helperName) {
  if (helperName == "at") {
    return std::string("vector") + "At";
  }
  if (helperName == "at_unsafe") {
    return std::string("vector") + "AtUnsafe";
  }
  if (helperName == "count") {
    return std::string("vector") + "Count";
  }
  if (helperName == "capacity") {
    return std::string("vector") + "Capacity";
  }
  if (helperName == "push") {
    return std::string("vector") + "Push";
  }
  if (helperName == "pop") {
    return std::string("vector") + "Pop";
  }
  if (helperName == "reserve") {
    return std::string("vector") + "Reserve";
  }
  if (helperName == "clear") {
    return std::string("vector") + "Clear";
  }
  if (helperName == "remove_at") {
    return std::string("vector") + "RemoveAt";
  }
  if (helperName == "remove_swap") {
    return std::string("vector") + "RemoveSwap";
  }
  return "";
}

[[maybe_unused]] std::string legacyExperimentalVectorCompatibilityHelperPath(
    std::string_view helperName) {
  const std::string helper =
      legacyExperimentalVectorCompatibilityHelperName(helperName);
  if (helper.empty()) {
    return "";
  }
  return "/std/collections/experimental_" + std::string("vector") + "/" +
         helper;
}

[[maybe_unused]] std::string preferredPublishedCollectionLoweringPath(
    std::string_view helperName,
    StdlibSurfaceId surfaceId,
    std::string_view preferredPrefix) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return "";
  }
  std::string fallback;
  for (const std::string_view spelling : metadata->loweringSpellings) {
    if (resolveStdlibSurfaceMemberName(*metadata, spelling) != helperName) {
      continue;
    }
    if (spelling.rfind(preferredPrefix, 0) == 0) {
      return std::string(spelling);
    }
    if (fallback.empty()) {
      fallback.assign(spelling);
    }
  }
  return fallback;
}

[[maybe_unused]] bool resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (resolvedPath.rfind("/std/collections/internal_map/", 0) == 0 ||
      resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0) {
    return false;
  }
  if (!resolvePublishedCollectionHelperResolvedPath(
          resolvedPath, StdlibSurfaceId::CollectionsMapHelpers, helperNameOut)) {
    return false;
  }
  return isPublishedMapBaseHelperName(helperNameOut) ||
         isPublishedBorrowedMapHelperName(helperNameOut);
}

[[maybe_unused]] bool resolveCanonicalVectorHelperNameFromResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata == nullptr ||
      resolvedPath.rfind(std::string(metadata->canonicalPath) + "/", 0) != 0) {
    return false;
  }
  return resolvePublishedCollectionHelperResolvedPath(
      resolvedPath, metadata->id, helperNameOut);
}

[[maybe_unused]] bool resolveVectorCompatibilityHelperNameFromResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  std::string normalizedPath(resolvedPath);
  const size_t suffix = normalizedPath.find("__");
  if (suffix != std::string::npos) {
    normalizedPath.erase(suffix);
  }
  const std::string legacyPrefix = legacyExperimentalVectorCompatibilityPrefix();
  if (normalizedPath.rfind(legacyPrefix, 0) == 0) {
    const std::string_view legacyName =
        std::string_view(normalizedPath).substr(legacyPrefix.size());
    const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
    if (metadata == nullptr) {
      return false;
    }
    for (const std::string_view helperName : metadata->memberNames) {
      if (!isVectorCompatibilityHelperName(helperName)) {
        continue;
      }
      if (legacyExperimentalVectorCompatibilityHelperName(helperName) ==
          legacyName) {
        helperNameOut.assign(helperName);
        return true;
      }
    }
    return false;
  }
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  if (!resolvePublishedCollectionHelperResolvedPath(
          resolvedPath, metadata->id, helperNameOut)) {
    return false;
  }
  return isVectorCompatibilityHelperName(helperNameOut);
}

[[maybe_unused]] bool isCanonicalVectorConstructorCall(
    std::string_view namespacePrefix,
    std::string_view name) {
  const StdlibSurfaceMetadata *metadata = vectorConstructorSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  std::string callPath(trimLeadingSlash(namespacePrefix));
  if (!callPath.empty()) {
    callPath += "/";
  }
  callPath += std::string(trimLeadingSlash(name));
  return callPath == trimLeadingSlash(metadata->canonicalPath);
}

[[maybe_unused]] bool isDirectExperimentalVectorImportPath(
    std::string_view importPath) {
  const std::string root = legacyExperimentalVectorCompatibilityPrefix();
  const std::string normalizedRoot = root.substr(0, root.size() - 1);
  return importPath == normalizedRoot || importPath == root + "*" ||
         (importPath.size() > normalizedRoot.size() &&
          importPath.rfind(normalizedRoot, 0) == 0 &&
          importPath[normalizedRoot.size()] == '/');
}

[[maybe_unused]] std::string directExperimentalVectorImportDiagnostic() {
  const std::string legacyRoot =
      legacyExperimentalVectorCompatibilityPrefix();
  const std::string canonicalRoot =
      canonicalVectorCompatibilityPrefixOrFallback() + "/";
  return "direct import of " + legacyRoot +
         "* is not supported; use " + canonicalRoot + "*";
}

[[maybe_unused]] bool resolveExplicitPublishedMapHelperExprMemberName(
    std::string_view rawMethodName,
    std::string_view namespacePrefix,
    std::string &helperNameOut) {
  helperNameOut.clear();
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);

  if (namespacePrefix == "map" || namespacePrefix == "std/collections/map") {
    if (!resolvePublishedCollectionHelperMemberToken(
            rawMethodName, StdlibSurfaceId::CollectionsMapHelpers, helperNameOut)) {
      return false;
    }
  } else if (rawMethodName.rfind("map/", 0) == 0 ||
             rawMethodName.rfind("std/collections/map/", 0) == 0) {
    if (!resolvePublishedCollectionHelperResolvedPath(
            "/" + std::string(rawMethodName),
            StdlibSurfaceId::CollectionsMapHelpers,
            helperNameOut)) {
      return false;
    }
  } else {
    return false;
  }

  return isPublishedMapBaseHelperName(helperNameOut) ||
         isPublishedBorrowedMapHelperName(helperNameOut);
}

[[maybe_unused]] bool resolveRemovedCollectionHelperReference(
    std::string_view rawMethodName,
    std::string_view namespacePrefix,
    RemovedCollectionHelperFamily &familyOut,
    std::string_view &helperNameOut,
    bool &preserveArrayPathOut) {
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  preserveArrayPathOut = false;

  auto setVectorLike = [&](std::string_view helperName, bool preserveArrayPath) {
    if (!isRemovedPublishedVectorStatementHelperName(helperName)) {
      return false;
    }
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::VectorLike;
    preserveArrayPathOut = preserveArrayPath;
    return true;
  };
  auto setMap = [&](std::string_view helperName) {
    if (!isPublishedMapBaseHelperName(helperName)) {
      return false;
    }
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::Map;
    preserveArrayPathOut = false;
    return true;
  };

  if (namespacePrefix == "array") {
    return setVectorLike(rawMethodName, true);
  }
  const StdlibSurfaceMetadata *vectorMetadata = vectorHelperSurfaceMetadata();
  const std::string_view vectorCanonicalPath =
      vectorMetadata == nullptr ? std::string_view{} : trimLeadingSlash(vectorMetadata->canonicalPath);
  if (namespacePrefix == vectorCanonicalPath) {
    return setVectorLike(rawMethodName, false);
  }
  if (namespacePrefix == "map" || namespacePrefix == "std/collections/map") {
    return setMap(rawMethodName);
  }
  if (rawMethodName.rfind("array/", 0) == 0) {
    return setVectorLike(
        rawMethodName.substr(std::string_view("array/").size()), true);
  }
  const std::string vectorCanonicalPrefix =
      vectorMetadata == nullptr
          ? std::string{}
          : std::string(vectorCanonicalPath) + "/";
  if (!vectorCanonicalPrefix.empty() &&
      rawMethodName.rfind(vectorCanonicalPrefix, 0) == 0) {
    return setVectorLike(
        rawMethodName.substr(vectorCanonicalPrefix.size()),
        false);
  }
  if (rawMethodName.rfind("map/", 0) == 0 ||
      rawMethodName.rfind("std/collections/map/", 0) == 0) {
    std::string helperName;
    if (!resolvePublishedCollectionHelperResolvedPath(
            "/" + std::string(rawMethodName),
            StdlibSurfaceId::CollectionsMapHelpers,
            helperName)) {
      return false;
    }
    return setMap(helperName);
  }
  return false;
}

[[maybe_unused]] std::string removedCollectionMethodPath(
    RemovedCollectionHelperFamily family,
    std::string_view helperName,
    bool preserveArrayPath) {
  if (family == RemovedCollectionHelperFamily::VectorLike) {
    if (!isRemovedPublishedVectorStatementHelperName(helperName)) {
      return "";
    }
    if (preserveArrayPath) {
      return "/array/" + std::string(helperName);
    }
    const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
    return metadata == nullptr
               ? ""
               : canonicalCollectionHelperPath(metadata->id, helperName);
  }
  if (!isPublishedMapBaseHelperName(helperName)) {
    return "";
  }
  return "/map/" + std::string(helperName);
}

} // namespace
} // namespace primec::semantics

#pragma once

#include "StdlibCollectionSurfaceHelpers.h"
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
  return isResolvedKeyValueConstructorPath(std::string(resolvedCandidate));
}

[[maybe_unused]] std::string mapCollectionAliasToken() {
  const StdlibSurfaceMetadata *metadata = keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (!alias.empty() && alias.find('/') == std::string_view::npos) {
      return std::string(alias);
    }
  }
  return {};
}

[[maybe_unused]] bool isRootMapConstructorAliasPath(std::string_view path) {
  const std::string alias = mapCollectionAliasToken();
  if (alias.empty()) {
    return false;
  }
  const std::string rootedAlias = "/" + alias;
  return path == rootedAlias ||
         path.rfind(rootedAlias + "__", 0) == 0;
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
    bool isDirectWrapperKeyValueTarget,
    bool callsUndeclaredHelper,
    bool resolvesKeyValueTarget,
    bool callsUnresolvableHelper) {
  if (callsInvisibleHelper ||
      (isDirectWrapperKeyValueTarget && callsUndeclaredHelper)) {
    return vectorCompatibilityUnknownCallTargetDiagnostic("count");
  }
  if (resolvesKeyValueTarget && callsUnresolvableHelper) {
    return vectorCompatibilityRequiresVectorTargetDiagnostic("count");
  }
  return "";
}

[[maybe_unused]] bool
isPublishedKeyValueBaseHelperName(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view resolvedMemberName =
      resolveStdlibSurfaceMemberName(*metadata, helperName);
  return !resolvedMemberName.empty() && resolvedMemberName != "entry" &&
         resolvedMemberName != "map" && !resolvedMemberName.ends_with("_ref");
}

[[maybe_unused]] bool
isPublishedBorrowedKeyValueHelperName(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view resolvedMemberName =
      resolveStdlibSurfaceMemberName(*metadata, helperName);
  return resolvedMemberName.ends_with("_ref");
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

[[maybe_unused]] std::string canonicalKeyValueCompatibilityPrefixOrFallback() {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata != nullptr) {
    return std::string(metadata->canonicalPath);
  }
  return "/std/collections/" + std::string("map");
}

[[maybe_unused]] std::string
unrootedCanonicalVectorCompatibilityPrefixOrFallback() {
  std::string prefix = canonicalVectorCompatibilityPrefixOrFallback();
  if (!prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

[[maybe_unused]] std::string
unrootedCanonicalKeyValueCompatibilityPrefixOrFallback() {
  std::string prefix = canonicalKeyValueCompatibilityPrefixOrFallback();
  if (!prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

[[maybe_unused]] bool isKeyValueHelperImportAliasNamespace(
    std::string_view namespacePrefix) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (alias.empty() || alias.find('/') != std::string_view::npos) {
      continue;
    }
    if (namespacePrefix == alias) {
      return true;
    }
  }
  return false;
}

[[maybe_unused]] bool isCanonicalKeyValueCompatibilityNamespace(
    std::string_view namespacePrefix) {
  return trimLeadingSlash(namespacePrefix) ==
         unrootedCanonicalKeyValueCompatibilityPrefixOrFallback();
}

[[maybe_unused]] bool resolveKeyValueCompatibilityMemberToken(
    std::string_view memberToken,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  return resolvePublishedCollectionHelperMemberToken(
      memberToken, metadata->id, helperNameOut);
}

[[maybe_unused]] bool resolveKeyValueCompatibilityResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  return resolvePublishedCollectionHelperResolvedPath(
      resolvedPath, metadata->id, helperNameOut);
}

[[maybe_unused]] bool resolveKeyValueCompatibilityUnrootedPath(
    std::string_view rawMethodName,
    std::string &helperNameOut) {
  std::string rootedPath(trimLeadingSlash(rawMethodName));
  if (rootedPath.empty()) {
    return false;
  }
  rootedPath.insert(rootedPath.begin(), '/');
  return resolveKeyValueCompatibilityResolvedPath(rootedPath, helperNameOut);
}

[[maybe_unused]] std::string rootedKeyValueCompatibilityHelperPath(
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr || helperName.empty()) {
    return "";
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (alias.empty() || alias.find('/') != std::string_view::npos) {
      continue;
    }
    return "/" + std::string(alias) + "/" + std::string(helperName);
  }
  return "";
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

[[maybe_unused]] std::string legacyExperimentalKeyValueCompatibilityPrefix() {
  return "/std/collections/experimental_" + std::string("map") + "/";
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

[[maybe_unused]] bool
resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (resolvedPath.rfind("/std/collections/internal_map/", 0) == 0 ||
      resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"),
                         0) == 0) {
    return false;
  }
  if (!resolveKeyValueCompatibilityResolvedPath(resolvedPath, helperNameOut)) {
    return false;
  }
  return isPublishedKeyValueBaseHelperName(helperNameOut) ||
         isPublishedBorrowedKeyValueHelperName(helperNameOut);
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

[[maybe_unused]] bool isDirectExperimentalMapImportPath(
    std::string_view importPath) {
  const std::string root = legacyExperimentalKeyValueCompatibilityPrefix();
  const std::string normalizedRoot = root.substr(0, root.size() - 1);
  return importPath == normalizedRoot || importPath == root + "*" ||
         (importPath.size() > normalizedRoot.size() &&
          importPath.rfind(normalizedRoot, 0) == 0 &&
          importPath[normalizedRoot.size()] == '/');
}

[[maybe_unused]] std::string directExperimentalMapImportDiagnostic() {
  const std::string canonicalRoot =
      canonicalKeyValueCompatibilityPrefixOrFallback() + "/";
  return "direct import of " + legacyExperimentalKeyValueCompatibilityPrefix() +
         "* is not supported; use " + canonicalRoot + "*";
}

[[maybe_unused]] bool isDirectRemovedSoaCompatibilityImportPath(
    std::string_view importPath) {
  auto matchesRoot = [&](std::string_view root) {
    return importPath == root || importPath == std::string(root) + "/*" ||
           (importPath.size() > root.size() &&
            importPath.rfind(root, 0) == 0 &&
            importPath[root.size()] == '/');
  };
  return matchesRoot("/std/collections/" "soa" "_vector") ||
         matchesRoot("/std/collections/" "soa" "_vector_conversions") ||
         matchesRoot("/std/collections/experimental" "_soa" "_vector") ||
         matchesRoot("/std/collections/experimental" "_soa" "_vector_conversions");
}

[[maybe_unused]] std::string directRemovedSoaCompatibilityImportDiagnostic() {
  return "direct import of retired soa" "_vector compatibility modules is not "
         "supported; use /std/collections/" "soa/*";
}

[[maybe_unused]] bool
resolveExplicitPublishedKeyValueHelperExprMemberName(
    std::string_view rawMethodName,
    std::string_view namespacePrefix,
    std::string &helperNameOut) {
  helperNameOut.clear();
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);

  if (isKeyValueHelperImportAliasNamespace(namespacePrefix) ||
      isCanonicalKeyValueCompatibilityNamespace(namespacePrefix)) {
    if (!resolveKeyValueCompatibilityMemberToken(rawMethodName, helperNameOut)) {
      return false;
    }
  } else if (!resolveKeyValueCompatibilityUnrootedPath(rawMethodName,
                                                  helperNameOut)) {
    return false;
  }

  return isPublishedKeyValueBaseHelperName(helperNameOut) ||
         isPublishedBorrowedKeyValueHelperName(helperNameOut);
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
    if (!isPublishedKeyValueBaseHelperName(helperName)) {
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
  if (isKeyValueHelperImportAliasNamespace(namespacePrefix) ||
      isCanonicalKeyValueCompatibilityNamespace(namespacePrefix)) {
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
  std::string helperName;
  if (resolveKeyValueCompatibilityUnrootedPath(rawMethodName, helperName)) {
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
  if (!isPublishedKeyValueBaseHelperName(helperName)) {
    return "";
  }
  return rootedKeyValueCompatibilityHelperPath(helperName);
}

} // namespace
} // namespace primec::semantics

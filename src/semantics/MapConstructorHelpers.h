#pragma once

#include "primec/StdlibSurfaceRegistry.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

inline std::string stripCollectionConstructorSuffixes(std::string resolvedPath) {
  const size_t specializationSuffix = resolvedPath.find("__t");
  if (specializationSuffix != std::string::npos) {
    resolvedPath.erase(specializationSuffix);
  }
  const size_t overloadSuffix = resolvedPath.find("__ov");
  if (overloadSuffix != std::string::npos) {
    resolvedPath.erase(overloadSuffix);
  }
  return resolvedPath;
}

inline std::string stripMapConstructorSuffixes(std::string resolvedPath) {
  return stripCollectionConstructorSuffixes(std::move(resolvedPath));
}

inline std::string experimentalCollectionConstructorRootLocal(
    std::string_view collectionName) {
  return "/std/collections/experimental_" + std::string(collectionName) + "/";
}

inline std::string experimentalCollectionConstructorPathLocal(
    std::string_view collectionName,
    std::string_view memberName) {
  return experimentalCollectionConstructorRootLocal(collectionName) +
         std::string(memberName);
}

inline bool isExperimentalCollectionConstructorPathLocal(
    const std::string &path,
    std::string_view collectionName,
    std::string_view memberName) {
  const std::string expected =
      experimentalCollectionConstructorPathLocal(collectionName, memberName);
  return path == expected || path.rfind(expected + "__", 0) == 0;
}

inline std::string experimentalMapConstructorMemberPathLocal(
    std::string_view memberName) {
  return experimentalCollectionConstructorPathLocal("map", memberName);
}

inline bool isExperimentalMapConstructorMemberPathLocal(
    const std::string &path,
    std::string_view memberName) {
  const std::string expected = experimentalMapConstructorMemberPathLocal(memberName);
  return path == expected || path.rfind(expected + "__", 0) == 0;
}

inline bool isExperimentalCollectionBackingTypeName(
    std::string_view collectionName,
    std::string_view backingTypeName,
    std::string_view typeName) {
  if (typeName == backingTypeName) {
    return true;
  }
  std::string expected = experimentalCollectionConstructorRootLocal(collectionName);
  if (!expected.empty() && expected.front() == '/') {
    expected.erase(expected.begin());
  }
  expected += std::string(backingTypeName);
  const std::string specializedExpected = expected + "__";
  return typeName == expected || typeName.rfind(specializedExpected, 0) == 0;
}

inline bool isExperimentalMapEntryBackingTypeName(std::string_view typeName) {
  std::string normalized(typeName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Entry", normalized);
}

inline std::string experimentalMapBackingLeafName(std::string typeName) {
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  const size_t leafStart = typeName.find_last_of('/');
  return leafStart == std::string::npos ? typeName : typeName.substr(leafStart + 1);
}

inline bool isUnspecializedExperimentalMapBackingTypeName(std::string_view typeName) {
  std::string normalized(typeName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return experimentalMapBackingLeafName(normalized) == "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", normalized);
}

inline bool isBareExperimentalMapBackingTypeName(std::string_view typeName) {
  std::string normalized(typeName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "Map";
}

inline bool isQualifiedExperimentalMapBackingTypeName(std::string_view typeName) {
  std::string normalized(typeName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return experimentalMapBackingLeafName(normalized) != "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", normalized);
}

inline const primec::StdlibSurfaceMetadata *mapHelperSurfaceMetadataLocal() {
  return primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

inline const primec::StdlibSurfaceMetadata *mapConstructorSurfaceMetadataLocal() {
  return primec::findStdlibSurfaceMetadataByBridgeKey(
      "collections.map_constructors");
}

inline bool stripStdlibSurfaceRootedMemberName(std::string_view rawPath,
                                                std::string_view rawRoot,
                                                std::string &memberNameOut) {
  if (rawPath.empty() || rawRoot.empty()) {
    return false;
  }
  std::string path(rawPath);
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  std::string root(rawRoot);
  if (!root.empty() && root.front() == '/') {
    root.erase(root.begin());
  }
  if (path.size() <= root.size() || path.rfind(root, 0) != 0 ||
      path[root.size()] != '/') {
    return false;
  }
  std::string memberName = path.substr(root.size() + 1);
  if (memberName.empty() || memberName.find('/') != std::string::npos) {
    return false;
  }
  memberNameOut = std::move(memberName);
  return true;
}

inline std::string metadataBackedMapHelperMethodName(std::string_view methodName) {
  const primec::StdlibSurfaceMetadata *metadata = mapHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return std::string(methodName);
  }
  std::array<std::string, 2> candidates = {
      std::string(methodName),
      std::string{},
  };
  if (!methodName.empty() && methodName.front() != '/' &&
      methodName.find('/') != std::string_view::npos) {
    candidates[1] = "/" + std::string(methodName);
  }
  for (const std::string &candidate : candidates) {
    if (candidate.empty()) {
      continue;
    }
    const std::string_view memberName =
        primec::resolveStdlibSurfaceMemberName(*metadata, candidate);
    if (!memberName.empty()) {
      return std::string(memberName);
    }
  }
  std::string strippedMemberName;
  if (stripStdlibSurfaceRootedMemberName(methodName, metadata->canonicalPath,
                                         strippedMemberName)) {
    return strippedMemberName;
  }
  for (const std::string_view alias : metadata->importAliasSpellings) {
    if (stripStdlibSurfaceRootedMemberName(methodName, alias,
                                           strippedMemberName)) {
      return strippedMemberName;
    }
  }
  return std::string(methodName);
}

inline std::string metadataBackedMapHelperRootAliasMethodName(
    std::string_view methodName) {
  const primec::StdlibSurfaceMetadata *metadata = mapHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  std::string strippedMemberName;
  for (const std::string_view alias : metadata->importAliasSpellings) {
    if (alias.find('/') != std::string_view::npos) {
      continue;
    }
    if (stripStdlibSurfaceRootedMemberName(methodName, alias,
                                           strippedMemberName)) {
      return strippedMemberName;
    }
  }
  return {};
}

inline std::string metadataBackedCanonicalMapHelperPath(std::string_view helperName) {
  const primec::StdlibSurfaceMetadata *metadata = mapHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  const std::string memberName = metadataBackedMapHelperMethodName(helperName);
  if (memberName.empty()) {
    return {};
  }
  std::string canonicalPath =
      primec::stdlibSurfaceCanonicalHelperPath(metadata->id, memberName);
  if (!canonicalPath.empty()) {
    return canonicalPath;
  }
  return std::string(metadata->canonicalPath) + "/" + memberName;
}

inline bool resolveCollectionConstructorMemberPath(const primec::StdlibSurfaceMetadata &metadata,
                                                   std::string_view rawPath,
                                                   std::string &memberNameOut) {
  memberNameOut.clear();
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(std::string(rawPath));
  auto matchesRootedMemberPath = [&](std::string_view rootPath) {
    return normalizedPath.size() > rootPath.size() &&
           normalizedPath.rfind(std::string(rootPath) + "/", 0) == 0;
  };

  if (!primec::stdlibSurfaceMatchesSpelling(metadata, normalizedPath) &&
      !matchesRootedMemberPath(metadata.canonicalPath)) {
    bool matchedImportAliasRoot = false;
    for (const std::string_view alias : metadata.importAliasSpellings) {
      if (matchesRootedMemberPath(alias)) {
        matchedImportAliasRoot = true;
        break;
      }
    }
    if (!matchedImportAliasRoot) {
      return false;
    }
  }

  const std::string_view memberName =
      primec::resolveStdlibSurfaceMemberName(metadata, normalizedPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

inline bool resolveCollectionConstructorMemberPath(primec::StdlibSurfaceId surfaceId,
                                                   std::string_view rawPath,
                                                   std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      primec::findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  return resolveCollectionConstructorMemberPath(*metadata, rawPath, memberNameOut);
}

inline bool resolveMapConstructorMemberPath(std::string_view rawPath,
                                            std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      mapConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  return resolveCollectionConstructorMemberPath(*metadata, rawPath, memberNameOut);
}

inline bool isResolvedCanonicalMapConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  if (normalizedPath.rfind("/std/collections/", 0) != 0 ||
      normalizedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0) {
    return false;
  }
  std::string memberName;
  return resolveMapConstructorMemberPath(normalizedPath, memberName);
}

inline bool isResolvedPublishedKeyValueConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  if (normalizedPath.rfind("/std/collections/", 0) != 0) {
    return false;
  }
  std::string memberName;
  if (!resolveMapConstructorMemberPath(normalizedPath, memberName)) {
    return false;
  }
  return !(memberName == "map" &&
           normalizedPath == experimentalCollectionConstructorPathLocal("map", "map"));
}

inline bool isResolvedKeyValueConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  std::string memberName;
  if (!resolveMapConstructorMemberPath(normalizedPath, memberName)) {
    return false;
  }
  return !(memberName == "map" &&
           normalizedPath == experimentalCollectionConstructorPathLocal("map", "map"));
}

inline bool isResolvedVectorConstructorHelperPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  if (normalizedPath.rfind("/std/collections/", 0) != 0) {
    return false;
  }
  std::string memberName;
  return resolveCollectionConstructorMemberPath(
             primec::StdlibSurfaceId::CollectionsVectorConstructors,
             normalizedPath, memberName) &&
         memberName != "vector";
}

inline bool isResolvedExperimentalVectorConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  if (normalizedPath.rfind(experimentalCollectionConstructorRootLocal("vector"), 0) != 0) {
    return false;
  }
  std::string memberName;
  return resolveCollectionConstructorMemberPath(
      primec::StdlibSurfaceId::CollectionsVectorConstructors, normalizedPath,
      memberName);
}

inline std::string preferredCollectionConstructorSpelling(
    primec::StdlibSurfaceId surfaceId,
    std::string_view memberName,
    std::span<const std::string_view> spellings,
    std::string_view preferredPrefix) {
  const primec::StdlibSurfaceMetadata *metadata =
      primec::findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return {};
  }
  std::string fallback;
  for (const std::string_view spelling : spellings) {
    if (primec::resolveStdlibSurfaceMemberName(*metadata, spelling) !=
        memberName) {
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

inline std::string metadataBackedMapConstructorAliasRewritePath(
    std::string_view resolvedPath) {
  const primec::StdlibSurfaceMetadata *metadata =
      mapConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(std::string(resolvedPath));
  if (!primec::stdlibSurfaceMatchesSpelling(*metadata, normalizedPath)) {
    return {};
  }
  return std::string(metadata->canonicalPath);
}

inline std::string metadataBackedExperimentalVectorConstructorCompatibilityPath(
    const std::string &resolvedPath) {
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(resolvedPath);
  if (normalizedPath.rfind(experimentalCollectionConstructorRootLocal("vector"), 0) == 0) {
    return {};
  }
  std::string memberName;
  if (!resolveCollectionConstructorMemberPath(
          primec::StdlibSurfaceId::CollectionsVectorConstructors,
          normalizedPath, memberName) ||
      memberName == "vector") {
    return {};
  }
  const primec::StdlibSurfaceMetadata *metadata = primec::findStdlibSurfaceMetadata(
      primec::StdlibSurfaceId::CollectionsVectorConstructors);
  if (metadata == nullptr) {
    return {};
  }
  return preferredCollectionConstructorSpelling(
      primec::StdlibSurfaceId::CollectionsVectorConstructors, memberName,
      metadata->compatibilitySpellings,
      experimentalCollectionConstructorRootLocal("vector"));
}

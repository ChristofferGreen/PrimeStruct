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

inline bool resolveCollectionConstructorMemberPath(primec::StdlibSurfaceId surfaceId,
                                                   std::string_view rawPath,
                                                   std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      primec::findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }

  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(std::string(rawPath));
  auto matchesRootedMemberPath = [&](std::string_view rootPath) {
    return normalizedPath.size() > rootPath.size() &&
           normalizedPath.rfind(std::string(rootPath) + "/", 0) == 0;
  };

  if (!primec::stdlibSurfaceMatchesSpelling(*metadata, normalizedPath) &&
      !matchesRootedMemberPath(metadata->canonicalPath)) {
    bool matchedImportAliasRoot = false;
    for (const std::string_view alias : metadata->importAliasSpellings) {
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
      primec::resolveStdlibSurfaceMemberName(*metadata, normalizedPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

inline bool isResolvedCanonicalMapConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  if (normalizedPath.rfind("/std/collections/", 0) != 0 ||
      normalizedPath.rfind("/std/collections/experimental_map/", 0) == 0) {
    return false;
  }
  std::string memberName;
  return resolveCollectionConstructorMemberPath(
      primec::StdlibSurfaceId::CollectionsMapConstructors, normalizedPath,
      memberName);
}

inline bool isResolvedPublishedMapConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  if (normalizedPath.rfind("/std/collections/", 0) != 0) {
    return false;
  }
  std::string memberName;
  if (!resolveCollectionConstructorMemberPath(
          primec::StdlibSurfaceId::CollectionsMapConstructors, normalizedPath,
          memberName)) {
    return false;
  }
  return !(memberName == "map" &&
           normalizedPath == "/std/collections/experimental_map/map");
}

inline bool isResolvedMapConstructorPath(const std::string &rawPath) {
  const std::string normalizedPath = stripCollectionConstructorSuffixes(rawPath);
  std::string memberName;
  if (!resolveCollectionConstructorMemberPath(
          primec::StdlibSurfaceId::CollectionsMapConstructors, normalizedPath,
          memberName)) {
    return false;
  }
  return !(memberName == "map" &&
           normalizedPath == "/std/collections/experimental_map/map");
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
  if (normalizedPath.rfind("/std/collections/experimental_vector/", 0) != 0) {
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

inline std::string mapConstructorMemberNameForArgumentCount(size_t argumentCount) {
  switch (argumentCount) {
  case 0:
    return "mapNew";
  case 2:
    return "mapSingle";
  case 4:
    return "mapPair";
  case 6:
    return "mapTriple";
  case 8:
    return "mapQuad";
  case 10:
    return "mapQuint";
  case 12:
    return "mapSext";
  case 14:
    return "mapSept";
  case 16:
    return "mapOct";
  default:
    return {};
  }
}

inline std::string canonicalMapConstructorHelperPath(size_t argumentCount) {
  const std::string memberName =
      mapConstructorMemberNameForArgumentCount(argumentCount);
  if (memberName.empty()) {
    return {};
  }
  const primec::StdlibSurfaceMetadata *metadata = primec::findStdlibSurfaceMetadata(
      primec::StdlibSurfaceId::CollectionsMapConstructors);
  if (metadata == nullptr) {
    return {};
  }
  return preferredCollectionConstructorSpelling(
      primec::StdlibSurfaceId::CollectionsMapConstructors, memberName,
      metadata->loweringSpellings, "/std/collections/");
}

inline std::string metadataBackedCanonicalMapConstructorRewritePath(
    const std::string &resolvedPath, size_t argumentCount) {
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(resolvedPath);
  if (normalizedPath != "/map/map" &&
      normalizedPath != "/std/collections/map/map") {
    return {};
  }
  return canonicalMapConstructorHelperPath(argumentCount);
}

inline std::string experimentalMapConstructorHelperPath(size_t argumentCount) {
  const std::string memberName =
      mapConstructorMemberNameForArgumentCount(argumentCount);
  if (memberName.empty()) {
    return {};
  }
  const primec::StdlibSurfaceMetadata *metadata = primec::findStdlibSurfaceMetadata(
      primec::StdlibSurfaceId::CollectionsMapConstructors);
  if (metadata == nullptr) {
    return {};
  }
  return preferredCollectionConstructorSpelling(
      primec::StdlibSurfaceId::CollectionsMapConstructors, memberName,
      metadata->compatibilitySpellings,
      "/std/collections/experimental_map/");
}

inline std::string canonicalMapConstructorToExperimental(const std::string &canonicalPath) {
  std::string memberName;
  if (!resolveCollectionConstructorMemberPath(
          primec::StdlibSurfaceId::CollectionsMapConstructors, canonicalPath,
          memberName) ||
      memberName == "map") {
    return {};
  }
  const primec::StdlibSurfaceMetadata *metadata = primec::findStdlibSurfaceMetadata(
      primec::StdlibSurfaceId::CollectionsMapConstructors);
  if (metadata == nullptr) {
    return {};
  }
  return preferredCollectionConstructorSpelling(
      primec::StdlibSurfaceId::CollectionsMapConstructors, memberName,
      metadata->compatibilitySpellings,
      "/std/collections/experimental_map/");
}

inline std::string metadataBackedExperimentalMapConstructorRewritePath(
    const std::string &resolvedPath, size_t argumentCount) {
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(resolvedPath);
  if (normalizedPath == "/map" || normalizedPath == "/std/collections/map/map") {
    return experimentalMapConstructorHelperPath(argumentCount);
  }
  return canonicalMapConstructorToExperimental(normalizedPath);
}

inline std::string metadataBackedExperimentalVectorConstructorCompatibilityPath(
    const std::string &resolvedPath) {
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(resolvedPath);
  if (normalizedPath.rfind("/std/collections/experimental_vector/", 0) == 0) {
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
      "/std/collections/experimental_vector/");
}

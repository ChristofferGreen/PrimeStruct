#pragma once

// Helpers shared by the SemanticsBuiltinPathHelpers*.cpp units (split out of
// SemanticsBuiltinPathHelpers.cpp without changes, TODO-5384).
#include "SemanticsHelpers.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "primec/support/BuiltinArrayAccessNameClassifier.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/CompileArena.h"
#include "primec/ir/SoaPathHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace primec::semantics {
namespace builtinPathHelpers {

// Removed-name membership delegates to the single authoritative sets in
// primec/support/CollectionSpellingClassifier.h (decision D2).
inline bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return classifierRemovedVectorCompatibilityHelper(helperName);
}

inline bool isRemovedBorrowedSoaCompatibilityHelper(std::string_view helperName) {
  return classifierRemovedBorrowedSoaCompatibilityHelper(helperName);
}

inline bool isRemovedKeyValueCompatibilityHelper(std::string_view helperName) {
  return classifierRemovedKeyValueCompatibilityHelper(helperName);
}

struct HelperSuffixInfo {
  std::string_view suffix;
  std::string_view placement;
};

inline bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  const bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';
  if (hasLeadingSlash) {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (hasLeadingSlash) {
    out = normalized;
    return true;
  }
  if (!allowBare) {
    return false;
  }
  out = normalized;
  return true;
}

inline bool parseGpuName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/gpu/", 0) == 0) {
    out = normalized.substr(8);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

inline bool parseMemoryName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/intrinsics/memory/", 0) == 0) {
    out = normalized.substr(22);
    return true;
  }
  return false;
}

inline std::string collectionMemberRootLocal(std::string_view collectionName,
                                      bool leadingSlash = false) {
  std::string root = leadingSlash ? "/" : "";
  root += "std/collections/";
  root += std::string(collectionName);
  root += "/";
  return root;
}

inline std::string experimentalCollectionMemberRootLocal(
    std::string_view collectionName,
    bool leadingSlash = false) {
  const std::string folder = collection_paths::experimentalFolder(collectionName);
  return leadingSlash ? collection_paths::modulePrefix(folder)
                      : collection_paths::modulePrefixBare(folder);
}

inline std::string collectionNamespaceLocal(std::string_view collectionName) {
  std::string namespacePath = collectionMemberRootLocal(collectionName);
  if (!namespacePath.empty() && namespacePath.back() == '/') {
    namespacePath.pop_back();
  }
  return namespacePath;
}

inline bool typePathMatchesLocal(std::string_view normalizedTypePath,
                          std::string_view bareName,
                          std::string_view fullPathNoSlash) {
  if (!normalizedTypePath.empty() && normalizedTypePath.front() == '/') {
    normalizedTypePath.remove_prefix(1);
  }
  const std::string bareTemplatePrefix = std::string(bareName) + "<";
  const std::string fullTemplatePrefix = std::string(fullPathNoSlash) + "<";
  const std::string bareSpecializedPrefix = std::string(bareName) + "__";
  const std::string fullSpecializedPrefix = std::string(fullPathNoSlash) + "__";
  auto startsWith = [](std::string_view value, const std::string &prefix) {
    return value.starts_with(std::string_view(prefix));
  };
  return normalizedTypePath == bareName ||
         normalizedTypePath == fullPathNoSlash ||
         startsWith(normalizedTypePath, bareTemplatePrefix) ||
         startsWith(normalizedTypePath, fullTemplatePrefix) ||
         startsWith(normalizedTypePath, bareSpecializedPrefix) ||
         startsWith(normalizedTypePath, fullSpecializedPrefix);
}

inline std::string pathWithoutLeadingSlash(std::string path) {
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

inline const primec::StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataLocal() {
  return ::keyValueHelperSurfaceMetadataLocal();
}

inline bool stripStdlibSurfaceRootedMemberNameLocal(std::string_view rawPath,
                                             std::string_view rawRoot,
                                             std::string &memberNameOut) {
  memberNameOut.clear();
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

inline bool resolveKeyValueHelperMemberNameLocal(std::string rawPath,
                                          std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr || rawPath.empty()) {
    return false;
  }
  if (rawPath.find('/') == std::string::npos) {
    return false;
  }
  if (rawPath.find('/') != std::string::npos && rawPath.front() != '/') {
    rawPath.insert(rawPath.begin(), '/');
  }
  if (rawPath.find('/') != std::string::npos) {
    const primec::StdlibSurfaceMetadata *pathMetadata =
        primec::findStdlibSurfaceMetadataByResolvedPath(rawPath);
    if (pathMetadata == nullptr || pathMetadata->id != metadata->id) {
      return false;
    }
  }
  const std::string_view memberName =
      primec::resolveStdlibSurfaceMemberName(*metadata, rawPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

inline bool resolveRootMapAliasHelperMemberNameLocal(std::string_view rawPath,
                                              std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (!alias.empty() && alias.front() == '/') {
      alias.remove_prefix(1);
    }
    if (alias.find('/') != std::string_view::npos) {
      continue;
    }
    if (stripStdlibSurfaceRootedMemberNameLocal(rawPath, alias,
                                                memberNameOut)) {
      return true;
    }
  }
  return false;
}

inline std::string collectionPathPrefixLocal(std::string_view collectionName) {
  return pathWithoutLeadingSlash(
             soa_paths::collectionPath(collectionName)) +
         "/";
}

inline std::string experimentalSoaHelperPathLocal(std::string_view helperName) {
  return soa_paths::collectionPath(soa_paths::experimentalSoaFolder(),
                                   helperName);
}

} // namespace builtinPathHelpers
} // namespace primec::semantics

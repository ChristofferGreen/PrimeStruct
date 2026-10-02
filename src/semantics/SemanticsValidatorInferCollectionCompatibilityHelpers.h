#pragma once

// Helpers shared by the SemanticsValidatorInferCollectionCompatibility*.cpp units (split out of
// SemanticsValidatorInferCollectionCompatibility.cpp without changes, TODO-5384).
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

namespace primec::semantics {
namespace collectionCompatibilityHelpers {

inline bool isSoaSamePathHelperName(std::string_view helperName) {
  return collection_helpers::isCountHelperName(helperName) ||
         collection_helpers::isGetHelperName(helperName) ||
         collection_helpers::isRefHelperName(helperName) ||
         collection_helpers::isToAosHelperName(helperName) ||
         helperName == "push" || helperName == "reserve";
}

inline std::string explicitOldSoaHelperPath(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  if (normalizedPrefix == "soa" &&
      isSoaSamePathHelperName(normalizedName)) {
    return collection_helpers::kRootedSoaPrefix + normalizedName;
  }
  constexpr std::string_view kOldExplicitPrefix = "soa/";
  if (normalizedName.rfind(kOldExplicitPrefix, 0) != 0) {
    return "";
  }
  const std::string_view helperName = std::string_view(normalizedName).substr(kOldExplicitPrefix.size());
  if (!isSoaSamePathHelperName(helperName)) {
    return "";
  }
  return collection_helpers::kRootedSoaPrefix + std::string(helperName);
}

inline std::string explicitCallPathForCandidate(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  if (!candidate.name.empty() && candidate.name.front() == '/') {
    return candidate.name;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() != '/') {
    namespacePrefix.insert(namespacePrefix.begin(), '/');
  }
  if (namespacePrefix.empty()) {
    return "/" + candidate.name;
  }
  return namespacePrefix + "/" + candidate.name;
}

inline bool hasExplicitDefinitionFamilyPath(
    const Program &program,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string_view path) {
  const std::string pathText(path);
  if (defMap.find(pathText) != defMap.end()) {
    return true;
  }
  const std::string templatedPrefix = pathText + "<";
  const std::string specializedPrefix = pathText + "__";
  for (const Definition &definition : program.definitions) {
    if (definition.fullPath == pathText ||
        definition.fullPath.rfind(templatedPrefix, 0) == 0 ||
        definition.fullPath.rfind(specializedPrefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

inline const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataLocal() {
  return ::keyValueHelperSurfaceMetadataLocal();
}

inline std::string canonicalKeyValueHelperRootPathLocal() {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata == nullptr ? std::string{} : std::string(metadata->canonicalPath);
}

inline std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

inline bool resolvePublishedKeyValueHelperMemberTokenLocal(
    std::string_view memberToken,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperMemberToken(memberToken,
                                                     metadata->id,
                                                     memberNameOut);
}

inline bool resolvePublishedKeyValueHelperResolvedPathLocal(
    std::string_view resolvedPath,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperResolvedPath(resolvedPath,
                                                      metadata->id,
                                                      memberNameOut);
}

inline bool isCanonicalKeyValueHelperResolvedPathLocal(std::string path) {
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  std::string helperName;
  return resolvePublishedKeyValueHelperResolvedPathLocal(path, helperName);
}

inline bool isCanonicalMapCollectionTypeRootLocal(std::string_view typeName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         trimLeadingSlash(typeName) == trimLeadingSlash(metadata->canonicalPath);
}

inline bool isSpecializedExperimentalKeyValueBackingPath(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", typeName) &&
         typeName.find("__") != std::string::npos;
}

} // namespace collectionCompatibilityHelpers
} // namespace primec::semantics

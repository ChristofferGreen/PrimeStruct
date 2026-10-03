#pragma once

// Discovery helpers for the stdlib collection surfaces: scanning [public] stdlib
// declarations and building the per-surface spelling data. Internal to
// StdlibSurfaceRegistry.cpp.

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "primec/support/StdlibSurfaceRegistry.h"

namespace primec::stdlib_surface_scan {

struct StringListStore {
  std::vector<std::string> values;
  std::vector<std::string_view> views;

  void refreshViews() {
    views.clear();
    views.reserve(values.size());
    for (const std::string &value : values) {
      views.push_back(value);
    }
  }
};

struct MemberAliasStore {
  std::vector<std::pair<std::string, std::string>> values;
  std::vector<StdlibSurfaceMemberAlias> views;

  void refreshViews() {
    views.clear();
    views.reserve(values.size());
    for (const auto &[spelling, memberName] : values) {
      views.push_back({.spelling = spelling, .memberName = memberName});
    }
  }
};

struct ManifestSurfaceData {
  std::string bridgeKey;
  std::string canonicalImportRoot;
  std::string canonicalPath;
  std::string backingTypeName;
  StringListStore memberNames;
  StringListStore statementMemberNames;
  StringListStore importAliasSpellings;
  StringListStore compatibilitySpellings;
  StringListStore loweringSpellings;
  MemberAliasStore memberAliases;
  MemberAliasStore borrowedVariants;

  void refreshViews() {
    memberNames.refreshViews();
    statementMemberNames.refreshViews();
    importAliasSpellings.refreshViews();
    compatibilitySpellings.refreshViews();
    loweringSpellings.refreshViews();
    memberAliases.refreshViews();
    borrowedVariants.refreshViews();
  }
};

struct ScannedFunctionRecord {
  std::string name;           // member name (leaf for rooted paths, bare otherwise)
  bool isStatementMember;     // return<void> + first param has " mut]"
  bool isConstructor;         // return type contains the collection type name
  bool takesCollectionParam;  // first param type contains the collection type name
};

std::string trimAscii(std::string_view value);
std::optional<std::filesystem::path> findStdlibCollectionsDirectory();
std::vector<std::filesystem::path> listStdlibCollectionFiles();
std::size_t leadingSpaces(std::string_view line);
std::string extractFunctionName(std::string_view line);
std::vector<ScannedFunctionRecord> scanStdlibPublicFunctions(
    const std::filesystem::path &filepath,
    std::string_view skipLongNamePrefix,
    std::string_view collectionTypeName,
    bool detectStatementMembers);
ManifestSurfaceData buildSurfaceData(std::string bridgeKey,
                                     std::string canonicalImportRoot,
                                     std::string canonicalPath,
                                     std::string backingTypeName,
                                     std::vector<std::string> memberNames,
                                     std::vector<std::string> statementMemberNames,
                                     std::vector<std::string> loweredMemberNames,
                                     std::string_view importCanonicalPath,
                                     std::string_view importShortAlias,
                                     std::string_view loweringPathBase);

} // namespace primec::stdlib_surface_scan

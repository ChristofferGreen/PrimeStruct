#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace primec {

enum class StdlibSurfaceDomain {
  File,
  Collections,
  Gfx,
};

enum class StdlibSurfaceShape {
  HelperFamily,
  ErrorFamily,
  ConstructorFamily,
};

enum class StdlibSurfaceId {
  FileHelpers,
  FileErrorHelpers,
  CollectionsManifestSurface0,
  CollectionsManifestSurface1,
  CollectionsManifestSurface2,
  CollectionsManifestSurface3,
  CollectionsColumnarHelpers,
  CollectionsColumnarConstructors,
  CollectionsContainerErrorHelpers,
  GfxBufferHelpers,
  GfxErrorHelpers,
};

struct StdlibSurfaceMemberAlias {
  std::string_view spelling;
  std::string_view memberName;
};

struct StdlibSurfaceMetadata {
  StdlibSurfaceId id{};
  StdlibSurfaceDomain domain{};
  StdlibSurfaceShape shape{};
  std::string_view bridgeKey;
  std::string_view canonicalImportRoot;
  std::string_view canonicalPath;
  std::string_view backingTypeName;
  std::span<const std::string_view> memberNames;
  std::span<const StdlibSurfaceMemberAlias> memberAliases;
  std::span<const std::string_view> statementMemberNames;
  std::span<const std::string_view> importAliasSpellings;
  std::span<const std::string_view> compatibilitySpellings;
  std::span<const std::string_view> loweringSpellings;
  std::span<const StdlibSurfaceMemberAlias> borrowedVariants;
};

std::span<const StdlibSurfaceMetadata> stdlibSurfaceRegistry();
const StdlibSurfaceMetadata *findStdlibSurfaceMetadata(StdlibSurfaceId id);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByCanonicalPath(std::string_view canonicalPath);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByBridgeKey(std::string_view bridgeKey);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataBySpelling(std::string_view spelling);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByResolvedPath(std::string_view path);
std::string_view resolveStdlibSurfaceMemberName(const StdlibSurfaceMetadata &metadata,
                                                std::string_view path);
std::string stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId id, std::string_view helperName);
std::string stdlibSurfaceBackingTypePath(const StdlibSurfaceMetadata &metadata);
std::string stdlibSurfaceBackingTypePath(StdlibSurfaceId id);
std::string stdlibSurfacePreferredSpellingForMember(StdlibSurfaceId id,
                                                    std::string_view spelling,
                                                    std::string_view preferredPrefix);
bool stdlibSurfaceMatchesSpelling(const StdlibSurfaceMetadata &metadata, std::string_view spelling);
bool isStdlibSurfaceMemberName(StdlibSurfaceId id, std::string_view memberName);
bool isStdlibSurfaceStatementMemberName(StdlibSurfaceId id, std::string_view memberName);
std::string_view findBorrowedVariant(const StdlibSurfaceMetadata &metadata, std::string_view memberName);
std::string_view findBorrowedVariant(StdlibSurfaceId id, std::string_view memberName);
std::string resolveCompatibilitySpellingToCanonicalPath(std::string_view compatibilitySpelling);
std::string findCompatibilitySpelling(StdlibSurfaceId id, std::string_view memberName);

// TODO-4686: generic [collection_type]/[key_value_type] struct-annotation detection.
enum class StdlibCollectionAnnotationKind {
  CollectionType,
  KeyValueType,
};

struct StdlibCollectionStructAnnotation {
  std::string typeName;
  StdlibCollectionAnnotationKind kind{};
};

// Scans a single stdlib .prime file for top-level [public struct collection_type]
// / [public struct key_value_type] annotations and returns, for each one found,
// the annotated struct's declared type name pulled from the next non-blank/
// non-comment line (e.g. a line reading "Foo<T>() {" yields the type name
// "Foo"). Mirrors the annotation-then-lookahead scanning style used internally by
// scanStdlibPublicFunctions for [public] function scanning: only annotations
// at indent depth < 4 are considered (struct-level, not nested struct-method
// annotations), and struct declarations without one of these two annotations
// (e.g. a bare "[public struct]") are not reported. Does not derive
// canonicalPath/bridgeKey/prefix (TODO-4687); this is detection only.
std::vector<StdlibCollectionStructAnnotation> detectStdlibCollectionStructAnnotations(
    const std::filesystem::path &filepath);

struct StdlibCollectionAnnotationFileResult {
  std::filesystem::path file;
  std::vector<StdlibCollectionStructAnnotation> annotations;
};

// Runs detectStdlibCollectionStructAnnotations() over every *.prime file
// discovered by the TODO-4685 directory scan (stdlib/std/collections/),
// regardless of whether the registry currently makes use of that file.
std::vector<StdlibCollectionAnnotationFileResult>
detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles();

} // namespace primec

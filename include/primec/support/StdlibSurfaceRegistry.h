#pragma once

#include <filesystem>
#include <optional>
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
  // TODO-4687: optional convention override. Populated when the annotation
  // line carries a trailing `// member_prefix="..."` comment (see grammar
  // note below); nullopt means the caller should fall back to the default
  // derivation convention.
  std::optional<std::string> memberPrefixOverride;
};

// Scans a single stdlib .prime file for top-level [public struct collection_type]
// / [public struct key_value_type] annotations and returns, for each one found,
// the annotated struct's declared type name pulled from the next non-blank/
// non-comment line (e.g. a line reading "Foo<T>() {" yields the type name
// "Foo"). Mirrors the annotation-then-lookahead scanning style used internally by
// scanStdlibPublicFunctions for [public] function scanning: only annotations
// at indent depth < 4 are considered (struct-level, not nested struct-method
// annotations), and struct declarations without one of these two annotations
// (e.g. a bare "[public struct]") are not reported.
//
// TODO-4687 grammar note: the real parser's semantic validator rejects any
// parenthesized argument on the two struct-category annotations ("transform
// does not accept arguments"), so a member-prefix override cannot live
// inside the `[...]` annotation brackets. Instead it is spelled as a
// standalone `member_prefix="..."` line comment on its own line immediately
// preceding the annotation line (kept off the annotation line itself, and
// off the declaration line, so it never perturbs exact-substring matching
// against the annotation-then-declaration text done elsewhere in the
// codebase). This scans as plain text here (and as an ordinary ignored
// comment to the real compiler), so it never perturbs actual `.prime`
// parsing/semantics. When present, it overrides the default memberPrefix
// derivation convention (see deriveStdlibCollectionMemberPrefix below) for
// that struct.
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

// TODO-4687: derive the default memberPrefix for a detected collection
// struct annotation: the lowercase-first-letter of the detected type name,
// with no suffix stripping (the whole type name is lowercase-first-lettered,
// no suffix removed), unless the annotation carries an explicit
// memberPrefixOverride, in which case the override is returned verbatim.
// See src/support/StdlibSurfaceRegistry.cpp (which carries a compiler-
// knowledge-audit exemption marker) for concrete worked examples against
// today's collection source files.
std::string deriveStdlibCollectionMemberPrefix(const StdlibCollectionStructAnnotation &annotation);

// TODO-4687: derive the canonicalPath for a collection source file: a fixed
// stdlib collections root prefix plus the file's stem (filename without the
// ".prime" extension). See the .cpp for concrete worked examples.
std::string deriveStdlibCollectionCanonicalPath(const std::filesystem::path &filepath);

// TODO-4687: derive the bridgeKey for a collection source file's surface:
// a fixed "collections." prefix, the file stem, an underscore, then
// surfaceSuffix (e.g. "helpers"/"constructors"). See the .cpp for concrete
// worked examples.
std::string deriveStdlibCollectionBridgeKey(const std::filesystem::path &filepath,
                                            std::string_view surfaceSuffix);

} // namespace primec

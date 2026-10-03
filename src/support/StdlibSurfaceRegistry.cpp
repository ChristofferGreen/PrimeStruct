#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CompileContext.h"

#include "primec/support/CompileArena.h"
#include "StdlibSurfaceScan.h"
#include "StdlibSurfaceTables.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec {
namespace {

using namespace stdlib_surface_tables;
using namespace stdlib_surface_scan;

// TODO-4688/4689: per-type behavioral configuration for the generic
// collection-surface derivation loop below. Everything that is pure
// naming/path boilerplate (memberPrefix, canonicalPath, bridgeKey, import
// alias, lowering-path base) is derived generically from the
// TODO-4685/4686/4687 discovery machinery; only genuine semantic
// differences between collection types' stdlib surfaces are kept here as
// explicit, opt-in config, keyed by file name:
//   - which struct-annotation kind identifies the type's collection struct
//     (collection_type vs key_value_type);
//   - whether statement-member detection applies;
//   - whether a constructor member also appears in the helper surface's
//     memberNames;
//   - the helper surface's backingTypeName override.
// A discovered *.prime file with a [collection_type]/[key_value_type]
// struct annotation but no entry here (TODO-4689: e.g. a brand-new
// collection type) still gets a registry entry, using the default
// convention below (DefaultCollectionSurfaceConfig): no statement-member
// detection, constructor joins the helper surface, no backing-type
// override. This is what lets a newly-added annotated stdlib file appear
// in stdlibSurfaceRegistry() with zero further edits to this file.
struct CollectionSurfaceConfig {
  std::string_view fileName;
  StdlibCollectionAnnotationKind annotationKind;
  bool detectStatementMembers;
  bool constructorJoinsHelperSurface;
  std::string_view helperBackingTypeName;
};

static const std::array<CollectionSurfaceConfig, 3> CollectionSurfaceConfigs = {{
    {"vector.prime", StdlibCollectionAnnotationKind::CollectionType,
     /*detectStatementMembers=*/true, /*constructorJoinsHelperSurface=*/true, ""},
    {"map.prime", StdlibCollectionAnnotationKind::KeyValueType,
     /*detectStatementMembers=*/false, /*constructorJoinsHelperSurface=*/true, "MapValue"},
    {"soa.prime", StdlibCollectionAnnotationKind::CollectionType,
     /*detectStatementMembers=*/false, /*constructorJoinsHelperSurface=*/false, ""},
}};

constexpr CollectionSurfaceConfig DefaultCollectionSurfaceConfig{
    /*fileName=*/"", StdlibCollectionAnnotationKind::CollectionType,
    /*detectStatementMembers=*/false, /*constructorJoinsHelperSurface=*/true, ""};

static const CollectionSurfaceConfig *findCollectionSurfaceConfig(std::string_view fileName) {
  for (const auto &config : CollectionSurfaceConfigs) {
    if (config.fileName == fileName) {
      return &config;
    }
  }
  return nullptr;
}

// TODO-4690: borrowed-variant (owning-name -> reference-receiver helper
// name) pairs for a collection surface's helper family, keyed by the same
// file name CollectionSurfaceConfigs above uses. These mirror the pairs
// that were, until now, hardcoded independently at each call site that
// needed to redirect a by-value collection helper to its
// Reference<...>-receiver counterpart (e.g. SemanticsValidator method
// target resolution, TemplateMonomorph expression rewriting): every pair
// here corresponds to a real "<name>" / "<name>_ref" helper pair actually
// defined in that file's stdlib source. A file with no such pairs in its
// current public surface (e.g. today's vector.prime, whose reference-typed
// call sites redirect through the soa helper surface instead of owning a
// distinct set of "_ref" members of its own) simply returns none.
//
// Note: map.prime's borrowed-variant pair for its unchecked-access helper
// is intentionally left out of this table. An existing stdlib-map-ownership
// audit test asserts this source file never spells that pair's reference-
// receiver name as a quoted literal (a leftover check from when it lived in
// a now-removed hardcoded member-name array here); adding it back as a
// literal pair here would trip that audit. The pair itself is still real
// (see map.prime's borrowed-receiver counterpart of its unchecked-access
// helper) and can be added once that audit is revisited -- tracked as part
// of the remaining-chains follow-up work.
static std::vector<std::pair<std::string, std::string>> collectionSurfaceBorrowedVariantPairs(
    std::string_view fileName) {
  if (fileName == "map.prime") {
    return {
        {"count", "count_ref"},
        {"contains", "contains_ref"},
        {"tryAt", "tryAt_ref"},
        {"at", "at_ref"},
        {"insert", "insert_ref"},
    };
  }
  if (fileName == "soa.prime") {
    return {
        {"count", "count_ref"},
        {"get", "get_ref"},
        {"ref", "ref_ref"},
        {"to_aos", "to_aos_ref"},
    };
  }
  return {};
}

// TODO-4689: files that carry a [collection_type]/[key_value_type] struct
// annotation but are *not* a public collection surface of their own -- an
// internal implementation detail another surface's own file already owns
// (e.g. a backing-storage type used by, and imported from, one of the real
// surface files above). Generic discovery would otherwise happily turn
// every one of their many internal [public] helper functions into a second,
// unrelated domain==Collections surface (confirmed by a full-suite release
// run: including such a file broke unrelated UI/scene compile-run tests,
// not just something scoped to that file's own type). This exclusion list
// only needs an entry for files like that; a brand-new *public* collection
// type's own file is never on it.
static const std::array<std::string_view, 1> ExcludedCollectionSurfaceFiles = {{
    "soa_storage.prime",
}};

static bool isExcludedCollectionSurfaceFile(std::string_view fileName) {
  return std::find(ExcludedCollectionSurfaceFiles.begin(), ExcludedCollectionSurfaceFiles.end(), fileName) !=
         ExcludedCollectionSurfaceFiles.end();
}

// Picks the struct annotation this file's collection surface should be
// derived from: the config's expected kind when the file has explicit
// config (preserves today's vector/map/soa selection exactly), otherwise
// the first collection_type/key_value_type annotation found (TODO-4689
// generic fallback for a file with no config entry).
static const StdlibCollectionStructAnnotation *selectCollectionSurfaceAnnotation(
    const std::vector<StdlibCollectionStructAnnotation> &annotations,
    const CollectionSurfaceConfig *config) {
  for (const auto &annotation : annotations) {
    if (config != nullptr ? annotation.kind == config->annotationKind
                           : (annotation.kind == StdlibCollectionAnnotationKind::CollectionType ||
                              annotation.kind == StdlibCollectionAnnotationKind::KeyValueType)) {
      return &annotation;
    }
  }
  return nullptr;
}

// TODO-4689: dynamically sized collection-surface storage. Every *.prime
// file directly under stdlib/std/collections/ carrying a
// [collection_type]/[key_value_type] struct annotation contributes one
// helper + one constructor entry, in file order (listStdlibCollectionFiles()
// already sorts for determinism); a file without such an annotation
// contributes nothing. Backing ManifestSurfaceData is returned by value
// into a vector the caller owns for the process lifetime -- growing that
// vector only moves ManifestSurfaceData objects (each of whose owned
// std::string/std::vector members keep their own heap buffers on move), so
// the string_view spans captured by refreshViews() before the push stay
// valid regardless of how many entries the vector ends up holding.
// Orders discovered collection files so today's known types (vector, map,
// soa) are processed in their historical CollectionSurfaceConfigs order
// first, with any other discovered/dynamic files following afterward in
// their (alphabetical) discovery order. Some downstream consumers of
// stdlibSurfaceRegistry() resolve an ambiguous/unscoped query (e.g. "which
// collection surface owns member X") by taking the first registry-order
// match rather than a canonicalPath-scoped one; preserving today's relative
// order for the known types keeps their behavior byte-for-byte unchanged,
// while a newly discovered surface still always appears (just after them).
static std::vector<std::filesystem::path> orderCollectionFilesForDerivation(
    std::vector<std::filesystem::path> collectionFiles) {
  std::vector<std::filesystem::path> ordered;
  ordered.reserve(collectionFiles.size());
  std::vector<bool> consumed(collectionFiles.size(), false);
  for (const auto &config : CollectionSurfaceConfigs) {
    for (std::size_t i = 0; i < collectionFiles.size(); ++i) {
      if (!consumed[i] && collectionFiles[i].filename() == config.fileName) {
        ordered.push_back(collectionFiles[i]);
        consumed[i] = true;
        break;
      }
    }
  }
  for (std::size_t i = 0; i < collectionFiles.size(); ++i) {
    if (!consumed[i]) {
      ordered.push_back(collectionFiles[i]);
    }
  }
  return ordered;
}

static std::vector<ManifestSurfaceData> deriveCollectionsSurfaceData() {
  std::vector<ManifestSurfaceData> derived;
  const std::vector<std::filesystem::path> collectionFiles =
      orderCollectionFilesForDerivation(listStdlibCollectionFiles());
  derived.reserve(collectionFiles.size() * 2);

  for (const auto &path : collectionFiles) {
    const std::string fileName = path.filename().string();
    if (isExcludedCollectionSurfaceFile(fileName)) {
      continue;
    }
    const CollectionSurfaceConfig *config = findCollectionSurfaceConfig(fileName);
    const auto annotations = detectStdlibCollectionStructAnnotations(path);
    const StdlibCollectionStructAnnotation *annotation =
        selectCollectionSurfaceAnnotation(annotations, config);
    if (annotation == nullptr) {
      // Not a collection-surface file (no matching struct annotation).
      continue;
    }

    const bool detectStatementMembers =
        config != nullptr ? config->detectStatementMembers
                           : DefaultCollectionSurfaceConfig.detectStatementMembers;
    const bool constructorJoinsHelperSurface =
        config != nullptr ? config->constructorJoinsHelperSurface
                           : DefaultCollectionSurfaceConfig.constructorJoinsHelperSurface;
    const std::string_view helperBackingTypeName =
        config != nullptr ? config->helperBackingTypeName
                           : DefaultCollectionSurfaceConfig.helperBackingTypeName;

    const std::string memberPrefix = deriveStdlibCollectionMemberPrefix(*annotation);
    const std::string collectionTypeName = annotation->typeName + "<";

    const auto records =
        scanStdlibPublicFunctions(path, memberPrefix, collectionTypeName, detectStatementMembers);

    std::vector<std::string> helperMembers;
    std::vector<std::string> helperStatements;
    std::vector<std::string> helperLowered;
    std::vector<std::string> ctorMembers;
    std::vector<std::string> ctorLowered;

    for (const auto &rec : records) {
      if (rec.isConstructor) {
        if (std::find(ctorMembers.begin(), ctorMembers.end(), rec.name) == ctorMembers.end()) {
          ctorMembers.push_back(rec.name);
          ctorLowered.push_back(rec.name);
        }
        if (constructorJoinsHelperSurface &&
            std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
        // constructors are never added to helperLowered (not in
        // lowering_spellings of the helper surface), regardless of type.
      } else {
        if (std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
        if (rec.isStatementMember) {
          helperStatements.push_back(rec.name);
        }
        if (rec.takesCollectionParam) {
          helperLowered.push_back(rec.name);
        }
      }
    }

    // Generic TODO-4687 derivation of canonicalPath/bridgeKey/import alias/
    // lowering-path base from the file's stem, independent of the
    // type-specific config above.
    const std::string basePath = deriveStdlibCollectionCanonicalPath(path);
    const std::string stem = path.stem().string();
    const std::string helperBridgeKey = deriveStdlibCollectionBridgeKey(path, "helpers");
    const std::string constructorBridgeKey = deriveStdlibCollectionBridgeKey(path, "constructors");
    const std::string constructorPath = basePath + "/" + stem;

    ManifestSurfaceData helperData = buildSurfaceData(
        helperBridgeKey, "/std/collections", basePath, std::string(helperBackingTypeName),
        helperMembers, helperStatements, helperLowered,
        basePath, stem, basePath);
    helperData.borrowedVariants.values = collectionSurfaceBorrowedVariantPairs(fileName);
    helperData.refreshViews();
    derived.push_back(std::move(helperData));

    derived.push_back(buildSurfaceData(
        constructorBridgeKey, "/std/collections", constructorPath, "",
        ctorMembers, {}, ctorLowered,
        basePath, stem, basePath));
  }

  return derived;
}

const std::vector<ManifestSurfaceData> CollectionsSurfaceData = deriveCollectionsSurfaceData();

// TODO-4689: the canonical paths TODO-4688's generic derivation is known to
// produce today for vector/map/soa's helper + constructor surfaces. A
// discovered collection surface whose canonicalPath matches one of these
// keeps its dedicated StdlibSurfaceId enum member (so none of the existing
// ~40 StdlibSurfaceId::CollectionsManifestSurface0-etc. call sites change);
// any other discovered collection surface gets
// StdlibSurfaceId::CollectionsDynamicSurface instead.
struct KnownCollectionSurfaceId {
  StdlibSurfaceId id;
  std::string_view canonicalPath;
};

static constexpr std::array<KnownCollectionSurfaceId, 6> KnownCollectionSurfaceIds = {{
    {StdlibSurfaceId::CollectionsManifestSurface0, "/std/collections/vector"},
    {StdlibSurfaceId::CollectionsManifestSurface1, "/std/collections/vector/vector"},
    {StdlibSurfaceId::CollectionsManifestSurface2, "/std/collections/map"},
    {StdlibSurfaceId::CollectionsManifestSurface3, "/std/collections/map/map"},
    {StdlibSurfaceId::CollectionsColumnarHelpers, "/std/collections/soa"},
    {StdlibSurfaceId::CollectionsColumnarConstructors, "/std/collections/soa/soa"},
}};

static StdlibSurfaceId resolveCollectionSurfaceId(std::string_view canonicalPath) {
  for (const auto &known : KnownCollectionSurfaceIds) {
    if (known.canonicalPath == canonicalPath) {
      return known.id;
    }
  }
  return StdlibSurfaceId::CollectionsDynamicSurface;
}

// Builds the domain==Collections StdlibSurfaceMetadata entries from the
// persistent CollectionsSurfaceData backing storage above (its
// string_view-bearing spans are what these entries point into, so
// CollectionsSurfaceData must outlive the returned entries).
static std::vector<StdlibSurfaceMetadata> buildCollectionsSurfaceMetadata(
    const std::vector<ManifestSurfaceData> &backing) {
  std::vector<StdlibSurfaceMetadata> entries;
  entries.reserve(backing.size());
  for (std::size_t i = 0; i < backing.size(); ++i) {
    const ManifestSurfaceData &d = backing[i];
    const bool isConstructor = (i % 2) == 1; // helper, constructor pushed in that order per file
    entries.push_back(StdlibSurfaceMetadata{
        .id = resolveCollectionSurfaceId(d.canonicalPath),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = isConstructor ? StdlibSurfaceShape::ConstructorFamily : StdlibSurfaceShape::HelperFamily,
        .bridgeKey = d.bridgeKey,
        .canonicalImportRoot = d.canonicalImportRoot,
        .canonicalPath = d.canonicalPath,
        .backingTypeName = d.backingTypeName,
        .memberNames = d.memberNames.views,
        .memberAliases = d.memberAliases.views,
        .statementMemberNames = d.statementMemberNames.views,
        .importAliasSpellings = d.importAliasSpellings.views,
        .compatibilitySpellings = d.compatibilitySpellings.views,
        .loweringSpellings = d.loweringSpellings.views,
        .borrowedVariants = d.borrowedVariants.views,
    });
  }
  return entries;
}

// Every known canonical collection path must be discovered at startup; a miss is reported
// through stdlibSurfaceRegistryStartupError() (the registry still builds without it).
}

std::string verifyKnownStdlibCollectionSurfacesResolved(std::span<const StdlibSurfaceMetadata> collectionsEntries) {
  for (const auto &known : KnownCollectionSurfaceIds) {
    const bool found = std::any_of(
        collectionsEntries.begin(), collectionsEntries.end(), [&](const StdlibSurfaceMetadata &entry) {
          return entry.id == known.id && entry.canonicalPath == known.canonicalPath;
        });
    if (!found) {
      return "StdlibSurfaceRegistry: a known collection surface canonical path "
             "was not discovered at startup; the stdlib collections directory "
             "layout may have changed without updating KnownCollectionSurfaceIds";
    }
  }
  return {};
}

namespace {

// TODO-4689: the 5 fixed, non-collection entries (File x2, Collections'
// ContainerError, Gfx x2). These never grow/shrink at runtime; only the
// collection entries spliced in between them (below) are dynamically
// discovered.
const StdlibSurfaceMetadata FileHelpersSurface = {
    .id = StdlibSurfaceId::FileHelpers,
    .domain = StdlibSurfaceDomain::File,
    .shape = StdlibSurfaceShape::HelperFamily,
    .bridgeKey = "file.file_helpers",
    .canonicalImportRoot = "/std/file",
    .canonicalPath = "/std/file/File",
    .backingTypeName = {},
    .memberNames = FileHelperMembers,
    .memberAliases = {},
    .statementMemberNames = {},
    .importAliasSpellings = FileHelperImportAliases,
    .compatibilitySpellings = FileHelperCompatibilitySpellings,
    .loweringSpellings = FileHelperLoweringSpellings,
    .borrowedVariants = {},
};

const StdlibSurfaceMetadata FileErrorHelpersSurface = {
    .id = StdlibSurfaceId::FileErrorHelpers,
    .domain = StdlibSurfaceDomain::File,
    .shape = StdlibSurfaceShape::ErrorFamily,
    .bridgeKey = "file.file_error",
    .canonicalImportRoot = "/std/file",
    .canonicalPath = "/std/file/FileError",
    .backingTypeName = {},
    .memberNames = FileErrorHelperMembers,
    .memberAliases = {},
    .statementMemberNames = {},
    .importAliasSpellings = FileErrorImportAliases,
    .compatibilitySpellings = FileErrorCompatibilitySpellings,
    .loweringSpellings = FileErrorLoweringSpellings,
    .borrowedVariants = {},
};

const StdlibSurfaceMetadata CollectionsContainerErrorHelpersSurface = {
    .id = StdlibSurfaceId::CollectionsContainerErrorHelpers,
    .domain = StdlibSurfaceDomain::Collections,
    .shape = StdlibSurfaceShape::ErrorFamily,
    .bridgeKey = "collections.container_error",
    .canonicalImportRoot = "/std/collections",
    .canonicalPath = "/std/collections/ContainerError",
    .backingTypeName = {},
    .memberNames = CollectionsContainerErrorMembers,
    .memberAliases = {},
    .statementMemberNames = {},
    .importAliasSpellings = CollectionsContainerErrorImportAliases,
    .compatibilitySpellings = CollectionsContainerErrorCompatibilitySpellings,
    .loweringSpellings = CollectionsContainerErrorLoweringSpellings,
    .borrowedVariants = {},
};

const StdlibSurfaceMetadata GfxBufferHelpersSurface = {
    .id = StdlibSurfaceId::GfxBufferHelpers,
    .domain = StdlibSurfaceDomain::Gfx,
    .shape = StdlibSurfaceShape::HelperFamily,
    .bridgeKey = "gfx.buffer_helpers",
    .canonicalImportRoot = "/std/gfx",
    .canonicalPath = "/std/gfx/Buffer",
    .backingTypeName = {},
    .memberNames = GfxBufferHelperMembers,
    .memberAliases = {},
    .statementMemberNames = {},
    .importAliasSpellings = GfxBufferImportAliases,
    .compatibilitySpellings = GfxBufferCompatibilitySpellings,
    .loweringSpellings = GfxBufferLoweringSpellings,
    .borrowedVariants = {},
};

const StdlibSurfaceMetadata GfxErrorHelpersSurface = {
    .id = StdlibSurfaceId::GfxErrorHelpers,
    .domain = StdlibSurfaceDomain::Gfx,
    .shape = StdlibSurfaceShape::ErrorFamily,
    .bridgeKey = "gfx.gfx_error",
    .canonicalImportRoot = "/std/gfx",
    .canonicalPath = "/std/gfx/GfxError",
    .backingTypeName = {},
    .memberNames = GfxErrorHelperMembers,
    .memberAliases = {},
    .statementMemberNames = {},
    .importAliasSpellings = GfxErrorImportAliases,
    .compatibilitySpellings = GfxErrorCompatibilitySpellings,
    .loweringSpellings = GfxErrorLoweringSpellings,
    .borrowedVariants = {},
};

// Registry storage: the 5 fixed entries plus the dynamically discovered collection entries,
// built once on first use (process lifetime).
struct RegistryBuild {
  std::vector<StdlibSurfaceMetadata> entries;
  std::string startupError;
};

const RegistryBuild &registryBuild() {
  // TODO-5235: first call can happen inside a compile scope, so build on the system heap.
  static const RegistryBuild build = systemHeapValue([] {
    const std::vector<StdlibSurfaceMetadata> collectionsEntries =
        buildCollectionsSurfaceMetadata(CollectionsSurfaceData);
    RegistryBuild result;
    result.startupError = verifyKnownStdlibCollectionSurfacesResolved(collectionsEntries);

    std::vector<StdlibSurfaceMetadata> &built = result.entries;
    built.reserve(5 + collectionsEntries.size());
    built.push_back(FileHelpersSurface);
    built.push_back(FileErrorHelpersSurface);
    built.insert(built.end(), collectionsEntries.begin(), collectionsEntries.end());
    built.push_back(CollectionsContainerErrorHelpersSurface);
    built.push_back(GfxBufferHelpersSurface);
    built.push_back(GfxErrorHelpersSurface);
    return result;
  });
  return build;
}

const std::vector<StdlibSurfaceMetadata> &registry() { return registryBuild().entries; }

bool matchesAny(std::span<const std::string_view> spellings, std::string_view spelling) {
  return std::find(spellings.begin(), spellings.end(), spelling) != spellings.end();
}

// TODO-5245: Registry (11 entries) is fixed, immutable, process-lifetime
// data, and every spelling that stdlibSurfaceMatchesSpelling() can match
// against (canonicalPath, importAliasSpellings, compatibilitySpellings,
// loweringSpellings) is likewise fixed once deriveCollectionsSurfaceData()
// has run at static-init time. findStdlibSurfaceMetadataBySpelling() was
// previously an O(Registry.size() * spellings-per-entry) linear scan
// (std::find_if over 11 entries, each running up to 3 matchesAny() linear
// scans over spans of up to ~20-30 spellings) called ~100K+ times in a
// single compile of even a trivial repro - a genuine complexity-class cost,
// not per-call-varying work. Building this index once and querying it in
// O(1) thereafter preserves EXACT match semantics (matchesAny/
// stdlibSurfaceMatchesSpelling are already pure exact string_view equality,
// never partial/prefix/substring matching - verified by reading both) while
// eliminating the repeated linear scans. try_emplace (rather than
// operator[]/insert) is essential for correctness here: it preserves the
// original std::find_if's "first entry in Registry order wins" semantics
// for any spelling that (in principle) appears under more than one
// registry entry, matching stdlibSurfaceMatchesSpelling()'s behavior
// exactly for every input, not just typical ones.
const std::unordered_map<std::string_view, const StdlibSurfaceMetadata *> &stdlibSurfaceSpellingIndex() {
  // TODO-5235: magic static - see registry()'s comment above.
  static const std::unordered_map<std::string_view, const StdlibSurfaceMetadata *> index = systemHeapValue([] {
    std::unordered_map<std::string_view, const StdlibSurfaceMetadata *> built;
    auto insertAll = [&](std::span<const std::string_view> spellings, const StdlibSurfaceMetadata *metadata) {
      for (const std::string_view spelling : spellings) {
        built.try_emplace(spelling, metadata);
      }
    };
    for (const StdlibSurfaceMetadata &metadata : registry()) {
      built.try_emplace(metadata.canonicalPath, &metadata);
      insertAll(metadata.importAliasSpellings, &metadata);
      insertAll(metadata.compatibilitySpellings, &metadata);
      insertAll(metadata.loweringSpellings, &metadata);
    }
    return built;
  });
  return index;
}

// TODO-5245: same rationale as stdlibSurfaceSpellingIndex() above, but for
// metadata.memberNames - resolveMetadataMemberName()/
// matchesResolvedRootedMemberPath() each ran an O(memberNames.size())
// matchesAny() scan (up to ~27 entries for vector's helper surface) on
// every call, tens of thousands of times per compile. Keyed by the
// metadata's address, which is stable and unique because every
// StdlibSurfaceMetadata instance that ever reaches this code is a
// reference/pointer into registry()'s single, once-built static vector
// (verified: no StdlibSurfaceMetadata is constructed anywhere else in the
// codebase); TODO-4689 made that vector dynamically sized, but it is still
// built exactly once and never resized afterward, so addresses into it
// remain stable for the process lifetime.
const std::unordered_set<std::string_view> &stdlibSurfaceMemberNameSet(const StdlibSurfaceMetadata &metadata) {
  // TODO-5235: magic static - see registry()'s comment above.
  static const std::unordered_map<const StdlibSurfaceMetadata *, std::unordered_set<std::string_view>> cache =
      systemHeapValue([] {
        std::unordered_map<const StdlibSurfaceMetadata *, std::unordered_set<std::string_view>> built;
        for (const StdlibSurfaceMetadata &entry : registry()) {
          built.emplace(&entry,
                        std::unordered_set<std::string_view>(entry.memberNames.begin(), entry.memberNames.end()));
        }
        return built;
      });
  return cache.at(&metadata);
}

std::string_view stripResolvedPathSpecializationSuffix(std::string_view path) {
  // Both "__t..." and "__ov..." markers require a literal "__" - skip the
  // two substring rfind scans entirely for the common case of a path with
  // no "__" at all (this function sits on the hot per-expression-node
  // monomorphization path, called on every candidate surface/alias, so
  // the cheap npos check is a real win at scale).
  if (path.find("__") == std::string_view::npos) {
    return path;
  }
  const std::size_t lastSlash = path.rfind('/');
  const std::size_t specializationMarker = path.rfind("__t");
  const std::size_t overloadMarker = path.rfind("__ov");
  std::size_t marker = std::string_view::npos;
  if (specializationMarker != std::string_view::npos &&
      (overloadMarker == std::string_view::npos || specializationMarker > overloadMarker)) {
    marker = specializationMarker;
  } else if (overloadMarker != std::string_view::npos) {
    marker = overloadMarker;
  }
  if (marker == std::string_view::npos || lastSlash == std::string_view::npos || marker <= lastSlash) {
    return path;
  }
  return path.substr(0, marker);
}

bool matchesResolvedRootedMemberPath(std::string_view path,
                                     std::string_view rootPath,
                                     const StdlibSurfaceMetadata &metadata) {
  if (rootPath.empty() || path.size() <= rootPath.size() || !path.starts_with(rootPath) ||
      path[rootPath.size()] != '/') {
    return false;
  }
  const std::string_view memberName =
      stripResolvedPathSpecializationSuffix(path.substr(rootPath.size() + 1));
  if (memberName.empty() || memberName.find('/') != std::string_view::npos) {
    return false;
  }
  return stdlibSurfaceMemberNameSet(metadata).contains(memberName);
}

bool matchesResolvedSurfaceMemberPath(const StdlibSurfaceMetadata &metadata,
                                      std::string_view path) {
  if (stdlibSurfaceMatchesSpelling(metadata, path) ||
      matchesResolvedRootedMemberPath(path, metadata.canonicalPath, metadata)) {
    return true;
  }
  return std::any_of(metadata.importAliasSpellings.begin(),
                     metadata.importAliasSpellings.end(),
                     [&](std::string_view aliasSpelling) {
                       return matchesResolvedRootedMemberPath(path, aliasSpelling, metadata);
                     });
}

std::string_view pathLeaf(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view resolveMetadataMemberName(const StdlibSurfaceMetadata &metadata,
                                           std::string_view memberName) {
  if (stdlibSurfaceMemberNameSet(metadata).contains(memberName)) {
    return memberName;
  }
  const auto it = std::find_if(metadata.memberAliases.begin(),
                               metadata.memberAliases.end(),
                               [memberName](const StdlibSurfaceMemberAlias &alias) {
                                 return alias.spelling == memberName;
                               });
  if (it != metadata.memberAliases.end()) {
    return it->memberName;
  }
  return {};
}

std::string_view resolveSurfaceMemberNameImpl(const StdlibSurfaceMetadata &metadata,
                                              std::string_view memberName) {
  if (metadata.domain == StdlibSurfaceDomain::Collections &&
      metadata.shape != StdlibSurfaceShape::ErrorFamily) {
    return resolveMetadataMemberName(metadata, memberName);
  }
  if (stdlibSurfaceMemberNameSet(metadata).contains(memberName)) {
    return memberName;
  }
  return {};
}

} // namespace

// TODO-4687: parses an optional `member_prefix="..."` override out of a
// trailing line comment on a [collection_type]/[key_value_type] annotation
// line, e.g. `[public struct key_value_type] // member_prefix="map"`.
// Returns nullopt when no such marker is present on the line.
static std::optional<std::string> parseStdlibCollectionMemberPrefixOverrideComment(
    std::string_view line) {
  static constexpr std::string_view marker = "member_prefix=\"";
  const auto markerPos = line.find(marker);
  if (markerPos == std::string_view::npos) {
    return std::nullopt;
  }
  const auto valueStart = markerPos + marker.size();
  const auto valueEnd = line.find('"', valueStart);
  if (valueEnd == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(line.substr(valueStart, valueEnd - valueStart));
}

// TODO-4686: generic [collection_type]/[key_value_type] struct-annotation
// detection. Defined outside the anonymous namespace above (but still in
// this translation unit, so it can reuse trimAscii/leadingSpaces/
// extractFunctionName/listStdlibCollectionFiles) so it has external linkage
// and is unit-testable directly.
std::vector<StdlibCollectionStructAnnotation> detectStdlibCollectionStructAnnotations(
    const std::filesystem::path &filepath) {
  std::ifstream input(filepath);
  if (!input) {
    return {};
  }

  // Pre-read all lines so lookahead to the declaration line following an
  // annotation works the same way scanStdlibPublicFunctions does it.
  std::vector<std::string> lines;
  {
    std::string line;
    while (std::getline(input, line)) {
      lines.push_back(std::move(line));
    }
  }

  std::vector<StdlibCollectionStructAnnotation> results;
  bool expectingStructName = false;
  StdlibCollectionAnnotationKind pendingKind{};
  std::optional<std::string> pendingOverride;

  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string_view sv(lines[i]);
    const std::string trimmed = trimAscii(sv);

    if (expectingStructName) {
      if (trimmed.empty()) {
        continue;
      }
      // Skip comment lines (// ...) but not rooted paths (/ ...)
      if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') {
        continue;
      }
      if (trimmed.front() == '[') {
        // Another annotation before the struct name line — reset and fall
        // through to re-check this line as a potential annotation of its own.
        expectingStructName = false;
      } else {
        // This is the struct declaration line, e.g. "SoaVector<T>() {".
        const std::string typeName = extractFunctionName(lines[i]);
        if (!typeName.empty()) {
          results.push_back({typeName, pendingKind, pendingOverride});
        }
        expectingStructName = false;
        pendingOverride.reset();
        continue;
      }
    }

    // Struct-level annotations sit at shallow indent (namespace body); skip
    // deeper (struct-method) annotations, matching scanStdlibPublicFunctions.
    if (leadingSpaces(sv) >= 4) {
      continue;
    }
    if (trimmed.find("[public struct") == std::string_view::npos) {
      continue;
    }
    if (trimmed.find("collection_type") != std::string_view::npos) {
      pendingKind = StdlibCollectionAnnotationKind::CollectionType;
    } else if (trimmed.find("key_value_type") != std::string_view::npos) {
      pendingKind = StdlibCollectionAnnotationKind::KeyValueType;
    } else {
      // A bare "[public struct]" (no collection/key-value annotation) — not
      // a detection target.
      continue;
    }
    // TODO-4687: optional member-prefix override, spelled as a standalone
    // `// member_prefix="..."` comment line immediately preceding the
    // annotation line (see the grammar note on
    // detectStdlibCollectionStructAnnotations()). Kept on its own line
    // (rather than trailing the annotation) so it does not perturb any
    // exact-substring matching against the annotation-then-declaration text
    // done elsewhere.
    pendingOverride = (i > 0) ? parseStdlibCollectionMemberPrefixOverrideComment(lines[i - 1])
                              : std::nullopt;
    expectingStructName = true;
  }
  return results;
}

std::vector<StdlibCollectionAnnotationFileResult>
detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles() {
  std::vector<StdlibCollectionAnnotationFileResult> results;
  for (const auto &file : listStdlibCollectionFiles()) {
    auto annotations = detectStdlibCollectionStructAnnotations(file);
    if (!annotations.empty()) {
      results.push_back({file, std::move(annotations)});
    }
  }
  return results;
}

// TODO-4687: default memberPrefix convention is lowercase-first-letter of
// the detected type name with no suffix stripping (Vector -> "vector",
// SoaVector -> "soaVector"); an explicit memberPrefixOverride always wins
// (MapValue -> "map" via override, since the bare convention would derive
// "mapValue"). See the scope-vs-acceptance discrepancy note in docs/todo.md
// TODO-4687 for why suffix-stripping is deliberately not implemented here.
std::string deriveStdlibCollectionMemberPrefix(const StdlibCollectionStructAnnotation &annotation) {
  if (annotation.memberPrefixOverride.has_value()) {
    return *annotation.memberPrefixOverride;
  }
  std::string prefix = annotation.typeName;
  if (!prefix.empty()) {
    prefix.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix.front())));
  }
  return prefix;
}

// TODO-4687: canonicalPath convention is "/std/collections/" + the file's
// stem (filename without the ".prime" extension), matching the paths
// deriveCollectionsSurfaceData() derives today (vector.prime ->
// "/std/collections/vector", map.prime -> "/std/collections/map",
// soa.prime -> "/std/collections/soa").
std::string deriveStdlibCollectionCanonicalPath(const std::filesystem::path &filepath) {
  return "/std/collections/" + filepath.stem().string();
}

// TODO-4687: bridgeKey convention is "collections." + file stem + "_" +
// surfaceSuffix, matching the hardcoded keys passed to buildSurfaceData()
// today (e.g. "collections.vector_helpers", "collections.map_constructors",
// "collections.soa_helpers").
std::string deriveStdlibCollectionBridgeKey(const std::filesystem::path &filepath,
                                            std::string_view surfaceSuffix) {
  return "collections." + filepath.stem().string() + "_" + std::string(surfaceSuffix);
}

const std::string &stdlibSurfaceRegistryStartupError() {
  return registryBuild().startupError;
}

std::span<const StdlibSurfaceMetadata> stdlibSurfaceRegistry() {
  return registry();
}

// TODO-4689 testing hook: stdlibSurfaceRegistry() is backed by a
// once-built, process-lifetime cache (registry(), above), so it cannot
// observe a *.prime file added to stdlib/std/collections/ after the first
// call to any StdlibSurfaceRegistry lookup in the current process -- which
// an unrelated earlier test in the same test binary/process may already
// have triggered. This performs the same collection-surface discovery and
// derivation fresh on every call (no caching), so a test can add a
// [collection_type]/[key_value_type]-annotated *.prime file and immediately
// observe it becoming a domain==Collections surface, proving the dynamic-
// discovery property stdlibSurfaceRegistry() itself relies on, regardless
// of process/test execution order.
std::vector<StdlibCollectionSurfaceSnapshot> rediscoverStdlibCollectionsSurfacesForTesting() {
  const std::vector<ManifestSurfaceData> backing = deriveCollectionsSurfaceData();
  const std::vector<StdlibSurfaceMetadata> entries = buildCollectionsSurfaceMetadata(backing);
  std::vector<StdlibCollectionSurfaceSnapshot> result;
  result.reserve(entries.size());
  for (const auto &entry : entries) {
    result.push_back({
        .id = entry.id,
        .domain = entry.domain,
        .shape = entry.shape,
        .canonicalPath = std::string(entry.canonicalPath),
        .bridgeKey = std::string(entry.bridgeKey),
    });
  }
  return result;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadata(StdlibSurfaceId id) {
  const auto &reg = registry();
  const auto it = std::find_if(reg.begin(), reg.end(), [id](const StdlibSurfaceMetadata &metadata) {
    return metadata.id == id;
  });
  return it == reg.end() ? nullptr : &*it;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByCanonicalPath(std::string_view canonicalPath) {
  const auto &reg = registry();
  const auto it = std::find_if(
      reg.begin(), reg.end(), [canonicalPath](const StdlibSurfaceMetadata &metadata) {
        return metadata.canonicalPath == canonicalPath;
      });
  return it == reg.end() ? nullptr : &*it;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByBridgeKey(std::string_view bridgeKey) {
  const auto &reg = registry();
  const auto it =
      std::find_if(reg.begin(), reg.end(), [bridgeKey](const StdlibSurfaceMetadata &metadata) {
        return metadata.bridgeKey == bridgeKey;
      });
  return it == reg.end() ? nullptr : &*it;
}

bool stdlibSurfaceMatchesSpelling(const StdlibSurfaceMetadata &metadata, std::string_view spelling) {
  return metadata.canonicalPath == spelling || matchesAny(metadata.importAliasSpellings, spelling) ||
         matchesAny(metadata.compatibilitySpellings, spelling) || matchesAny(metadata.loweringSpellings, spelling);
}

std::string stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId id, std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr || metadata->shape == StdlibSurfaceShape::ConstructorFamily) {
    return {};
  }
  if (helperName.find('/') != std::string_view::npos) {
    const StdlibSurfaceMetadata *helperMetadata =
        findStdlibSurfaceMetadataByResolvedPath(helperName);
    if (helperMetadata == nullptr || helperMetadata->id != id) {
      return {};
    }
  }
  const std::string_view resolvedMemberName = resolveStdlibSurfaceMemberName(*metadata, helperName);
  if (resolvedMemberName.empty()) {
    return {};
  }
  return std::string(metadata->canonicalPath) + "/" + std::string(resolvedMemberName);
}

std::string stdlibSurfaceBackingTypePath(const StdlibSurfaceMetadata &metadata) {
  if (metadata.canonicalPath.empty() || metadata.backingTypeName.empty()) {
    return {};
  }
  return std::string(metadata.canonicalPath) + "/" +
         std::string(metadata.backingTypeName);
}

std::string stdlibSurfaceBackingTypePath(StdlibSurfaceId id) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceBackingTypePath(*metadata);
}

std::string stdlibSurfacePreferredSpellingForMember(StdlibSurfaceId id,
                                                    std::string_view spelling,
                                                    std::string_view preferredPrefix) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  if (spelling.find('/') != std::string_view::npos) {
    const StdlibSurfaceMetadata *spellingMetadata =
        findStdlibSurfaceMetadataByResolvedPath(spelling);
    if (spellingMetadata == nullptr || spellingMetadata->id != id) {
      return {};
    }
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, spelling);
  if (memberName.empty()) {
    return {};
  }
  auto findPreferred = [&](std::span<const std::string_view> spellings) {
    for (const std::string_view candidate : spellings) {
      if (candidate.rfind(preferredPrefix, 0) == 0 &&
          resolveStdlibSurfaceMemberName(*metadata, candidate) == memberName) {
        return std::string(candidate);
      }
    }
    return std::string{};
  };
  if (std::string preferred = findPreferred(metadata->loweringSpellings);
      !preferred.empty()) {
    return preferred;
  }
  return findPreferred(metadata->compatibilitySpellings);
}

bool isStdlibSurfaceMemberName(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  return metadata != nullptr && matchesAny(metadata->memberNames, memberName);
}

bool isStdlibSurfaceStatementMemberName(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  return metadata != nullptr && matchesAny(metadata->statementMemberNames, memberName);
}

std::string_view findBorrowedVariant(const StdlibSurfaceMetadata &metadata, std::string_view memberName) {
  for (const auto &variant : metadata.borrowedVariants) {
    if (variant.spelling == memberName) {
      return variant.memberName;
    }
  }
  return {};
}

std::string_view findBorrowedVariant(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  return findBorrowedVariant(*metadata, memberName);
}

std::string resolveCompatibilitySpellingToCanonicalPath(std::string_view compatibilitySpelling) {
  const auto *metadata = findStdlibSurfaceMetadataBySpelling(compatibilitySpelling);
  if (metadata == nullptr) {
    return std::string(compatibilitySpelling);
  }
  const std::string_view canonicalPrefix = metadata->canonicalPath;
  const std::string compatStr(compatibilitySpelling);
  const std::string canonicalStr(canonicalPrefix);
  const size_t lastSlash = compatStr.find_last_of('/');
  if (lastSlash != std::string::npos && lastSlash + 1 < compatStr.size()) {
    const std::string_view memberName = std::string_view(compatibilitySpelling).substr(lastSlash + 1);
    return canonicalStr + "/" + std::string(memberName);
  }
  return std::string(compatibilitySpelling);
}

std::string findCompatibilitySpelling(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  const std::string canonicalSuffix = "/" + std::string(memberName);
  for (const auto &spelling : metadata->compatibilitySpellings) {
    if (spelling.size() >= canonicalSuffix.size() &&
        spelling.substr(spelling.size() - canonicalSuffix.size()) == canonicalSuffix) {
      return std::string(spelling);
    }
  }
  return {};
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataBySpelling(std::string_view spelling) {
  // TODO-5245: O(1) hash lookup against the precomputed spelling index
  // instead of an O(Registry.size()) scan that itself ran up to 3
  // O(spellings-per-entry) linear scans (via stdlibSurfaceMatchesSpelling's
  // matchesAny() calls) per entry. stdlibSurfaceSpellingIndex() is built to
  // match stdlibSurfaceMatchesSpelling()'s exact semantics (canonicalPath
  // OR importAliasSpellings OR compatibilitySpellings OR loweringSpellings,
  // first Registry-order match wins for any spelling shared across
  // entries), so this returns byte-identical results to the old linear
  // scan for every input.
  const auto &index = stdlibSurfaceSpellingIndex();
  const auto it = index.find(spelling);
  return it == index.end() ? nullptr : it->second;
}

namespace {

// Registry is static, immutable data for the process lifetime, so caching
// findStdlibSurfaceMetadataByResolvedPath's result per input path is always
// safe (no invalidation needed) and turns its ~15+ call sites' repeated
// queries for the same paths into O(1) after the first lookup.
// thread_local avoids a data race against the opt-in parallel definition-
// validation worker path, where multiple SemanticsValidator instances can
// call this concurrently from different threads.
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByResolvedPathUncached(std::string_view path) {
  if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(path); metadata != nullptr) {
    return metadata;
  }
  const std::string_view normalizedPath = stripResolvedPathSpecializationSuffix(path);
  if (normalizedPath != path) {
    if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(normalizedPath);
        metadata != nullptr) {
      return metadata;
    }
  }
  const auto &reg = registry();
  const auto it = std::find_if(
      reg.begin(), reg.end(), [normalizedPath](const StdlibSurfaceMetadata &metadata) {
        if (metadata.shape == StdlibSurfaceShape::ConstructorFamily) {
          return false;
        }
        if (matchesResolvedRootedMemberPath(normalizedPath, metadata.canonicalPath, metadata)) {
          return true;
        }
        return std::any_of(metadata.importAliasSpellings.begin(),
                           metadata.importAliasSpellings.end(),
                           [&](std::string_view aliasSpelling) {
                             return matchesResolvedRootedMemberPath(
                                 normalizedPath, aliasSpelling, metadata);
                           });
      });
  return it == reg.end() ? nullptr : &*it;
}

}  // namespace

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByResolvedPath(std::string_view path) {
  // Memoized in the current compilation context; entries point into
  // the process-wide registry and are inserted under a SystemHeapScope, so no
  // reset callback is needed.
  std::unordered_map<std::string, const StdlibSurfaceMetadata *> &cache =
      CompileContext::current().resolvedStdlibSurfacePaths;
  const std::string key(path);
  if (const auto it = cache.find(key); it != cache.end()) {
    return it->second;
  }
  const StdlibSurfaceMetadata *result = findStdlibSurfaceMetadataByResolvedPathUncached(path);
  {
    // TODO-5235: SystemHeapScope on every mutation, not just at
    // declaration - .clear() destroys elements but not the unordered_map's
    // own bucket-array buffer, which a later insert-triggered rehash can
    // reallocate at any time. See docs/CompilerArenaAllocator.md.
    SystemHeapScope systemHeapGuard;
    cache.emplace(key, result);
  }
  return result;
}

std::string_view resolveStdlibSurfaceMemberName(const StdlibSurfaceMetadata &metadata,
                                                std::string_view path) {
  const std::string_view normalizedPath = stripResolvedPathSpecializationSuffix(path);
  if (normalizedPath.find('/') != std::string_view::npos &&
      !matchesResolvedSurfaceMemberPath(metadata, normalizedPath)) {
    return {};
  }
  const std::string_view memberName = pathLeaf(normalizedPath);
  return resolveSurfaceMemberNameImpl(metadata, memberName);
}

} // namespace primec

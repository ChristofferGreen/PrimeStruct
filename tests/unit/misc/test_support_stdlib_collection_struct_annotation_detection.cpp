#include "primec/support/StdlibSurfaceRegistry.h"

#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.stdlib.collection_struct_annotation_detection");

namespace {

std::filesystem::path writeFixture(std::string_view name, std::string_view contents) {
  const std::filesystem::path path = primec::testing::testScratchPath(
      std::string("collection_struct_annotation_detection/") + std::string(name));
  std::ofstream out(path);
  out << contents;
  out.close();
  return path;
}

} // namespace

TEST_CASE("detects a [collection_type] struct and its declared type name") {
  const auto path = writeFixture("collection_type.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace widget {
  [public struct collection_type]
  Widget<T>() {
    [public i32] count
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  REQUIRE(results.size() == 1);
  CHECK(results[0].typeName == "Widget");
  CHECK(results[0].kind == primec::StdlibCollectionAnnotationKind::CollectionType);
}

TEST_CASE("detects a [key_value_type] struct and its declared type name") {
  const auto path = writeFixture("key_value_type.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace bag {
  [public struct key_value_type]
  BagValue<K, V>() {
    [public i32] count
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  REQUIRE(results.size() == 1);
  CHECK(results[0].typeName == "BagValue");
  CHECK(results[0].kind == primec::StdlibCollectionAnnotationKind::KeyValueType);
}

TEST_CASE("does not misclassify a bare [public struct] with no collection/key-value annotation") {
  const auto path = writeFixture("bare_struct.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace bag {
  [public struct]
  Entry<K, V>() {
    [public K] key
    [public V] value
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  CHECK(results.empty());
}

TEST_CASE("does not report a deeper, struct-method-indented annotation as a top-level struct") {
  const auto path = writeFixture("nested_only.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace widget {
  [public struct]
  Widget<T>() {
      [public struct collection_type]
      Inner<T>() {
        [public i32] count
      }
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  CHECK(results.empty());
}

TEST_CASE("skips comment lines between the annotation and the struct declaration") {
  const auto path = writeFixture("comment_between.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace widget {
  [public struct collection_type]
  // A comment describing the struct.
  Widget<T>() {
    [public i32] count
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  REQUIRE(results.size() == 1);
  CHECK(results[0].typeName == "Widget");
}

TEST_CASE("returns empty for a missing file") {
  const auto results = primec::detectStdlibCollectionStructAnnotations(
      primec::testing::testScratchRoot() / "does_not_exist.prime");
  CHECK(results.empty());
}

TEST_CASE("detects all annotated structs across the real stdlib collection files") {
  const auto perFile = primec::detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles();

  std::vector<std::string> foundNames;
  for (const auto &fileResult : perFile) {
    for (const auto &annotation : fileResult.annotations) {
      foundNames.push_back(annotation.typeName);
    }
  }
  std::sort(foundNames.begin(), foundNames.end());

  // TODO-4702 added RingBuffer (stdlib/std/collections/ring_buffer.prime) as
  // a second toy collection type, proving the discovery mechanism requires
  // zero further edits here to pick up a genuinely new [collection_type]
  // struct.
  const std::vector<std::string> expected = {"MapValue", "RingBuffer", "SoaColumn", "SoaVector", "Vector"};
  CHECK(foundNames == expected);

  // Confirm MapValue is specifically detected as [key_value_type] and the
  // other 3 as [collection_type], and that SoaColumn (soa_storage.prime,
  // currently unused by the registry proper) is discovered without being
  // misclassified as one of the other 3.
  for (const auto &fileResult : perFile) {
    for (const auto &annotation : fileResult.annotations) {
      if (annotation.typeName == "MapValue") {
        CHECK(annotation.kind == primec::StdlibCollectionAnnotationKind::KeyValueType);
      } else {
        CHECK(annotation.kind == primec::StdlibCollectionAnnotationKind::CollectionType);
      }
    }
  }
}

TEST_CASE("detects a member_prefix override comment preceding the annotation line") {
  const auto path = writeFixture("member_prefix_override.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace bag {
  // member_prefix="bag"
  [public struct key_value_type]
  BagValue<K, V>() {
    [public i32] count
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  REQUIRE(results.size() == 1);
  CHECK(results[0].typeName == "BagValue");
  REQUIRE(results[0].memberPrefixOverride.has_value());
  CHECK(*results[0].memberPrefixOverride == "bag");
}

TEST_CASE("leaves memberPrefixOverride unset when no override comment is present") {
  const auto path = writeFixture("no_override.prime", R"PRIME(
namespace std {
  namespace collections {
  namespace widget {
  [public struct collection_type]
  Widget<T>() {
    [public i32] count
  }
  }
  }
}
)PRIME");

  const auto results = primec::detectStdlibCollectionStructAnnotations(path);
  REQUIRE(results.size() == 1);
  CHECK_FALSE(results[0].memberPrefixOverride.has_value());
}

TEST_CASE("real map.prime carries the member_prefix=\"map\" override") {
  const auto perFile = primec::detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles();
  bool found = false;
  for (const auto &fileResult : perFile) {
    for (const auto &annotation : fileResult.annotations) {
      if (annotation.typeName == "MapValue") {
        found = true;
        REQUIRE(annotation.memberPrefixOverride.has_value());
        CHECK(*annotation.memberPrefixOverride == "map");
      }
    }
  }
  CHECK(found);
}

TEST_CASE("deriveStdlibCollectionMemberPrefix: default convention lowercases only the first letter") {
  primec::StdlibCollectionStructAnnotation vectorAnnotation{
      "Vector", primec::StdlibCollectionAnnotationKind::CollectionType, std::nullopt};
  CHECK(primec::deriveStdlibCollectionMemberPrefix(vectorAnnotation) == "vector");

  // SoaVector: no suffix stripping — the whole type name is lowercase-first-
  // lettered, matching today's hardcoded "soaVector" skipLongNamePrefix.
  primec::StdlibCollectionStructAnnotation soaAnnotation{
      "SoaVector", primec::StdlibCollectionAnnotationKind::CollectionType, std::nullopt};
  CHECK(primec::deriveStdlibCollectionMemberPrefix(soaAnnotation) == "soaVector");

  // MapValue without an override would derive "mapValue", not "map" — this
  // is exactly why the acceptance criteria require map.prime's explicit
  // override.
  primec::StdlibCollectionStructAnnotation mapNoOverride{
      "MapValue", primec::StdlibCollectionAnnotationKind::KeyValueType, std::nullopt};
  CHECK(primec::deriveStdlibCollectionMemberPrefix(mapNoOverride) == "mapValue");
}

TEST_CASE("deriveStdlibCollectionMemberPrefix: explicit override always wins") {
  primec::StdlibCollectionStructAnnotation mapWithOverride{
      "MapValue", primec::StdlibCollectionAnnotationKind::KeyValueType,
      std::optional<std::string>("map")};
  CHECK(primec::deriveStdlibCollectionMemberPrefix(mapWithOverride) == "map");
}

TEST_CASE("derived memberPrefix/canonicalPath/bridgeKey match today's hardcoded "
          "deriveCollectionsSurfaces() values for vector/map/soa") {
  const auto perFile = primec::detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles();

  auto findAnnotation = [&](std::string_view fileName,
                            std::string_view typeName) -> const primec::StdlibCollectionStructAnnotation * {
    for (const auto &fileResult : perFile) {
      if (fileResult.file.filename() != fileName) {
        continue;
      }
      for (const auto &annotation : fileResult.annotations) {
        if (annotation.typeName == typeName) {
          return &annotation;
        }
      }
    }
    return nullptr;
  };

  // --- vector.prime / Vector -> "vector" (no override needed) ---
  {
    const auto *annotation = findAnnotation("vector.prime", "Vector");
    REQUIRE(annotation != nullptr);
    const std::filesystem::path filepath = "stdlib/std/collections/vector.prime";
    CHECK(primec::deriveStdlibCollectionMemberPrefix(*annotation) == "vector");
    CHECK(primec::deriveStdlibCollectionCanonicalPath(filepath) == "/std/collections/vector");
    CHECK(primec::deriveStdlibCollectionBridgeKey(filepath, "helpers") ==
          "collections.vector_helpers");
    CHECK(primec::deriveStdlibCollectionBridgeKey(filepath, "constructors") ==
          "collections.vector_constructors");
  }

  // --- map.prime / MapValue -> "map" (via explicit override) ---
  {
    const auto *annotation = findAnnotation("map.prime", "MapValue");
    REQUIRE(annotation != nullptr);
    const std::filesystem::path filepath = "stdlib/std/collections/map.prime";
    CHECK(primec::deriveStdlibCollectionMemberPrefix(*annotation) == "map");
    CHECK(primec::deriveStdlibCollectionCanonicalPath(filepath) == "/std/collections/map");
    CHECK(primec::deriveStdlibCollectionBridgeKey(filepath, "helpers") ==
          "collections.map_helpers");
    CHECK(primec::deriveStdlibCollectionBridgeKey(filepath, "constructors") ==
          "collections.map_constructors");
  }

  // --- soa.prime / SoaVector -> "soaVector" (no override needed) ---
  {
    const auto *annotation = findAnnotation("soa.prime", "SoaVector");
    REQUIRE(annotation != nullptr);
    const std::filesystem::path filepath = "stdlib/std/collections/soa.prime";
    CHECK(primec::deriveStdlibCollectionMemberPrefix(*annotation) == "soaVector");
    CHECK(primec::deriveStdlibCollectionCanonicalPath(filepath) == "/std/collections/soa");
    CHECK(primec::deriveStdlibCollectionBridgeKey(filepath, "helpers") ==
          "collections.soa_helpers");
    CHECK(primec::deriveStdlibCollectionBridgeKey(filepath, "constructors") ==
          "collections.soa_constructors");
  }
}

// TODO-4688: parity snapshot of deriveCollectionsSurfaces()'s output,
// captured from the 3 hand-written blocks before folding them into one
// generic loop. This is the hard byte-identical-output requirement from
// TODO-4688's acceptance criteria: every field of every derived collection
// StdlibSurfaceMetadata entry, asserted against literal values captured
// from the pre-refactor implementation.
TEST_CASE("deriveCollectionsSurfaces() output is byte-identical across the TODO-4688 refactor") {
  struct Expected {
    primec::StdlibSurfaceId id;
    std::string_view bridgeKey;
    std::string_view canonicalImportRoot;
    std::string_view canonicalPath;
    std::string_view backingTypeName;
    std::vector<std::string_view> memberNames;
    std::vector<std::string_view> statementMemberNames;
    std::vector<std::string_view> importAliasSpellings;
    std::vector<std::string_view> loweringSpellings;
  };

  const std::vector<Expected> expectations = {
      {primec::StdlibSurfaceId::CollectionsManifestSurface0,
       "collections.vector_helpers", "/std/collections", "/std/collections/vector", "",
       {"vector", "count", "capacity", "push", "pop", "reserve", "clear", "remove_at",
        "remove_swap", "at", "at_unsafe"},
       {"push", "pop", "reserve", "clear", "remove_at", "remove_swap"},
       {"/std/collections/vector", "vector"},
       {"/std/collections/vector/count", "/std/collections/vector/capacity",
        "/std/collections/vector/push", "/std/collections/vector/pop",
        "/std/collections/vector/reserve", "/std/collections/vector/clear",
        "/std/collections/vector/remove_at", "/std/collections/vector/remove_swap",
        "/std/collections/vector/at", "/std/collections/vector/at_unsafe"}},
      {primec::StdlibSurfaceId::CollectionsManifestSurface1,
       "collections.vector_constructors", "/std/collections", "/std/collections/vector/vector", "",
       {"vector"},
       {},
       {"/std/collections/vector", "vector"},
       {"/std/collections/vector/vector"}},
      {primec::StdlibSurfaceId::CollectionsManifestSurface2,
       "collections.map_helpers", "/std/collections", "/std/collections/map", "MapValue",
       {"entry", "map", "count", "count_ref", "insert", "insert_ref", "contains", "contains_ref",
        "tryAt", "tryAt_ref", "at", "at_ref", "at_unsafe", "at_unsafe_ref"},
       {},
       {"/std/collections/map", "map"},
       {"/std/collections/map/count", "/std/collections/map/count_ref",
        "/std/collections/map/insert", "/std/collections/map/insert_ref",
        "/std/collections/map/contains", "/std/collections/map/contains_ref",
        "/std/collections/map/tryAt", "/std/collections/map/tryAt_ref",
        "/std/collections/map/at", "/std/collections/map/at_ref",
        "/std/collections/map/at_unsafe", "/std/collections/map/at_unsafe_ref"}},
      {primec::StdlibSurfaceId::CollectionsManifestSurface3,
       "collections.map_constructors", "/std/collections", "/std/collections/map/map", "",
       {"map"},
       {},
       {"/std/collections/map", "map"},
       {"/std/collections/map/map"}},
      {primec::StdlibSurfaceId::CollectionsColumnarHelpers,
       "collections.soa_helpers", "/std/collections", "/std/collections/soa", "",
       {"count", "count_ref", "get", "get_ref", "ref", "ref_ref", "field_view", "reserve",
        "push", "to_aos", "to_aos_ref"},
       {},
       {"/std/collections/soa", "soa"},
       {"/std/collections/soa/count", "/std/collections/soa/count_ref",
        "/std/collections/soa/get", "/std/collections/soa/get_ref",
        "/std/collections/soa/ref", "/std/collections/soa/ref_ref",
        "/std/collections/soa/field_view", "/std/collections/soa/reserve",
        "/std/collections/soa/push", "/std/collections/soa/to_aos",
        "/std/collections/soa/to_aos_ref"}},
      {primec::StdlibSurfaceId::CollectionsColumnarConstructors,
       "collections.soa_constructors", "/std/collections", "/std/collections/soa/soa", "",
       {"soa", "single", "from_aos"},
       {},
       {"/std/collections/soa", "soa"},
       {"/std/collections/soa/soa", "/std/collections/soa/single",
        "/std/collections/soa/from_aos"}},
  };

  for (const auto &expected : expectations) {
    const auto *m = primec::findStdlibSurfaceMetadata(expected.id);
    REQUIRE(m != nullptr);
    CAPTURE(expected.bridgeKey);
    CHECK(m->bridgeKey == expected.bridgeKey);
    CHECK(m->canonicalImportRoot == expected.canonicalImportRoot);
    CHECK(m->canonicalPath == expected.canonicalPath);
    CHECK(m->backingTypeName == expected.backingTypeName);
    CHECK(std::vector<std::string_view>(m->memberNames.begin(), m->memberNames.end()) ==
          expected.memberNames);
    CHECK(std::vector<std::string_view>(m->statementMemberNames.begin(),
                                         m->statementMemberNames.end()) ==
          expected.statementMemberNames);
    CHECK(std::vector<std::string_view>(m->importAliasSpellings.begin(),
                                         m->importAliasSpellings.end()) ==
          expected.importAliasSpellings);
    CHECK(std::vector<std::string_view>(m->loweringSpellings.begin(), m->loweringSpellings.end()) ==
          expected.loweringSpellings);
  }
}

TEST_SUITE_END();

// TODO-4689: proves the registry's collection-surface storage is
// dynamically discovered (not a fixed, compile-time-sized list) by adding a
// brand-new [collection_type]-annotated *.prime file at test time and
// checking it becomes a domain==Collections surface with no dedicated
// StdlibSurfaceId enum member -- with zero edits to
// StdlibSurfaceRegistry.cpp/.h beyond the one-time testing hook added
// alongside this task (rediscoverStdlibCollectionsSurfacesForTesting()).
// Kept in its own TEST_SUITE (see PrimeStructMiscTestSuites in
// CMakeLists.txt) so ctest runs it as its own process: this avoids the
// awkwardness of stdlibSurfaceRegistry() itself being a once-built,
// process-lifetime cache that an earlier test's unrelated registry lookup
// could otherwise have already built before this file existed on disk.
TEST_SUITE_BEGIN("primestruct.stdlib.collection_registry_dynamic_discovery");

TEST_CASE("a newly added collection_type annotated file appears as a domain==Collections "
          "surface with no dedicated enum member, without any registry source edits") {
  // Locate the real stdlib/std/collections directory via the same
  // production discovery entry point exercised elsewhere in this file,
  // rather than duplicating its directory-walk logic here.
  const auto discovered = primec::detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles();
  REQUIRE(!discovered.empty());
  const std::filesystem::path collectionsDir = discovered.front().file.parent_path();

  const std::filesystem::path spikePath =
      collectionsDir / "todo4689_spike_dynamic_registry_type.prime";

  struct SpikeFileGuard {
    std::filesystem::path path;
    ~SpikeFileGuard() {
      std::error_code ec;
      std::filesystem::remove(path, ec);
    }
  } spikeFileGuard{spikePath};

  {
    std::ofstream out(spikePath);
    REQUIRE(bool(out));
    out << R"PRIME(
namespace std {
  namespace collections {
  namespace todo4689_spike {
  [public struct collection_type]
  Todo4689SpikeBag<T>() {
    [public i32] count

    [public return<i32>]
    size() {
      return(this.count)
    }
  }
  }
  }
}
)PRIME";
  }

  const auto snapshots = primec::rediscoverStdlibCollectionsSurfacesForTesting();

  const primec::StdlibCollectionSurfaceSnapshot *helperSurface = nullptr;
  for (const auto &snapshot : snapshots) {
    if (snapshot.canonicalPath == "/std/collections/todo4689_spike_dynamic_registry_type") {
      helperSurface = &snapshot;
      break;
    }
  }

  REQUIRE(helperSurface != nullptr);
  CHECK(helperSurface->domain == primec::StdlibSurfaceDomain::Collections);
  CHECK(helperSurface->shape == primec::StdlibSurfaceShape::HelperFamily);
  // No dedicated StdlibSurfaceId enum member exists for this brand-new
  // type: it resolves to the shared "dynamically discovered" id instead.
  CHECK(helperSurface->id == primec::StdlibSurfaceId::CollectionsDynamicSurface);
  CHECK(helperSurface->bridgeKey == "collections.todo4689_spike_dynamic_registry_type_helpers");
}

TEST_SUITE_END();

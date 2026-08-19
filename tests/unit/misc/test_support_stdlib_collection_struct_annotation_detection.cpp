#include "primec/support/StdlibSurfaceRegistry.h"

#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <fstream>
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

TEST_CASE("detects all 4 currently-annotated structs across the real stdlib collection files") {
  const auto perFile = primec::detectStdlibCollectionStructAnnotationsAcrossDiscoveredFiles();

  std::vector<std::string> foundNames;
  for (const auto &fileResult : perFile) {
    for (const auto &annotation : fileResult.annotations) {
      foundNames.push_back(annotation.typeName);
    }
  }
  std::sort(foundNames.begin(), foundNames.end());

  const std::vector<std::string> expected = {"MapValue", "SoaColumn", "SoaVector", "Vector"};
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

TEST_SUITE_END();

#include "primec/support/StdlibSurfaceRegistry.h"

#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <fstream>
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

TEST_SUITE_END();

#include "third_party/doctest.h"

#include "src/semantics/StdlibCollectionSurfaceHelpers.h"

#include <string>

// TODO-5964: the key-value and collection path predicates exist once; these
// cases pin the behavior chosen when the per-file copies were merged.

TEST_SUITE_BEGIN("primestruct.semantics.collection_path_predicates");

namespace {

std::string backingRoot() {
  std::string root = experimentalCollectionConstructorRootLocal("map");
  if (!root.empty() && root.front() == '/') {
    root.erase(root.begin());
  }
  return root + "Map";
}

} // namespace

TEST_CASE("a specialized key-value backing path needs the generated suffix") {
  const std::string root = backingRoot();
  CHECK(isSpecializedExperimentalKeyValueBackingPath(root + "__t1234"));
  CHECK(isSpecializedExperimentalKeyValueBackingPath("/" + root + "__t1234"));
  CHECK_FALSE(isSpecializedExperimentalKeyValueBackingPath(root));
  CHECK_FALSE(isSpecializedExperimentalKeyValueBackingPath("/" + root));
  CHECK_FALSE(isSpecializedExperimentalKeyValueBackingPath("Map"));
  CHECK_FALSE(isSpecializedExperimentalKeyValueBackingPath("/std/collections/map/Map"));
  CHECK_FALSE(isSpecializedExperimentalKeyValueBackingPath(""));
}

TEST_CASE("the canonical key-value helper path joins the canonical root and the helper") {
  CHECK(canonicalKeyValueHelperPathLocal("at") == "/std/collections/map/at");
  CHECK(canonicalKeyValueHelperPathLocal("count") == "/std/collections/map/count");
}

TEST_CASE("key-value access helpers are at and at_unsafe, tryAt only in the wider predicate") {
  CHECK(isCanonicalKeyValueAccessHelperName("at"));
  CHECK(isCanonicalKeyValueAccessHelperName("at_ref"));
  CHECK(isCanonicalKeyValueAccessHelperName("at_unsafe"));
  CHECK(isCanonicalKeyValueAccessHelperName("at_unsafe_ref"));
  CHECK_FALSE(isCanonicalKeyValueAccessHelperName("tryAt"));
  CHECK_FALSE(isCanonicalKeyValueAccessHelperName("count"));
  CHECK(isCanonicalKeyValueAccessOrTryAtHelperName("tryAt"));
  CHECK(isCanonicalKeyValueAccessOrTryAtHelperName("at"));
  CHECK_FALSE(isCanonicalKeyValueAccessOrTryAtHelperName("insert"));
}

TEST_CASE("generated helper suffixes are stripped from the leaf at the earliest suffix") {
  CHECK(stripGeneratedHelperSuffix("count") == "count");
  CHECK(stripGeneratedHelperSuffix("/map/count__t1") == "/map/count");
  CHECK(stripGeneratedHelperSuffix("/map/count__ov2") == "/map/count");
  CHECK(stripGeneratedHelperSuffix("/map/count__t1__ov2") == "/map/count");
  // The earliest suffix wins whichever generated kind comes first.
  CHECK(stripGeneratedHelperSuffix("/map/count__ov2__t1") == "/map/count");
  // A parent component keeps its own specialization suffix.
  CHECK(stripGeneratedHelperSuffix("/Box__t9/get__ov1") == "/Box__t9/get");
}

TEST_SUITE_END();

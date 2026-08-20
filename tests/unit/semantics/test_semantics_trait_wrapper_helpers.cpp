#include "third_party/doctest.h"

#include <string>
#include <vector>

#include "src/semantics/SemanticsHelpers.h"
#include "src/semantics/StdlibCollectionSurfaceHelpers.h"

// TODO-4694: isKeyValueSurfaceTypeName is a behavior-preserving,
// generically-named wrapper unioning today's key-value type-name
// classification helpers: isKeyValueCollectionTypeName plus the
// literal-arg map-and-entry backing-type-name cases of
// isExperimentalCollectionBackingTypeName. These tests prove
// isKeyValueSurfaceTypeName(name) == isKeyValueCollectionTypeName(name) ||
// isExperimentalCollectionBackingTypeName("map", "Map", name) ||
// isExperimentalCollectionBackingTypeName("map", "Entry", name)
// across a representative set of currently-known inputs, covering both
// true and false cases and the special-cased backing-type spellings.

TEST_SUITE_BEGIN("primestruct.semantics.trait_wrapper_helpers");

namespace {

bool oldUnionKeyValueSurfaceTypeName(const std::string &name) {
  return primec::semantics::isKeyValueCollectionTypeName(name) ||
         isExperimentalCollectionBackingTypeName("map", "Map", name) ||
         isExperimentalCollectionBackingTypeName("map", "Entry", name);
}

} // namespace

TEST_CASE("isKeyValueSurfaceTypeName matches the union of the old key-value type-name helpers") {
  const std::vector<std::string> candidates = {
      // Canonical / surface key-value collection spellings.
      "Map",
      "Map<i32, string>",
      "Map<string, Map<i32, bool>>",
      "/Map",
      "Entry",
      "/Entry",
      "Entry<i32, string>",

      // Non-key-value collection spellings that should stay false.
      "array",
      "args",
      "vector",
      "Vector<i32>",
      "soa",
      "soa<i32>",

      // Primitive / scalar spellings.
      "i32",
      "i64",
      "u64",
      "f32",
      "f64",
      "bool",
      "string",

      // Reference/pointer-wrapped and templated spellings that the old
      // helpers treat as plain (non-key-value) names, since
      // isKeyValueCollectionTypeName/isExperimentalCollectionBackingTypeName
      // operate on the immediate base spelling rather than unwrapping
      // Reference/Pointer themselves.
      "Reference<Map<i32, string>>",
      "Pointer<Map<i32, string>>",

      // Empty / malformed / unrelated spellings.
      "",
      "/",
      "NotAType",
      "MapLike",
      "SomeEntryStruct",
      "/std/collections/experimental_map/Map",
      "/std/collections/experimental_map/Map__t0",
      "/std/collections/experimental_map/Entry",
      "/std/collections/experimental_map/Entry__t0",
      "/std/collections/experimental_map/SomethingElse",
  };

  for (const auto &candidate : candidates) {
    CAPTURE(candidate);
    CHECK(primec::semantics::isKeyValueSurfaceTypeName(candidate) ==
          oldUnionKeyValueSurfaceTypeName(candidate));
  }
}

TEST_CASE("isKeyValueSurfaceTypeName covers known-true and known-false anchor cases") {
  // Anchor cases pinned to concrete expected results, so a degenerate
  // wrapper (e.g. one that always returns false) cannot pass purely by
  // matching a similarly-degenerate comparison helper.
  CHECK(primec::semantics::isKeyValueSurfaceTypeName("Map"));
  CHECK(primec::semantics::isKeyValueSurfaceTypeName("Map<i32, string>"));
  CHECK(primec::semantics::isKeyValueSurfaceTypeName("Entry"));
  CHECK_FALSE(primec::semantics::isKeyValueSurfaceTypeName("i32"));
  CHECK_FALSE(primec::semantics::isKeyValueSurfaceTypeName("array"));
  CHECK_FALSE(primec::semantics::isKeyValueSurfaceTypeName(""));
}

TEST_SUITE_END();

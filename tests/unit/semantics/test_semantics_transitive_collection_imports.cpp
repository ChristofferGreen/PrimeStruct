#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.transitive_collection_imports");

// Regression coverage for TODO-4677/4678: transitive wildcard imports of the
// vector module must expose helper member names (vectorCount, vectorPush, ...)
// exactly like every other module. Before the fix, buildImportAliases special-
// cased the vector folder in the transitive branch and registered only the
// Vector type alias, so bare vectorCount calls inside imported stdlib modules
// (map.prime) failed with "unknown call target" whenever the user imported
// /std/collections/* instead of /std/collections/vector/* directly.

TEST_CASE("bare vector helper resolves with directory wildcard import") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(vectorCount<i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector helper resolves inside monomorphized templated caller") {
  const std::string source = R"(
import /std/collections/*

[return<i32>]
wrapped<K>([Vector<K>] values) {
  return(vectorCount<K>(values))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32)}
  return(wrapped<i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin count keeps map dispatch despite vector helper aliases") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32> mut] values{/std/collections/map/map<i32, i32>(1i32, 10i32)}
  insert(values, 2i32, 20i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

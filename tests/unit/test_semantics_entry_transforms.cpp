#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.transforms");

TEST_CASE("unsupported return type fails") {
  const std::string source = R"(
[return<u32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported return type") != std::string::npos);
}

TEST_CASE("array return requires template argument") {
  const std::string source = R"(
[return<array>]
main() {
  return(array<i32>{1i32})
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array return type requires exactly one template argument") != std::string::npos);
}

TEST_CASE("template vector and map returns are allowed") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<T>>]
wrapVector<T>([T] value) {
  return(vector<T>(value))
}

[effects(heap_alloc), return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(map<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector<i32>(9i32)}
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 3i32)}
  return(plus(count(values), count(pairs)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib map returns are allowed") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values) {
  return(1i32)
}

[effects(heap_alloc), return</std/collections/map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(map<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 3i32)}
  return(/std/collections/map/count(pairs))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector return rejects wrong template arity") {
  const std::string source = R"(
[return<vector<i32, i32>>]
main() {
  return(vector<i32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector return type requires exactly one template argument") != std::string::npos);
}

TEST_CASE("map return rejects wrong template arity") {
  const std::string source = R"(
[return<map<string>>]
main() {
  return(map<string, i32>("only"raw_utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map return type requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("map return rejects unsupported builtin Comparable key contract") {
  const std::string source = R"(
[return<map<array<i32>, i32>>]
main() {
  return(map<array<i32>, i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): array<i32>") !=
        std::string::npos);
}

TEST_CASE("canonical stdlib map return rejects direct template arguments") {
  const std::string source = R"(
[return</std/collections/map<string>>]
main() {
  return(map<string, i32>("only"raw_utf8, 1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /std/collections/map") !=
        std::string::npos);
}

TEST_CASE("vector return accepts array element type during semantics validation") {
  const std::string source = R"(
[return<vector<array<i32>>>]
main() {
  return(vector<array<i32>>())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map return accepts array value type during semantics validation") {
  const std::string source = R"(
[return<map<string, array<i32>>>]
main() {
  return(map<string, array<i32>>())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return transform rejects duplicate return") {
  const std::string source = R"(
[return<int>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate return transform") != std::string::npos);
}

TEST_CASE("effects transform aggregates distinct effects") {
  const std::string source = R"(
[effects(io_out), effects(io_err), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

#include "test_semantics_entry_transforms_references.h"

TEST_CASE("capabilities transform rejects duplicates") {
  const std::string source = R"(
[capabilities(io_out), capabilities(asset_read), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform") != std::string::npos);
}

TEST_SUITE_END();

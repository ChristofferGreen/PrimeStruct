#include "../test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.transforms");

TEST_CASE("unknown return type fails and names the type") {
  const std::string source = R"(
[return<u32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error == "unknown return type: u32 on /main");
}

TEST_CASE("unknown struct-like return type names the missing type") {
  const std::string source = R"(
[return<NoSuchType>]
make() {
  return(NoSuchType{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error == "unknown return type: NoSuchType on /make");
}

TEST_CASE("unknown type inside a collection return type is named") {
  const char *shapes[] = {"vector<NoSuchType>",
                          "array<NoSuchType>",
                          "Buffer<NoSuchType>",
                          "vector<vector<NoSuchType>>"};
  for (const char *shape : shapes) {
    const std::string source = std::string("[return<") + shape +
                               ">]\nwrap() {\n  return(vector<i32>{})\n}\n\n" +
                               "[return<int>]\nmain() {\n  return(1i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error == "unknown return type: NoSuchType on /wrap");
  }
}

TEST_CASE("collection return types of known element types still validate") {
  const std::string source = R"(
[struct]
Item() {
  [i32] value{1i32}
}

[enum]
Color() {
  Red
  Green
}

[sum]
Shape {
  [i32] number
  [bool] flag
}

[return<vector<Item>> effects(heap_alloc)]
items() {
  return(vector<Item>{})
}

[return<vector<Shape>> effects(heap_alloc)]
shapes() {
  return(vector<Shape>{})
}

[return<vector<Color>> effects(heap_alloc)]
colors() {
  return(vector<Color>{})
}

[return<vector<T>> effects(heap_alloc)]
generic<T>() {
  return(vector<T>{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unknown type inside a Task return type is named") {
  const std::string source = R"(
[return<Task<NoSuchType>>]
wrap() {
  return(vector<i32>{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error == "unknown return type: NoSuchType on /wrap");
}

TEST_CASE("unknown type inside a map return type is named") {
  const std::string valueSource = R"(
[return<map<string, NoSuchType>>]
wrap() {
  return(map<string, i32>{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(valueSource, "/main", error));
  CHECK(error == "unknown return type: NoSuchType on /wrap");
}

TEST_CASE("unknown type nested two templates deep in a map is named") {
  const std::string source = R"(
[return<map<string, vector<NoSuchType>>>]
wrap() {
  return(map<string, i32>{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error == "unknown return type: NoSuchType on /wrap");
}

TEST_CASE("resolvable but unsupported return type keeps its message") {
  const std::string source = R"(
[return<map<string, void>>]
wrap() {
  return(map<string, i32>{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported return type on /wrap") != std::string::npos);
}

TEST_CASE("vector return of an enum, a struct and a primitive stays accepted") {
  const std::string source = R"(
[enum]
Colors() {
  Red
  Green
}

[struct]
Point() {
  [i32] x{0i32}
}

[return<vector<Colors>>]
colors() {
  return(vector<Colors>{})
}

[return<vector<Point>>]
points() {
  return(vector<Point>{})
}

[return<array<i32>>]
numbers() {
  return(array<i32>{1i32})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a defined sum inside a map return type is not called unknown") {
  const std::string source = R"(
[sum]
Item {
  [i32] plain
}

[return<map<string, Item>>]
wrap() {
  return(map<string, i32>{})
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown return type") == std::string::npos);
  CHECK(error.find("on /wrap") != std::string::npos);
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

TEST_CASE("canonical stdlib map returns report retired map helper diagnostics") {
  const std::string source = R"(
[public return<int>]
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
  REQUIRE(validateProgram(source, "/main", error));
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

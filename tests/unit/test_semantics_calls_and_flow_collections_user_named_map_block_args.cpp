#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("user definition named map accepts block arguments") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  map(4i32, 6i32) {
    assign(result, 7i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection builtin still rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(vector([value] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("builtin at validates on array literal call target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(4i32, 5i32), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin at on canonical map call target requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(map<i32, i32>(7i32, 8i32), 7i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("builtin at reordered canonical map receiver requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(7i32, map<i32, i32>(7i32, 8i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("user definition named vector call is not treated as builtin collection target") {
  const std::string source = R"(
[return<int>]
vector<T>([T] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(vector<i32>(9i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named map call is not treated as builtin collection target") {
  const std::string source = R"(
[return<int>]
map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(map<i32, i32>(7i32, 8i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named Map reordered receiver is not treated as builtin experimental map target") {
  const std::string source = R"(
[return<int>]
Map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(0i32, Map<i32, i32>(7i32, 8i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named Map call is not treated as builtin experimental map target") {
  const std::string source = R"(
[return<int>]
Map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(Map<i32, i32>(7i32, 8i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/at") != std::string::npos);
}


TEST_SUITE_END();

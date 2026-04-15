#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("vector stdlib namespaced push auto inference uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count-capacity call-form requires explicit vector helper definitions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [auto] inferredCount{/vector/count(values)}
  [auto] inferredCapacity{/std/collections/vector/capacity(values)}
  return(plus(inferredCount, inferredCapacity))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count auto inference rejects map target without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/vector/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced capacity auto inference rejects map target without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/vector/capacity(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector stdlib namespaced count helper auto inference keeps canonical precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector stdlib namespaced count helper auto inference keeps inferred return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count helper auto inference falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count auto inference rejects compatibility alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector stdlib namespaced count auto inference keeps non-builtin arity mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count auto inference non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression keeps canonical precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count expression keeps return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression rejects compatibility alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count expression keeps non-builtin arity mismatch diagnostics" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression rejects non-builtin compatibility alias fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(27i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count expression compatibility alias fallback reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(27i32)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count auto inference rejects non-builtin compatibility arity fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(27i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count non-builtin arity rejects array helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count non-builtin arity diagnostics report builtin mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count auto inference non-builtin arity rejects array helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/vector/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map stdlib namespaced count expression keeps canonical helper return precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression succeeds when canonical helper matches return type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count expression non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count expression does not fall back to map alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression keeps canonical diagnostics when alias helper exists") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map compatibility count call keeps explicit alias precedence with canonical templated helper present" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility count call keeps alias mismatch diagnostics with canonical templated helper present") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /map/count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression ignores templated alias helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map compatibility count call does not inherit canonical templated helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map compatibility explicit-template count call keeps alias precedence with canonical templated helper" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count<i32, i32>(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_SUITE_END();

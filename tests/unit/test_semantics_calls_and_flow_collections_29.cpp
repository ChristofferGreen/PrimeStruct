#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("wrapper-returned slash-method map access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
main() {
  return(count(wrapMap()./std/collections/map/at(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression inferred template fallback keeps alias diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count auto inference keeps canonical helper return precedence") {
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
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count auto inference falls back to map alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count auto inference succeeds when canonical helper matches return type") {
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
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count statement falls back to map alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  /std/collections/map/count(values, true)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count auto inference non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression keeps canonical precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced capacity expression keeps return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression rejects compatibility alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced capacity expression keeps non-builtin arity mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values, true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression uses canonical helper definition for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression follows alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 0i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression reports alias return mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 0i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced access expression uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression uses alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression reports alias mismatch for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced access expression uses canonical helper definition for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity auto inference keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/capacity(values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("access helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/get([vector<i32>] values, [string] index) {
  return(12i32)
}

[effects(heap_alloc), return<int>]
/string/get([string] values, [vector<i32>] index) {
  return(98i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [string] index{"payload"raw_utf8}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access helper auto inference follows alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access helper auto inference reports alias mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced access helper auto inference uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced access helper auto inference keeps receiver helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at([index] 1i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced access helper auto inference keeps inferred return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at([index] 1i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("map stdlib namespaced access helper auto inference falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at([index] 1i32, [values] values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("stdlib namespaced map at requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/at(values, 1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("stdlib namespaced map at does not inherit alias-only helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/at(values, 1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("stdlib namespaced map at_unsafe requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at_unsafe(values, 1i32)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("stdlib namespaced map at_unsafe does not inherit alias-only helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at_unsafe(values, 1i32)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("soa access helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([soa_vector<Particle>] values, [vector<i32>] index) {
  return(13i32)
}

[effects(heap_alloc), return<bool>]
/vector/get([vector<i32>] values, [soa_vector<Particle>] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] index{vector<i32>(0i32)}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

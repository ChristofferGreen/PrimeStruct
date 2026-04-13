#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("map wrapper temporary tryAt auto inference requires canonical helper definition") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  [auto] inferred{/std/collections/map/tryAt(wrapMap(), 1i32)}
  return(Result.ok(try(inferred)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("map wrapper temporary contains call validates canonical target classification") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  return(/std/collections/map/contains(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary contains auto inference uses canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  [auto] inferred{/std/collections/map/contains(wrapMap(), 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary contains call requires canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
main() {
  return(/std/collections/map/contains(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("map wrapper temporary contains auto inference requires canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
main() {
  [auto] inferred{/std/collections/map/contains(wrapMap(), 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("map compatibility at call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("map compatibility at call keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [bool] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility at_unsafe auto inference requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("map compatibility at_unsafe keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(23i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [bool] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor does not resolve map alias helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/map<T, U>([T] key, [U] value) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/map<i32, i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/map") != std::string::npos);
}

TEST_CASE("map unnamespaced count call resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map unnamespaced count auto inference resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map unnamespaced count call rejects explicit same-path alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map unnamespaced count call requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map unnamespaced count auto inference requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map at call resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at wrapper call resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at wrapper call requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("bare map at call ignores compatibility helper when canonical helper is absent") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("bare map at call prefers canonical helper over compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at_unsafe auto inference resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at_unsafe auto inference ignores compatibility helper when canonical helper is absent") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("bare map at_unsafe auto inference prefers canonical helper over compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at wrapper call resolves through same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapVector(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at call prefers canonical helper over compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at call rejects named arguments even through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector at call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("bare vector at auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at auto inference requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("bare vector at_unsafe auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at_unsafe auto inference requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_SUITE_END();

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("vector helper call-form expression builtin stays statement-only with named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([values] values, [value] 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper statement call-form user shadow accepts named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push([value] 3i32, [values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement prefers labeled named receiver") {
  const std::string source = R"(
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [soa_vector<Particle>] value) {
}

[effects(heap_alloc), return<void>]
/soa_vector/push([soa_vector<Particle> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [soa_vector<Particle>] payload{soa_vector<Particle>()}
  push([value] payload, [values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement labeled receiver does not fall back to non-receiver label") {
  const std::string source = R"(
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<void>]
/soa_vector/push([vector<i32> mut] values, [soa_vector<Particle>] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [soa_vector<Particle>] payload{soa_vector<Particle>()}
  push([value] payload, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector helper statement skips temp-leading positional receiver probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<void>]
/vector/push([vector<Counter> mut] values, [Counter] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Counter> mut] values{vector<Counter>(Counter())}
  push(makeCounter(), values)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/push") != std::string::npos);
}

TEST_CASE("vector helper statement validates on variadic vector pack receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate([args<vector<i32>>] values) {
  push(at(values, 0i32), 9i32)
  values[1i32].clear()
  values[2i32].remove_swap(0i32)
  return(plus(count(at(values, 0i32)),
              plus(capacity(values[1i32]),
                   count(values[2i32]))))
}

[effects(heap_alloc), return<int>]
main() {
  return(mutate(vector<i32>(1i32),
                vector<i32>(2i32, 3i32),
                vector<i32>(4i32, 5i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement validates on dereferenced variadic vector pack receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_refs([args<Reference<vector<i32>>>] values) {
  push(dereference(at(values, 0i32)), 9i32)
  dereference(at(values, 1i32)).clear()
  dereference(at(values, 2i32)).remove_swap(0i32)
  return(plus(count(dereference(at(values, 0i32))),
              plus(capacity(dereference(at(values, 1i32))),
                   count(dereference(at(values, 2i32))))))
}

[effects(heap_alloc), return<int>]
mutate_ptrs([args<Pointer<vector<i32>>>] values) {
  push(dereference(at(values, 0i32)), 9i32)
  dereference(at(values, 1i32)).clear()
  dereference(at(values, 2i32)).remove_swap(0i32)
  return(plus(count(dereference(at(values, 0i32))),
              plus(capacity(dereference(at(values, 1i32))),
                   count(dereference(at(values, 2i32))))))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] a0{vector<i32>(1i32)}
  [vector<i32> mut] a1{vector<i32>(2i32, 3i32)}
  [vector<i32> mut] a2{vector<i32>(4i32, 5i32)}
  [vector<i32> mut] b0{vector<i32>(1i32)}
  [vector<i32> mut] b1{vector<i32>(2i32, 3i32)}
  [vector<i32> mut] b2{vector<i32>(4i32, 5i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}
  [Pointer<vector<i32>>] p0{location(b0)}
  [Pointer<vector<i32>>] p1{location(b1)}
  [Pointer<vector<i32>>] p2{location(b2)}
  return(plus(mutate_refs(r0, r1, r2),
              mutate_ptrs(p0, p1, p2)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement user shadow resolves on variadic vector pack receivers") {
  const std::string source = R"(
[return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[return<int>]
mutate([args<vector<i32>>] values) {
  push(at(values, 0i32), 9i32)
  values[0i32].push(3i32)
  return(0i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(mutate(vector<i32>(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector at call-form helper shadow accepts reordered named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("at call-form helper shadow prefers labeled named receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [vector<i32>] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/map/at([map<string, i32>] values, [vector<i32>] index) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  [vector<i32>] index{vector<i32>(0i32)}
  return(at([index] index, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call-form labeled receiver does not fall back to non-receiver label") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [vector<i32>] index) {
  return(92i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  [vector<i32>] index{vector<i32>(0i32)}
  return(at([index] index, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector at call-form helper shadow accepts positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(1i32, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map at call-form helper shadow accepts string positional reordered arguments") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(81i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at call-form helper shadow prefers later map receiver over leading string") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(83i32)
}

[return<int>]
/string/at([string] values, [map<string, i32>] key) {
  return(84i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at_unsafe call-form helper shadow accepts string positional reordered arguments") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(82i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector at_unsafe call-form helper shadow accepts reordered named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector at call-form helper shadow skips temp-leading positional probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<Counter>]
/vector/at([vector<Counter>] values, [Counter] index) {
  return(index)
}

[effects(heap_alloc), return<Counter>]
main() {
  [vector<Counter>] values{vector<Counter>(Counter())}
  return(at(makeCounter(), values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("user definition named push with positional args is not treated as builtin") {
  const std::string source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  push(1i32, 2i32)
  return(push(3i32, 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named push accepts named arguments") {
  const std::string source = R"(
[return<int>]
push([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(push([value] 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method helper in expressions requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector clear named args require imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear([values] values)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("user definition named count accepts named arguments") {
  const std::string source = R"(
[return<int>]
count([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(count([value] 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named capacity accepts named arguments") {
  const std::string source = R"(
[return<int>]
capacity([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(capacity([value] 7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity builtin still rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity([value] values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("user definition named at accepts named arguments") {
  const std::string source = R"(
[return<int>]
at([i32] value, [i32] index) {
  return(plus(value, index))
}

[return<int>]
main() {
  return(at([value] 3i32, [index] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named at_unsafe accepts named arguments") {
  const std::string source = R"(
[return<int>]
at_unsafe([i32] value, [i32] index) {
  return(minus(value, index))
}

[return<int>]
main() {
  return(at_unsafe([value] 7i32, [index] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access builtin still rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at([value] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("user definition named vector accepts named arguments") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(vector([value] 9i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named map accepts named arguments") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  return(map([key] 4i32, [value] 6i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named array accepts named arguments") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(array([value] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named vector accepts block arguments") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  vector(9i32) {
    assign(result, 4i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named array accepts block arguments") {
  const std::string source = R"(
[return<int>]
array<T>([T] value) {
  return(1i32)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  array<i32>(2i32) {
    assign(result, 5i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_SUITE_END();

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("bare map call form statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  [bool] marker{true}
  count(values, marker) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map call form statement body arguments keep validating through canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  [bool] marker{true}
  count(values, marker) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map helper expression body arguments require canonical helper resolution") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(values.count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map call form expression body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(count(values, true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map call form expression body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(count(values, true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("reference-wrapped map helper receiver expression body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference-wrapped map helper receiver expression body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map namespaced count method expression body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(values./std/collections/map/count(true) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map namespaced at method expression body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(values./map/at(5i32) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map stdlib call form expression body arguments use canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(/std/collections/map/at_unsafe(values, 5i32) { 1i32 })
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced access alias chained method rejects canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).tag())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("vector namespaced access alias chained method keeps removed-alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).tag(1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK((error.find("unknown call target: /vector/at") != std::string::npos ||
         error.find("unknown call target: /std/collections/vector/at") != std::string::npos ||
         error.find("argument type mismatch for /i32/tag parameter self") != std::string::npos));
}

TEST_CASE("vector namespaced access alias field expression keeps struct receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK((error.find("unknown call target: /vector/at") != std::string::npos ||
         error.find("unknown call target: /std/collections/vector/at") != std::string::npos ||
         error.find("unable to infer return type on /project") != std::string::npos));
}

TEST_CASE("vector canonical access call keeps same-path struct receiver forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector canonical unsafe access call forwards field inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/std/collections/vector/at_unsafe(values, 2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map namespaced access call keeps canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map namespaced access alias chained method requires explicit alias definition") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("map namespaced access alias chained method rejects before downstream tag diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self, [bool] marker) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("map namespaced unsafe access alias chained method requires explicit alias definition") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at_unsafe(values, 2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("map namespaced unsafe access alias chained method rejects before downstream tag diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at_unsafe(values, 2i32).tag(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("vector namespaced access alias field expression keeps removed-alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32).value.tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("canonical vector constructor call infers canonical helper return kind") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector([i32] seed) {
  return(Marker(seed))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project() {
  return(/std/collections/vector/vector(9i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector constructor auto binding infers same-path helper return kind") {
  const std::string source = R"(
Box<T>() {
  [T] value{0i32}
}

[return<Box<T>>]
/std/collections/vector/vector<T>([T] seed) {
  return(Box<T>(seed))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] boxed{/std/collections/vector/vector<i32>(9i32)}
  return(boxed.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector constructor auto binding ignores experimental constructor rewrite") {
  const std::string source = R"(
Box<T>() {
  [T] value{0i32}
}

Other() {
  [i32] alt{0i32}
}

[return<Box<T>>]
/std/collections/vector/vector<T>([T] seed) {
  return(Box<T>(seed))
}

[return<Other>]
/std/collections/experimental_vector/vector<T>([T] seed) {
  return(Other(7i32))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] boxed{/std/collections/vector/vector<i32>(9i32)}
  return(boxed.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector constructor auto binding keeps same-path overload family") {
  const std::string source = R"(
Box() {
  [i32] value{0i32}
}

Other() {
  [i32] alt{0i32}
}

[return<Box>]
/std/collections/vector/vector([i32] seed) {
  return(Box(seed))
}

[return<Other>]
/std/collections/vector/vector([i32] first, [i32] second) {
  return(Other(plus(first, second)))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] boxed{/std/collections/vector/vector(9i32)}
  return(boxed.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector constructor helper return keeps non-vector count diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector([i32] seed) {
  return(Marker(seed))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/vector(9i32).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Marker/count") != std::string::npos);
}

TEST_CASE("templated canonical vector constructor helper return keeps non-vector count diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector<T>([T] seed) {
  return(Marker(9i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/vector<i32>(9i32).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Marker/count") != std::string::npos);
}

TEST_CASE("templated canonical vector constructor helper return keeps non-vector capacity diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector<T>([T] seed) {
  return(Marker(9i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/vector<i32>(9i32).capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Marker/capacity") != std::string::npos);
}

TEST_CASE("templated experimental vector constructor helper return keeps non-vector count diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/experimental_vector/vector<T>([T] seed) {
  return(Marker(9i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/experimental_vector/vector<i32>(9i32).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Marker/count") != std::string::npos);
}

TEST_CASE("vector constructor alias call keeps removed-alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector([i32] seed) {
  return(Marker(seed))
}

[return<Marker>]
project() {
  return(/vector/vector(9i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project().value)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/vector") != std::string::npos);
}

TEST_CASE("canonical vector constructor call no longer falls back to vectorPair helper") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vectorPair([i32] first, [i32] second) {
  return(Marker(plus(first, second)))
}

[return<Marker>]
project() {
  return(/std/collections/vector/vector(9i32, 11i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project().value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/vector") != std::string::npos);
}

TEST_CASE("explicit vector import supports bare stdlib constructor and namespaced helpers") {
  const std::string source = R"(
import /std/collections/vector

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 8i32, 15i32)}
  return(plus(/std/collections/vector/count(values),
      plus(/std/collections/vector/capacity(values),
          plus(/std/collections/vector/at(values, 0i32),
              /std/collections/vector/at_unsafe(values, 2i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit vector import keeps duplicate same-path ctor diagnostics") {
  const std::string source = R"(
import /std/collections/vector

Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector([i32] seed) {
  return(Marker(seed))
}

[effects(heap_alloc), return<int>]
main() {
  return(vector(9i32).value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /std/collections/vector/vector") != std::string::npos);
}

TEST_CASE("wildcard vector import supports concise vector binding example") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues() {
  [vector<int> mut] values{4, 8, 15}
  [int mut] total{0}
  [int] count{values.count()}

  for([int mut] index{0}; index < count; ++index) {
    total = total + values[index]
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method alias access rejects canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector method alias access field expression keeps removed alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK((error.find("unknown method: /vector/at") != std::string::npos ||
         error.find("unknown method: /std/collections/vector/at") != std::string::npos ||
         error.find("unknown call target: /vector/at") != std::string::npos ||
         error.find("unknown call target: /std/collections/vector/at") != std::string::npos ||
         error.find("unable to infer return type on /project") != std::string::npos));
}

TEST_CASE("vector unsafe method alias access struct method chain keeps primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at_unsafe(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_SUITE_END();

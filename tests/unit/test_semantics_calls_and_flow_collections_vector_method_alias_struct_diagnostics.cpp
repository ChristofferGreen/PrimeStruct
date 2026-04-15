#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("vector unsafe method alias access field expression keeps removed alias diagnostics") {
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
  return(values./vector/at_unsafe(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("vector method access with alias and canonical struct helpers infers the alias return") {
  const std::string source = R"(
AliasMarker {
  [i32] value
}

CanonicalMarker {
  [i32] value
}

[return<AliasMarker>]
/vector/at([vector<i32>] values, [i32] index) {
  return(AliasMarker(plus(index, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(CanonicalMarker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values.at(2i32).value)
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

TEST_CASE("vector method access reports current receiver diagnostics over canonical helper") {
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
  return(values.at(2i32).tag())
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

TEST_CASE("vector unsafe method access with canonical helper validates auto return") {
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
  return(values.at_unsafe(2i32).value)
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

TEST_CASE("map method access keeps canonical struct-return forwarding") {
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

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method access field expression keeps canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method access prefers canonical struct-return over alias helper") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method access reports canonical builtin result type over alias helper") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(plus(key, 40i32)))
}

[return<i32>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(key)
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map method alias access keeps primitive receiver diagnostics during inference") {
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

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("map method alias access keeps primitive argument diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("wrapper-returned map method alias access keeps primitive receiver diagnostics during inference") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<int>]
/i32/tag([i32] self) {
  return(plus(self, 40i32))
}

[return<auto>]
project() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("wrapper-returned map method alias access keeps primitive argument diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project() {
  return(wrapMap()./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("std-namespaced vector method alias access keeps helper receiver diagnostics during inference") {
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
  return(values./std/collections/vector/at(2i32).tag())
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

TEST_CASE("std-namespaced vector method alias access keeps helper missing-method diagnostics during inference") {
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
  return(values./std/collections/vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Marker/tag") != std::string::npos);
}

TEST_CASE("vector method alias access keeps removed-alias diagnostics") {
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
  return(values./vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("vector alias access auto wrapper keeps primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vector alias access auto wrapper keeps primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
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
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("templated stdlib canonical vector helpers reject method-call sugar template args on count") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

  [effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count<i32>(true), values.at<i32>(2i32)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("templated slash-path vector helper methods stay on unknown method diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

  [effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count<i32>(true), values./std/collections/vector/at<i32>(2i32)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("templated slash-path vector helper arity failures stay on unknown method diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./vector/count<i32>())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("templated stdlib canonical vector helper reports current argument mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_SUITE_END();

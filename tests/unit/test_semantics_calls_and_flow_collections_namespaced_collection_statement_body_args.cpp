#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("array namespaced slash method spelling block-arg diagnostics keep divide target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  values./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("array namespaced vector helper call form accepts statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [bool] marker{true}
  /array/count(values, marker) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced helper call form accepts statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [bool] marker{true}
  /vector/count(values, marker) { 1i32 }
  return(0i32)
  }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical vector helper call form accepts statement body arguments") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [bool] marker{true}
  /std/collections/vector/count(values, marker) { 1i32 }
  return(0i32)
  }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced vector helper call form rejects expression body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count(values, true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced helper call form expression body arguments currently pass semantics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true) { 1i32 })
  }
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /vector/count") !=
        std::string::npos);
}

TEST_CASE("stdlib canonical vector helper call-form expression body arguments reject compatibility fallback") {
  const std::string source = R"(
[return<bool>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true) { 1i32 })
  }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("array namespaced slash method pointer receiver diagnostics keep divide target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  ptr./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("stdlib namespaced method body-arg diagnostics normalize pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  ptr./std/collections/vector/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("array namespaced method expression body-arg diagnostics normalize pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr./array/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr./std/collections/vector/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("array namespaced slash method reference receiver diagnostics normalize reference receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference<i32>] ref{location(value)}
  ref./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") !=
        std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize reference receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(ref./std/collections/vector/count(true) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") !=
        std::string::npos);
}

TEST_CASE("array namespaced slash method temporary pointer diagnostics keep divide target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  location(value)./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize temporary pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(location(value)./std/collections/vector/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("array namespaced method body-arg diagnostics normalize helper-returned reference receiver target") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  borrow(location(value)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") !=
        std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize helper-returned reference receiver target") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") !=
        std::string::npos);
}

TEST_CASE("array namespaced method body-arg canonical-fallback helper keeps rooted borrow diagnostic") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  /vector/borrow(location(value)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/borrow") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg canonical-fallback helper keeps rooted borrow diagnostic") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(/vector/borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/borrow") != std::string::npos);
}

TEST_CASE("array namespaced method expression body-arg helper-returned reference keeps rooted borrow diagnostic") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
/Reference/count([Reference<i32>] self, [bool] marker) {
  return(7i32)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(/vector/borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/borrow") != std::string::npos);
}

TEST_CASE("array namespaced method expression body-arg helper mismatch keeps rooted borrow diagnostic") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
/Reference/count([Reference<i32>] self, [i32] marker) {
  return(7i32)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(/vector/borrow(location(value)).count(true) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/borrow") != std::string::npos);
}

TEST_CASE("map method expression body-arg infers canonical helper on referenced wrapper receiver") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(7i32)
}

[return<bool>]
/Reference/count([Reference</std/collections/map<i32, i32>>] self, [bool] marker) {
  return(false)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method expression body-arg referenced wrapper keeps canonical diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/Reference/count([Reference</std/collections/map<i32, i32>>] self, [bool] marker) {
  return(9i32)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated canonical map count wrapper method sugar rejects without explicit alias") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(7i32)
}

[return<int>]
main() {
  return(wrapValues().count(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map helper statement body arguments require canonical helper resolution") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values.count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map helper statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values.count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map helper statement body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values.count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("wrapper-returned bare map helper statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(5i32, 6i32))
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  wrapValues().count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned bare map helper statement body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(5i32, 6i32))
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  wrapValues().count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("reference-wrapped map helper receiver statement body arguments fall back to canonical helper target") {
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
  borrowMap(location(values)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference-wrapped map helper receiver statement body arguments keep canonical mismatch diagnostics") {
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
  borrowMap(location(values)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map namespaced count method statement body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values./std/collections/map/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /map/count") != std::string::npos);
}

TEST_CASE("map namespaced at method statement body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values./map/at(5i32) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /map/at") != std::string::npos);
}

TEST_CASE("map stdlib call form statement body arguments use canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  /std/collections/map/at_unsafe(values, 5i32) { 1i32 }
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

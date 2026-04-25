#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("wrapper-returned canonical map method ignores alias helper when canonical helper is absent") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(71i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map method requires helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib canonical map slash return type keeps canonical method diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(73i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib canonical map count method auto inference falls back to canonical helper return") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map count method auto inference keeps return mismatch diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib canonical map count method auto inference keeps canonical precedence over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map count method auto inference keeps canonical mismatch diagnostics over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib canonical map count call auto inference keeps canonical precedence over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
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

TEST_CASE("stdlib canonical map count call auto inference keeps canonical mismatch diagnostics over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib canonical map access count shadow keeps canonical precedence over alias helper") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/string/count([string] values) {
  return(5i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(1i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map access count shadow currently validates mixed canonical and alias returns") {
  const std::string source = R"(
[return<string>]
/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/string/count([string] values) {
  return(5i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(1i32).count()))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("rejects stdlib canonical vector helper method-precedence forwarding in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(40i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count(true), values.at(true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("array namespaced vector helper alias keeps unknown-method diagnostics for auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./array/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("array namespaced vector helper alias keeps unknown-method diagnostics for typed inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./array/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced helper alias keeps unknown-method auto-inference diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./vector/count(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced capacity alias keeps unknown-method auto-inference diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./vector/capacity(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("array namespaced vector capacity alias keeps unknown-method diagnostics for auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./array/capacity(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper alias uses same-path helper auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector access alias uses same-path helper auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/at(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector access alias method-call inference keeps return mismatch diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/at(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper alias method-call inference keeps return mismatch diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity alias uses same-path helper auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/capacity(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector capacity alias method-call inference keeps return mismatch diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/capacity(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access unsafe alias uses same-path helper auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/at_unsafe([vector<i32>] values, [bool] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/at_unsafe(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector access unsafe alias method-call inference keeps return mismatch diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/at_unsafe([vector<i32>] values, [bool] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/at_unsafe(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access slash method uses imported helper on vector receiver") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(0i32),
              values./std/collections/vector/at_unsafe(1i32)))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector access slash method uses imported helper diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./std/collections/vector/at(true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/at parameter index") !=
        std::string::npos);
}

TEST_CASE("stdlib namespaced vector access unsafe slash method uses imported helper diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./std/collections/vector/at_unsafe(true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/at_unsafe parameter index") !=
        std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method local same-path overload set rejects duplicate definitions") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(31i32)
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(32i32)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(33i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/count(),
                    items./std/collections/vector/count()),
              text./std/collections/vector/count()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects map receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects array receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(values./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects string receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method keeps local string same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([string] values) {
  return(37i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count method keeps local array same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(38i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./std/collections/vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count method keeps local map same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(39i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count method rejects incompatible local same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(39i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method rejects local string receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method rejects local array receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method rejects local map receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method rejects local string same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([string] values) {
  return(34i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method rejects local array same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([array<i32>] values) {
  return(35i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method rejects local map same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(36i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects wrapper map receiver without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects wrapper map same-path helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(47i32)
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method keeps wrapper array same-path helper") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(48i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count method keeps wrapper string same-path helper") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(49i32)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count method rejects wrapper array receiver without helper") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects wrapper string receiver without helper") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count method on builtin vector receiver requires same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method on builtin vector receiver rejects rooted helper fallback") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method wrapper vector chain keeps unknown-method diagnostics") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/count().tag())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("bare vector capacity method on builtin vector receiver validates without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector capacity method local same-path overload set rejects duplicate definitions") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(41i32)
}

[return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(42i32)
}

[return<int>]
/std/collections/vector/capacity([string] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/capacity(),
                    items./std/collections/vector/capacity()),
              text./std/collections/vector/capacity()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects map receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects array receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(values./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects string receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects local string same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([string] values) {
  return(44i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects local array same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(45i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects local map same-path helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(46i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("vector namespaced capacity method rejects local string receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity method rejects local array receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity method on builtin vector receiver requires same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./vector/capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector capacity method on builtin vector receiver rejects rooted helper fallback") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects wrapper map receiver without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects wrapper map same-path helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(48i32)
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects wrapper array same-path helper") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(49i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects wrapper string same-path helper") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/capacity([string] values) {
  return(50i32)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity method rejects wrapper map receiver without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/capacity") != std::string::npos);
}

TEST_CASE(
    "vector namespaced capacity slash method wrapper vector chain keeps unknown-method diagnostics") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/capacity().tag())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("array namespaced slash method spelling rejects statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

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

TEST_CASE("stdlib namespaced vector helper alias rejects statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  values./std/collections/vector/count(true) { 1i32 }
  return(0i32)
  }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_SUITE_END();

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("map compatibility explicit-template count call keeps non-templated alias diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
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
  CHECK(error.find("template arguments are only supported on templated definitions: /map/count") != std::string::npos);
}

TEST_CASE("map compatibility explicit-template count method resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count<i32, i32>(true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper reference templated map count method resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/Reference/count([Reference</std/collections/map<i32, i32>>] self) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([Reference</std/collections/map<K, V>>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(borrowValues(location(values)).count<i32, i32>(true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper reference templated map count method keeps canonical diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/Reference/count<K, V>([Reference</std/collections/map<K, V>>] self, [bool] marker) {
  return(7i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(borrowValues(location(values)).count<i32, i32>(true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map slash-path explicit-template count method stays on unknown method diagnostic") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./map/count<i32, i32>(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("map canonical slash-path explicit-template access method stays on unknown method diagnostic") {
  const std::string source = R"(
[effects(heap_alloc), return<i32>]
/std/collections/map/at<K, V>([map<K, V>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/map/at<i32, i32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("map canonical explicit-template count call keeps canonical non-templated diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count<i32, i32>(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("map canonical implicit-template count call keeps canonical non-templated diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map canonical implicit-template count call infers wrapper slash return envelope") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(), true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map canonical implicit-template count wrapper slash return keeps canonical diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map canonical wrapper auto local preserves collection template info") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32, 2i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues()}
  return(plus(plus(/std/collections/map/count(values), /std/collections/map/at(values, 1i32)),
              plus(/std/collections/map/at_unsafe(values, 2i32), values[1i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map canonical wrapper auto local keeps builtin count diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues()}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map canonical reference wrapper auto local preserves collection template info") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [auto] values{borrowValues(location(source))}
  return(plus(plus(/std/collections/map/count(values), /std/collections/map/at(values, 1i32)),
              plus(/std/collections/map/at_unsafe(values, 2i32), values[1i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map canonical reference wrapper auto local keeps key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  [auto] values{borrowValues(location(source))}
  return(/std/collections/map/at(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at") != std::string::npos);
}

TEST_CASE("canonical map borrowed receiver validates direct stdlib access") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/at(borrowValues(location(source)), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver keeps parameter key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowValues(location(source)), true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("canonical map borrowed receiver validates direct stdlib contains") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  if(/std/collections/map/contains(borrowValues(location(source)), 1i32),
     then(){ return(1i32) },
     else(){ return(0i32) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver keeps contains key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  if(/std/collections/map/contains(borrowValues(location(source)), true),
     then(){ return(1i32) },
     else(){ return(0i32) })
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver validates direct stdlib tryAt") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Result<i32, ContainerError>] found{/std/collections/map/tryAt(borrowValues(location(source)), 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver keeps tryAt key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  [Result<i32, ContainerError>] found{/std/collections/map/tryAt(borrowValues(location(source)), true)}
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit canonical map binding keeps builtin helper validation") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(plus(/std/collections/map/count(values), /std/collections/map/at(values, 1i32)),
              plus(/std/collections/map/at_unsafe(values, 2i32), values[1i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit canonical map parameter keeps builtin helper validation") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues([/std/collections/map<i32, i32>] values) {
  [auto] first{/std/collections/map/at(values, 1i32)}
  [auto] second{/std/collections/map/at_unsafe(values, 2i32)}
  [auto] total{/std/collections/map/count(values)}
  return(plus(plus(total, first), plus(second, values[1i32])))
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues(map<i32, i32>(1i32, 4i32, 2i32, 5i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit canonical map parameter keeps builtin key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues([/std/collections/map<i32, i32>] values) {
  return(/std/collections/map/at(values, true))
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues(map<i32, i32>(1i32, 4i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at") != std::string::npos);
}

TEST_CASE("explicit canonical map parameter keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), return<int>]
showValue([/std/collections/map<i32, string>] values) {
  print_line(values[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  return(showValue(map<i32, string>(1i32, "hello"utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map reference parameter keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), return<int>]
showValue([Reference</std/collections/map<i32, string>>] values) {
  print_line(values[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(showValue(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(io_out), return<int>]
showValue() {
  print_line(wrapMap()[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned referenced canonical map keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[effects(io_out), return<int>]
showValue() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  print_line(borrowMap(location(values))[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map reference parameter keeps if string branch compatibility") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
showValue([Reference</std/collections/map<i32, string>>] values) {
  [string] message{if(true, then(){ values[1i32] }, else(){ "fallback"utf8 })}
  return(count(message))
}

[return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(showValue(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map keeps if string branch compatibility") {
  const std::string source = R"(
import /std/collections/*

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<int>]
showValue() {
  [string] message{if(true, then(){ wrapMap()[1i32] }, else(){ "fallback"utf8 })}
  return(count(message))
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_SUITE_END();

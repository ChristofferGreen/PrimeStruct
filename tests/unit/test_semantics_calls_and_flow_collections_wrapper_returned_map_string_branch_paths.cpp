#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("wrapper-returned referenced canonical map keeps if string branch compatibility") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
showValue() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [string] message{if(true, then(){ borrowMap(location(values))[1i32] }, else(){ "fallback"utf8 })}
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

TEST_CASE("explicit canonical map parameter keeps pathspace string diagnostics") {
  const std::string source = R"(
[effects(pathspace_take), return<int>]
useValue([/std/collections/map<i32, i32>] values) {
  take(values[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  return(useValue(map<i32, i32>(1i32, 4i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("canonical map reference parameter keeps pathspace string diagnostics") {
  const std::string source = R"(
[effects(pathspace_take), return<int>]
useValue([Reference</std/collections/map<i32, i32>>] values) {
  take(values[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(useValue(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map keeps pathspace string diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(pathspace_take), return<int>]
useValue() {
  take(wrapMap()[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  return(useValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map keeps pathspace string diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(pathspace_take), return<int>]
useValue() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  take(borrowMap(location(values))[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  return(useValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("canonical map reference parameter keeps if string branch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
showValue([Reference</std/collections/map<i32, i32>>] values) {
  [string] message{if(true, then(){ values[1i32] }, else(){ "fallback"utf8 })}
  return(0i32)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(showValue(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map keeps if string branch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
showValue() {
  [string] message{if(true, then(){ wrapMap()[1i32] }, else(){ "fallback"utf8 })}
  return(0i32)
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map keeps if string branch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
showValue() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [string] message{if(true, then(){ borrowMap(location(values))[1i32] }, else(){ "fallback"utf8 })}
  return(0i32)
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map keeps builtin key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values))[true])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map access count call auto inference keeps string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("abc"utf8)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<bool>]
main() {
  [auto] inferred{count(wrapMap()[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map method access count keeps builtin string helper shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(7i32)
}

[return<i32>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(8i32)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<int>]
main() {
  return(plus(wrapMap().at(1i32).count(),
              wrapMap().at_unsafe(1i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned direct canonical map access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound map access count keeps builtin string fallback") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/string/count([string] values) {
  return(91i32)
}

Holder() {
  [map<i32, string>] values{map<i32, string>(1i32, "abc"utf8)}
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.values.at(1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map method access count keeps string receiver typing") {
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

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
main() {
  return(plus(wrapMap().at(1i32).count(),
              wrapMap().at_unsafe(1i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector access count call keeps builtin string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<string>] values{vector<string>("hello"utf8)}
  [auto] inferred{count(/std/collections/vector/at(values, 0i32))}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector unsafe access count call keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at_unsafe(values, 0i32)))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector method access count keeps builtin string fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"utf8)}
  return(values.at(0i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector unsafe method access count keeps primitive receiver diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(values.at_unsafe(0i32).count())
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("slash-method vector access count keeps builtin string fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"utf8)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("slash-method vector access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("wrapper-returned vector access count keeps builtin string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("hello"utf8))
}

[return<bool>]
main() {
  [auto] direct{count(/vector/at(wrapValues(), 0i32))}
  [auto] method{wrapValues().at_unsafe(0i32).count()}
  return(method)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned vector access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("wrapper-returned canonical map access count call auto inference keeps string helper mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<bool>]
/string/count([string] values) {
  return(false)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<int>]
main() {
  [auto] inferred{count(wrapMap()[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map access count call auto inference keeps string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("abc"utf8)
}

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<bool>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [auto] inferred{count(borrowMap(location(values))[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported wrapper-returned canonical map reference access count keeps string receiver typing") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("non-imported wrapper-returned canonical map reference access keeps primitive receiver diagnostics" * doctest::skip(true)) {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map access count call auto inference keeps string helper mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [auto] inferred{count(borrowMap(location(values))[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_SUITE_END();

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("wrapper temporary templated vector method arity mismatch reports canonical argument mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias keeps builtin count diagnostics when only canonical templated helper exists") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count capacity and access helpers validate as builtin aliases") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [i32] c{/vector/count(values)}
  [i32] k{/vector/capacity(values)}
  [i32] first{/vector/at(values, 0i32)}
  [i32] second{/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced vector count rejects template arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced capacity rejects template arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/capacity<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions") !=
        std::string::npos);
  CHECK(error.find("/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count([values] values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count and stdlib namespaced capacity accept same-path helpers on builtin vector target") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(14i32)
}

[return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(15i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(plus(/vector/count(values), /std/collections/vector/capacity(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count builtin vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count map target without helper reports unknown target") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(53i32)
}

[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count map target without helper reports unknown target") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapMap()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([string] values) {
  return(54i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapText()))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count string target without helper reports unknown target") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapText()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count accepts same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/vector/count([string] values) {
  return(44i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/vector/count(wrapText()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count string target without helper reports unknown target") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/vector/count(wrapText()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(55i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count array target without helper reports unknown target") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/count([array<i32>] values) {
  return(45i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count array target without helper reports unknown target") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/vector/count(wrapArray()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method rejects same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/vector/count([string] values) {
  return(46i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method rejects same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(47i32)
}

[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method map target without helper reports unknown method") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method string target without helper reports unknown method") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method rejects same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/count([array<i32>] values) {
  return(47i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method array target without helper reports unknown method") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced capacity slash method rejects same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([string] values) {
  return(46i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity slash method rejects same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([map<i32, i32>] values) {
  return(47i32)
}

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

TEST_CASE("vector namespaced capacity slash method rejects same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([array<i32>] values) {
  return(48i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity slash method map target without helper reports unknown method") {
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

TEST_CASE("vector namespaced capacity slash method string target without helper reports unknown method") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity slash method array target without helper reports unknown method") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/capacity())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced access slash method vector target without alias helper reports unknown method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(1i32),
              values./vector/at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK((error.find("/vector/at") != std::string::npos ||
         error.find("/vector/at_unsafe") != std::string::npos));
  CHECK((error.find("unknown method") != std::string::npos ||
         error.find("unknown call target") != std::string::npos));
}

TEST_CASE("vector namespaced access slash method vector target without canonical helper reports unknown method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("bare vector access method vector target without helper reports unknown method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.at(1i32),
              values.at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector access method wrapper target without helper reports unknown method") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapVector().at(1i32),
              wrapVector().at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("array compatibility access slash method vector target without helper reports unknown method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./array/at(1i32),
              values./array/at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/at") != std::string::npos);
}

TEST_CASE("array compatibility access slash method vector chain stops before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./array/at_unsafe(2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("array compatibility access slash method wrapper vector chain stops before receiver typing") {
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
  return(plus(wrapVector()./array/at(1i32).tag(),
              wrapVector()./array/at_unsafe(2i32).tag()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /array/at") != std::string::npos);
}

TEST_CASE("vector namespaced access slash method array target without alias helper reports unknown method") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(plus(wrapArray()./vector/at(1i32),
              wrapArray()./vector/at_unsafe(0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced access slash method string target without canonical helper reports unknown method") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/at(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("vector namespaced access slash method string target without alias helper reports unknown method") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/at(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(44i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced count accepts same-path helper on vector target") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(45i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/array/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced count accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(46i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/count(wrapVector(), true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced capacity accepts same-path helper on vector target") {
  const std::string source = R"(
[return<int>]
/array/capacity([vector<i32>] values, [bool] marker) {
  return(47i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/array/capacity(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced capacity accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/array/capacity([vector<i32>] values, [bool] marker) {
  return(48i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector(), true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced at accepts same-path helper on vector target") {
  const std::string source = R"(
[return<int>]
/array/at([vector<i32>] values, [i32] index, [bool] marker) {
  return(47i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/array/at(values, 1i32, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced at accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/array/at([vector<i32>] values, [i32] index, [bool] marker) {
  return(48i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced at_unsafe accepts same-path helper on vector target") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([vector<i32>] values, [i32] index, [bool] marker) {
  return(49i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/array/at_unsafe(values, 1i32, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced at_unsafe accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([vector<i32>] values, [i32] index, [bool] marker) {
  return(50i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at_unsafe(wrapVector(), 1i32, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced slash-method helpers accept same-path helper returns on vector receivers") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/array/count([vector<i32>] values) {
  return(77i32)
}

[return<int>]
/array/capacity([vector<i32>] values) {
  return(78i32)
}

[return<Marker>]
/array/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 10i32)))
}

[return<Marker>]
/array/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 20i32)))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(plus(values./array/count(), values./array/capacity()),
              plus(plus(values./array/at(1i32).tag(), values./array/at_unsafe(2i32).tag()),
                   plus(wrapVector()./array/at(0i32).tag(), wrapVector()./array/at_unsafe(1i32).tag()))))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced count wrapper vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapVector()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapVector()))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced count wrapper vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/count(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced count rejects named arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/count([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("array namespaced vector count call rejects named arguments via unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/count([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("array namespaced vector count builtin alias call is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/capacity([values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector capacity requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([map<i32, i32>] values) {
  return(42i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([array<i32>] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on builtin vector target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(44i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(44i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced capacity wrapper vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_SUITE_END();

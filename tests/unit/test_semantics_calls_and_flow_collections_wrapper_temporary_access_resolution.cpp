#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("slash-method access wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/at(1i32).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("map wrapper temporary count call validates target classification") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/map/count(wrapMapAuto()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary count method validates target classification") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(wrapMapAuto().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count validates on wrapper temporary vector target") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVectorAuto() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapVectorAuto()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count wrapper temporary vector target requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVectorAuto() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapVectorAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity wrapper temporary vector target requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVectorAuto() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/capacity(wrapVectorAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count rejects wrapper temporary map target") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapMapAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("wrapper temporary canonical vector count slash-method rejects map receiver") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(wrapMapAuto()./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("wrapper temporary canonical vector capacity slash-method rejects map receiver") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(wrapMapAuto()./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("map wrapper temporary access call validates map target classification") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  [i32] first{/std/collections/map/at(wrapMapAuto(), 1i32)}
  [i32] second{/std/collections/map/at_unsafe(wrapMapAuto(), 1i32)}
  return(plus(first, second))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary unsafe access validates direct stdlib helper") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/map/at_unsafe(wrapMapAuto(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary access keeps canonical key diagnostics") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/map/at(wrapMapAuto(), true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("map wrapper temporary bare helper calls validate target classification") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[return<int>]
main() {
  [i32] first{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] second{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] count{mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32))}
  return(plus(plus(first, second), count))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("declared canonical map access positional reorder keeps key diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at(1i32, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("map wrapper temporary capacity reports vector target diagnostic") {
  const std::string source = R"(
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(capacity(wrapMapAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("map wrapper temporary capacity method reports vector target diagnostic") {
  const std::string source = R"(
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(wrapMapAuto().capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("count preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(missing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("capacity preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  return(capacity(missing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("at preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  at(missing(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("vector access methods resolve through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(values.at(1i32), values.at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector access methods resolve through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapVector().at(1i32), wrapVector().at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector access methods keep imported helper argument diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(values.at(true), values.at_unsafe(true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/at parameter index") !=
        std::string::npos);
}

TEST_CASE("bare vector capacity call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector capacity call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("vector capacity compatibility alias rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity<i32>(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions") !=
        std::string::npos);
  CHECK(error.find("/vector/capacity") != std::string::npos);
}

TEST_CASE("vector capacity compatibility alias keeps rooted block-arg target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity(values) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /vector/capacity") !=
        std::string::npos);
}

TEST_CASE("vector capacity compatibility alias rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /vector/capacity") != std::string::npos);
}

TEST_CASE("capacity method on vector binding requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity wrapper temporaries keep current chained method failure boundary") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 50i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(capacity(wrapVector()).tag(), wrapVector().capacity().tag()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method target for tag") != std::string::npos);
}

TEST_CASE("wrapper vector capacity method keeps current imported compile boundary") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector capacity method requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("capacity method rejects extra arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("capacity rejects non-vector target") {
  const std::string source = R"(
[return<int>]
main() {
  return(capacity(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("capacity method rejects array target") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("count method keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(counter.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/count") != std::string::npos);
}

TEST_CASE("count method keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/count") != std::string::npos);
}

TEST_CASE("count call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(count(counter))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/count") != std::string::npos);
}

TEST_CASE("count call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(count(makeCounter()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/count") != std::string::npos);
}

TEST_CASE("capacity method keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(counter.capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity method keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(capacity(counter))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(capacity(makeCounter()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("at call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  at(counter, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("at call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  at(makeCounter(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("at_unsafe call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  at_unsafe(makeCounter(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at_unsafe") != std::string::npos);
}

TEST_CASE("push call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Counter mut] counter{Counter()}
  push(counter, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/push") != std::string::npos);
}

TEST_CASE("reserve method keeps unknown method on wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter mut] counter{Counter()}
  return(counter)
}

[effects(heap_alloc), return<int>]
main() {
  makeCounter().reserve(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/reserve") != std::string::npos);
}

TEST_CASE("at method validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call validates on map binding") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

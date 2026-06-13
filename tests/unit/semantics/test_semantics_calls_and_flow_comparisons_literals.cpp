#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.comparisons_literals");

TEST_CASE("string comparisons validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "beta"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("string comparisons reject mixed types") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed string/numeric operands") != std::string::npos);
}

TEST_CASE("builtin at map string comparisons validate") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "one"utf8)}
  return(equal(at(values, 1i32), 1i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin map at comparisons allow root at fallback") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
at([map<i32, string>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "one"utf8)}
  return(equal(at(values, 1i32), 1i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic operators reject bool operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(true, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
}

TEST_CASE("comparisons reject non-numeric operands") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<bool>]
main() {
  [thing] item{1i32}
  return(equal(item, item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("comparisons allow bool with signed integers") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("comparisons reject bool with unsigned") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed signed/unsigned operands") != std::string::npos);
}

TEST_CASE("comparisons reject mixed int and float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i32, 2.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed int/float operands") != std::string::npos);
}

TEST_CASE("stdlib map constructor validates through imported helper") {
  const std::string source = R"(
import /std/collections/map

[return<int>]
use([map<i32, i32>] x) {
  return(count(x))
}

[effects(heap_alloc), return<int>]
main() {
  return(use(/std/collections/map/map<i32, i32>(1i32, 2i32)))
}
)";
  std::string error;
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructor bool keys and values hit current if-condition diagnostic") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [map<bool, bool>] values{/std/collections/map/map<bool, bool>(true, false)}
  if(at_unsafe(values, true),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if condition requires bool") != std::string::npos);
}

TEST_CASE("array literal validates single element argument passing") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array<i32>(1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array literal missing template arg fails") {
  const std::string source = R"(
[return<int>]
use([array<i32>] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("vector literal missing template arg fails") {
  const std::string source = R"(
[return<int>]
use([vector<i32>] x) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(use(vector(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("collection literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("soa literal missing template arg fails") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  soa(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("soa literal validates when element type is soa-safe struct") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  soa<Particle>(Particle(1i32))
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa literal requires struct element type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  soa<i32>(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa literal requires struct element type") != std::string::npos);
}

TEST_CASE("soa literal rejects string element field envelope") {
  const std::string source = R"(
Particle() {
  [string] name{"n"utf8}
}

[effects(heap_alloc), return<int>]
main() {
  soa<Particle>(Particle("a"utf8))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/name: string") != std::string::npos);
}

TEST_CASE("soa literal rejects nested template element field envelope") {
  const std::string source = R"(
Particle() {
  [array<i32>] values{array<i32>(1i32)}
}

[effects(heap_alloc), return<int>]
main() {
  soa<Particle>(Particle(array<i32>(2i32)))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/values: array<i32>") != std::string::npos);
}

TEST_CASE("soa literal rejects nested struct disallowed envelope") {
  const std::string source = R"(
Label() {
  [string] text{"tag"utf8}
}

Particle() {
  [Label] meta{Label("default"utf8)}
}

[effects(heap_alloc), return<int>]
main() {
  soa<Particle>(Particle(Label("runtime"utf8)))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/meta/text: string") != std::string::npos);
}

TEST_CASE("soa literal accepts nested primitive struct fields") {
  const std::string source = R"(
Meta() {
  [i32] id{1i32}
}

Particle() {
  [Meta] meta{Meta(1i32)}
}

[effects(heap_alloc), return<int>]
main() {
  soa<Particle>(Particle(Meta(2i32)))
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa literal requires heap_alloc effect when non-empty") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  soa<Particle>(Particle(2i32))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("array literal type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([array<i32>] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array<i32>(1i32, "hi"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires element type i32") != std::string::npos);
}

TEST_CASE("vector literal type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([vector<i32>] x) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(use(vector<i32>(1i32, "hi"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("collection literal requires element type i32") != std::string::npos);
}

TEST_CASE("array literal rejects software numeric type") {
  const std::string source = R"(
[return<int>]
main() {
  array<decimal>(convert<decimal>(1.0f32))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: decimal") != std::string::npos);
}

TEST_CASE("vector literal rejects software numeric type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<decimal>(convert<decimal>(1.0f32))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: decimal") != std::string::npos);
}

TEST_CASE("map constructor without import fails ordinary resolution") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map(1i32, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("unknown call target") != std::string::npos);
  CHECK(error.find("map literal") == std::string::npos);
}

TEST_CASE("map constructor rejects software numeric types") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  /std/collections/map/map<integer, i32>(convert<integer>(1i32), 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("map literal") == std::string::npos);
}

TEST_CASE("map constructor key type mismatch fails") {
  const std::string source = R"(
import /std/collections/map

[return<int>]
use([map<i32, i32>] x) {
  return(count(x))
}

[effects(heap_alloc), return<int>]
main() {
  return(use(/std/collections/map/map<i32, i32>("a"utf8, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("map literal") == std::string::npos);
}

TEST_CASE("map constructor value type mismatch fails") {
  const std::string source = R"(
import /std/collections/map

[return<int>]
use([map<string, i32>] x) {
  return(count(x))
}

[effects(heap_alloc), return<int>]
main() {
  return(use(/std/collections/map/map<string, i32>("a"utf8, "b"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("map literal") == std::string::npos);
}

TEST_CASE("map constructor odd raw argument count uses ordinary argument diagnostics") {
  const std::string source = R"(
import /std/collections/map

[return<int>]
use([map<i32, i32>] x) {
  return(count(x))
}

[effects(heap_alloc), return<int>]
main() {
  return(use(/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map access validates key type") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32)}
  return(at(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("map literal") == std::string::npos);
}

TEST_CASE("map constructor access validates") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map constructor access validates for string keys") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("a"utf8, 2i32)}
  return(at_unsafe(values, "a"utf8))
}
)";
  std::string error;
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe map access validates key type") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("map literal") == std::string::npos);
}

TEST_CASE("string map access rejects numeric index") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 3i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires string map key") != std::string::npos);
}

TEST_CASE("unsafe string map access rejects numeric index") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 3i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires string map key") != std::string::npos);
}

TEST_CASE("map access accepts matching key type") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 3i32)}
  return(at(values, "a"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map access accepts string key expression") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 1i32)}
  [map<i32, string>] keys{map<i32, string>(1i32, "a"utf8)}
  return(at(values, at(keys, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe map access accepts string key expression") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 1i32)}
  [map<i32, string>] keys{map<i32, string>(1i32, "a"utf8)}
  return(at_unsafe(values, at(keys, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

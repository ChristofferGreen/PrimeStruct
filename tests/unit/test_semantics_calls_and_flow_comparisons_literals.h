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

TEST_CASE("builtin at map string comparisons reject mixed types") {
  const std::string source = R"(
[return<bool>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "one"utf8)}
  return(equal(at(values, 1i32), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed string/numeric operands") != std::string::npos);
}

TEST_CASE("user-defined at comparisons use resolved return type") {
  const std::string source = R"(
[return<int>]
at([map<i32, string>] values, [i32] key) {
  return(key)
}

[return<bool>]
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

TEST_CASE("map literal validates") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map literal validates bool keys and values") {
  const std::string source = R"(
[return<int>]
main() {
  map<bool, bool>(true, false)
  return(1i32)
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
  CHECK(error.find("vector literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("soa_vector literal missing template arg fails") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  soa_vector(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("soa_vector literal emits deterministic unsupported diagnostic") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  soa_vector<Particle>(Particle(1i32))
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector is not implemented yet") != std::string::npos);
}

TEST_CASE("soa_vector literal requires struct element type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  soa_vector<i32>(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector literal requires struct element type") != std::string::npos);
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
  CHECK(error.find("vector literal requires element type i32") != std::string::npos);
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

TEST_CASE("map literal missing template args fails") {
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
  CHECK(error.find("map literal requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("map literal rejects software numeric types") {
  const std::string source = R"(
[return<int>]
main() {
  map<integer, i32>(convert<integer>(1i32), 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_CASE("map literal key type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>("a"utf8, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires key type i32") != std::string::npos);
}

TEST_CASE("map literal value type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<string, i32>("a"utf8, "b"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires value type i32") != std::string::npos);
}

TEST_CASE("map literal requires even argument count") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>(1i32, 2i32, 3i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires an even number of arguments") != std::string::npos);
}

TEST_CASE("map access validates key type") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("map literal access validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(map<i32, i32>(1i32, 2i32), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map literal access validates for string keys") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(map<string, i32>("a"utf8, 2i32), "a"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe map access validates key type") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires map key type i32") != std::string::npos);
}

TEST_CASE("string map access rejects numeric index") {
  const std::string source = R"(
[return<int>]
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
[return<int>]
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
[return<int>]
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
[return<int>]
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
[return<int>]
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

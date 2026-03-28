TEST_SUITE_BEGIN("primestruct.semantics.transforms");

TEST_CASE("unsupported return type fails") {
  const std::string source = R"(
[return<u32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported return type") != std::string::npos);
}

TEST_CASE("array return requires template argument") {
  const std::string source = R"(
[return<array>]
main() {
  return(array<i32>{1i32})
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array return type requires exactly one template argument") != std::string::npos);
}

TEST_CASE("template vector and map returns are allowed") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<T>>]
wrapVector<T>([T] value) {
  return(vector<T>(value))
}

[effects(heap_alloc), return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(map<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector<i32>(9i32)}
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 3i32)}
  return(plus(count(values), count(pairs)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib map returns are allowed") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values) {
  return(1i32)
}

[effects(heap_alloc), return</std/collections/map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(map<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 3i32)}
  return(/std/collections/map/count(pairs))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector return rejects wrong template arity") {
  const std::string source = R"(
[return<vector<i32, i32>>]
main() {
  return(vector<i32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector return type requires exactly one template argument") != std::string::npos);
}

TEST_CASE("map return rejects wrong template arity") {
  const std::string source = R"(
[return<map<string>>]
main() {
  return(map<string, i32>("only"raw_utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map return type requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("map return rejects unsupported builtin Comparable key contract") {
  const std::string source = R"(
[return<map<array<i32>, i32>>]
main() {
  return(map<array<i32>, i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): array<i32>") !=
        std::string::npos);
}

TEST_CASE("canonical stdlib map return rejects wrong template arity") {
  const std::string source = R"(
[return</std/collections/map<string>>]
main() {
  return(map<string, i32>("only"raw_utf8, 1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /std/collections/map") !=
        std::string::npos);
}

TEST_CASE("soa_vector return rejects missing template arg") {
  const std::string source = R"(
[return<soa_vector>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector return type requires exactly one template argument on /main") !=
        std::string::npos);
}

TEST_CASE("soa_vector return type validates with soa-safe struct element type") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
main() {
  return(soa_vector<Particle>())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector return requires struct element type") {
  const std::string source = R"(
[return<soa_vector<i32>>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector return type requires struct element type on /main") != std::string::npos);
}

TEST_CASE("soa_vector return rejects disallowed element field envelope") {
  const std::string source = R"(
Particle() {
  [array<i32>] values{array<i32>(1i32)}
}

[return<soa_vector<Particle>>]
main() {
  return(soa_vector<Particle>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/values: array<i32>") != std::string::npos);
}

TEST_CASE("soa_vector return rejects nested struct disallowed envelope") {
  const std::string source = R"(
Meta() {
  [string] text{"meta"utf8}
}

Particle() {
  [Meta] meta{Meta("default"utf8)}
}

[return<soa_vector<Particle>>]
main() {
  return(soa_vector<Particle>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/meta/text: string") != std::string::npos);
}

#include "test_semantics_entry_transforms_references.h"

TEST_CASE("software numeric return type is rejected") {
  const std::string source = R"(
[return<complex>]
main() {
  [complex] value{convert<complex>(convert<decimal>(1.0f32))}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: complex") != std::string::npos);
}

TEST_CASE("duplicate return transform fails") {
  const std::string source = R"(
[return<int>, return<i32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate return transform") != std::string::npos);
}

TEST_CASE("single_type_to_return requires a type transform") {
  const std::string source = R"(
[single_type_to_return]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return requires a type transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects multiple type transforms") {
  const std::string source = R"(
[single_type_to_return i32 i64]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return requires a single type transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects duplicate markers") {
  const std::string source = R"(
[single_type_to_return single_type_to_return i32]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate single_type_to_return transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects return transform combo") {
  const std::string source = R"(
[single_type_to_return return<i32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return cannot be combined with return transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects template args") {
  const std::string source = R"(
[single_type_to_return<i32> i32]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return does not accept template arguments") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects arguments") {
  const std::string source = R"(
[single_type_to_return(1i32) i32]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return does not accept arguments") != std::string::npos);
}

TEST_CASE("single_type_to_return enforces non-void return") {
  const std::string source = R"(
[single_type_to_return i32]
main() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("missing return statement") != std::string::npos);
}

TEST_CASE("text group rejects non-text transform") {
  const std::string source = R"(
[text(return<int>)]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("text(...) group requires text transforms") != std::string::npos);
}

TEST_CASE("semantic group rejects text transform") {
  const std::string source = R"(
[semantic(operators) return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("text transform cannot appear in semantic(...) group") != std::string::npos);
}

TEST_CASE("bool return type validates") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("float return type validates") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.5f)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array return type validates") {
  const std::string source = R"(
[return<array<i32>>]
main() {
  return(array<i32>{1i32, 2i32})
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("int alias maps to i32") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i64)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("float alias maps to f32") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.0f64)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected f32") != std::string::npos);
}

TEST_CASE("return type mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(true)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
}

TEST_CASE("return type mismatch for array fails") {
  const std::string source = R"(
[return<array<i32>>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected array") != std::string::npos);
}

TEST_CASE("return type mismatch for bool fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
}

TEST_CASE("return transform rejects arguments in source syntax") {
  const std::string source = R"(
[return<int>(foo)]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return transform does not accept arguments") != std::string::npos);
}

TEST_CASE("effects transform validates identifiers") {
  const std::string source = R"(
[effects(global_write, io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("effects transform rejects template arguments") {
  const std::string source = R"(
[effects<io>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("effects transform rejects invalid capability") {
  const std::string source = R"(
[effects("io"utf8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("effects transform rejects duplicate capability") {
  const std::string source = R"(
[effects(io_out, io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("capabilities transform validates render and io identifiers") {
  const std::string source = R"(
[effects(render_graph, io_out), capabilities(render_graph, io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capabilities require matching effects") {
  const std::string source = R"(
[effects(io_out), capabilities(io_err), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capability requires matching effect") != std::string::npos);
}

TEST_CASE("capabilities transform rejects template arguments") {
  const std::string source = R"(
[capabilities<io>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("capabilities transform rejects invalid capability") {
  const std::string source = R"(
[capabilities("io"utf8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("capabilities transform rejects duplicate io_out capability") {
  const std::string source = R"(
[capabilities(io_out, io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("effects transform allows distinct transforms") {
  const std::string source = R"(
[effects(io_out), effects(asset_read), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("effects transform rejects duplicate effect names across transforms") {
  const std::string source = R"(
[effects(io_out), effects(io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform") != std::string::npos);
}

TEST_CASE("capabilities transform rejects duplicates") {
  const std::string source = R"(
[capabilities(io_out), capabilities(asset_read), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform") != std::string::npos);
}

TEST_SUITE_END();

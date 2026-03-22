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

TEST_CASE("reference return allows direct parameter reference") {
  const std::string source = R"(
[return<Reference<i32>>]
identity([Reference<i32>] input) {
  return(input)
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(identity(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference return rejects local reference escape") {
  const std::string source = R"(
[return<Reference<i32>>]
bad() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(ref)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return requires direct parameter reference") != std::string::npos);
}

TEST_CASE("reference return rejects derived parameter reference") {
  const std::string source = R"(
[return<Reference<i32>>]
bad([Reference<i32>] input) {
  [Reference<i32>] alias{location(input)}
  return(alias)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return requires direct parameter reference") != std::string::npos);
}

TEST_CASE("reference return requires matching template target") {
  const std::string source = R"(
[return<Reference<i32>>]
bad([Reference<i64>] input) {
  return(input)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return type mismatch") != std::string::npos);
}

TEST_CASE("unsafe definitions allow overlapping mutable references") {
  const std::string source = R"(
[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] refA{location(value)}
  [Reference<i32> mut] refB{location(value)}
  [i32] observed{dereference(refA)}
  assign(refA, 2i32)
  assign(refB, 3i32)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects safe-call boundary escape") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(ref)
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference allows unsafe-call boundary") {
  const std::string source = R"(
[unsafe, return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(ref)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe parameter reference allows named safe-call boundary") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
forward([Reference<i32>] input) {
  consume([input] input)
  return()
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  forward(ref)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects named safe-call boundary escape through call result") {
  const std::string source = R"(
[unsafe, return<Reference<i32>>]
forward([Reference<i32>] input) {
  return(input)
}

[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume([input] forward([input] ref))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe transform rejects duplicate markers") {
  const std::string source = R"(
[unsafe, unsafe, return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate unsafe transform") != std::string::npos);
}

TEST_CASE("unsafe transform rejects arguments") {
  const std::string source = R"(
[unsafe(flag), return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe does not accept arguments") != std::string::npos);
}

TEST_CASE("unsafe scope accepts reference location initialization") {
  const std::string source = R"(
[unsafe, return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  assign(value, 2i32)
  return(dereference(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe scope allows pointer to reference conversion initializer") {
  const std::string source = R"(
[unsafe, return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  return(dereference(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe scope rejects mismatched pointer to reference conversion") {
  const std::string source = R"(
[unsafe, return<int>]
main() {
  [i64 mut] value{1i64}
  [Pointer<i64>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe Reference binding type mismatch") != std::string::npos);
}

TEST_CASE("unsafe pointer-converted reference rejects safe-call boundary escape") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  consume(ref)
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through call result") {
  const std::string source = R"(
[unsafe, return<Reference<i32>>]
forward([Reference<i32>] input) {
  return(input)
}

[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(forward(ref))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through if expression") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(if(true, then(){ ref }, else(){ ref }))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through block return expression") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(block(){ return(ref) })
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through match expression") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [i32] selector{0i32}
  consume(match(selector, case(0i32) { ref }, else() { ref }))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe parameter reference allows safe-call boundary") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
forward([Reference<i32>] input) {
  consume(input)
  return()
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  forward(ref)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects assignment escape to outer binding") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32>] local{ptr}
  assign(out, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("unsafe reference allows assignment through local alias") {
  const std::string source = R"(
[unsafe, return<void>]
useLocal([Pointer<i32>] ptr) {
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] localOut{location(sinkValue)}
  [Reference<i32> mut] alias{location(localOut)}
  [Reference<i32>] local{ptr}
  assign(alias, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  useLocal(location(sourceValue))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference allows assignment through parameter-rooted pointer alias") {
  const std::string source = R"(
[unsafe, return<void>]
forward([Reference<i32> mut] out, [Reference<i32>] input) {
  [Pointer<i32>] aliasPtr{location(input)}
  [Reference<i32>] alias{aliasPtr}
  assign(out, alias)
  return()
}

[return<int>]
main() {
  [i32 mut] first{1i32}
  [i32 mut] second{2i32}
  [Reference<i32> mut] out{location(first)}
  [Reference<i32>] input{location(second)}
  forward(out, input)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects assignment escape through parameter alias") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32> mut] alias{location(out)}
  [Reference<i32>] local{ptr}
  assign(alias, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("unsafe reference rejects pointer-write escape through parameter alias") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32> mut] alias{location(out)}
  [Reference<i32>] local{ptr}
  assign(dereference(location(alias)), local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("unsafe reference rejects direct pointer-write escape to parameter") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32>] local{ptr}
  assign(dereference(out), local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("safe reference rejects assignment escape to parameter") {
  const std::string source = R"(
[return<void>]
leak([Reference<i32> mut] out) {
  [i32 mut] localValue{1i32}
  [Reference<i32>] local{location(localValue)}
  assign(out, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reference escapes via assignment to out") != std::string::npos);
  CHECK(error.find("root: localValue") != std::string::npos);
  CHECK(error.find("sink: out") != std::string::npos);
}

TEST_CASE("safe reference rejects assignment escape through match expression") {
  const std::string source = R"(
[return<void>]
leak([Reference<i32> mut] out) {
  [i32 mut] localValue{1i32}
  [Reference<i32>] local{location(localValue)}
  [i32] selector{0i32}
  assign(out, match(selector, case(0i32) { local }, else() { local }))
  return()
}

[return<int>]
main() {
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("safe reference allows assignment from parameter to parameter") {
  const std::string source = R"(
[return<void>]
forward([Reference<i32> mut] out, [Reference<i32>] input) {
  assign(out, input)
  return()
}

[return<int>]
main() {
  [i32 mut] first{1i32}
  [i32 mut] second{2i32}
  [Reference<i32> mut] out{location(first)}
  [Reference<i32>] input{location(second)}
  forward(out, input)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects assign before pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  assign(value, 2i32)
  [i32] observed{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] observed{dereference(ptr)}
  assign(value, 2i32)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects move before pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] moved{move(value)}
  [i32] observed{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows move after pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] observed{dereference(ptr)}
  [i32] moved{move(value)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

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

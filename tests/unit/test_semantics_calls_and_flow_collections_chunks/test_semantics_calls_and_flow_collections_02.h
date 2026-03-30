TEST_CASE("container error contract shape validates through Result.why") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[return<ContainerError>]
containerMissingKey() {
  return(ContainerError(1i32))
}

[return<void>]
main() {
  [Result<ContainerError>] status{ containerMissingKey().code }
  [Result<ContainerError>] unknown{ 99i32 }
  [string] message{ Result.why(status) }
  [auto] fallback{ Result.why(unknown) }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("containerErrorResult helper validates for value-carrying container results") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

[return<ContainerError>]
containerMissingKey() {
  return(ContainerError(1i32))
}

[return<Result<T, ContainerError>>]
containerErrorResult<T>([ContainerError] err) {
  return(multiply(convert<i64>(err.code), 4294967296i64))
}

[return<Result<i32, ContainerError>>]
main() {
  return(containerErrorResult<i32>(containerMissingKey()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported i32 container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<i32, ContainerError>>]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 7i32)}
  return(mapTryAt<string, i32>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported bool container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<bool, ContainerError>>]
main() {
  [map<string, bool>] values{map<string, bool>("left"raw_utf8, true)}
  return(mapTryAt<string, bool>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported string container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<string, ContainerError>>]
main() {
  [map<string, string>] values{map<string, string>("left"raw_utf8, "alpha"utf8)}
  return(mapTryAt<string, string>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector count wrapper call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count wrapper call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector capacity wrapper call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector capacity wrapper call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("count helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin validates on method calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method on vector binding accepts same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method on vector binding requires helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("count method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa_vector count validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(/soa_vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa_vector count slash-method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values./soa_vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib helpers validate on builtin soa_vector binding") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorCount<Particle>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa_vector stdlib helpers reject primitive element types") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[return<int>]
main() {
  [SoaVector<i32>] values{soaVectorNew<i32>()}
  return(soaVectorCount<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires struct type argument: i32") != std::string::npos);
}

TEST_CASE("experimental soa_vector stdlib helpers reject non-reflect struct element types") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorCount<Particle>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires reflect-enabled struct type argument: /Particle") != std::string::npos);
}

TEST_CASE("get helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa_vector get validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /soa_vector/get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get helper rejects non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa_vector target") != std::string::npos);
}

TEST_CASE("ref helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit soa_vector ref validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /soa_vector/ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref helper rejects non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ref requires soa_vector target") != std::string::npos);
}

TEST_CASE("to_soa helper validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa method validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  values.to_soa()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit to_soa slash-method validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  values./to_soa()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit to_aos slash-method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values./to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos validates on borrowed indexed soa_vector receiver") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
/score([args<Reference<soa_vector<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa and to_aos helpers compose for explicit conversion") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(count(to_aos(to_soa(values))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa helper rejects non-vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("to_soa requires vector target") != std::string::npos);
}

TEST_CASE("to_soa helper call-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_soa([soa_vector<Particle>] values) {
  return(5i32)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(to_soa(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa helper method-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_soa([vector<Particle>] values) {
  return(5i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(values.to_soa())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper rejects non-soa target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("to_aos requires soa_vector target") != std::string::npos);
}

TEST_CASE("to_aos helper call-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([vector<Particle>] values) {
  return(6i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(to_aos(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper method-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([soa_vector<Particle>] values) {
  return(6i32)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.to_aos())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("aos and soa containers do not implicitly convert") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/consumeSoa([soa_vector<Particle>] values) {
  return(0i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(consumeSoa(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/consumeSoa") != std::string::npos);
}

TEST_CASE("ecs style soa_vector update loop validates with deferred structural phase") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa_vector<Particle> mut] particles, [vector<Particle> mut] spawnQueue) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }

  [soa_vector<Particle>] stagedSpawns{to_soa(spawnQueue)}
  reserve(particles, plus(count(particles), count(stagedSpawns)))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] particles{soa_vector<Particle>()}
  [vector<Particle> mut] spawnQueue{vector<Particle>()}
  simulateStep(particles, spawnQueue)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ecs style update loop still requires explicit aos to soa conversion") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa_vector<Particle> mut] particles) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] particles{vector<Particle>()}
  simulateStep(particles)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/simulateStep") != std::string::npos);
}

#include "../test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("explicit old-surface to_aos direct call validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("to_aos method validates with soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values.to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported non-root to_aos forms validate with soa binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{values.to_aos()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector return accepts local builtin vector binding") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<vector<Particle>>]
copyValues() {
  [mut] out{vector<Particle>()}
  return(out)
}

[return<int>]
main() {
  [vector<Particle>] unpacked{copyValues()}
  return(count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit old-surface to_aos slash-method validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  values./to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("to_aos validates borrowed indexed retired soa receiver") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
/score([args<Reference<soa<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("to_soa and to_aos helpers reject removed canonical soa to_aos bridge") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
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
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("to_soa helper validates retired soa non-vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("to_soa requires vector target") !=
        std::string::npos);
}

TEST_CASE("to_soa helper call-form validates retired soa user-helper parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_soa([soa<Particle>] values) {
  return(5i32)
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(to_soa(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
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

TEST_CASE("non-root to_aos forms keep canonical reject on vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(values)
  values.to_aos()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/std/collections/soa/to_aos") != std::string::npos);
}

TEST_CASE("explicit old-surface to_aos_ref direct call validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  /to_aos_ref(location(values))
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("explicit old-surface to_aos_ref slash-method validates retired soa binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  location(values)./to_aos_ref()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("to_aos method fallback through struct helper return receivers validates removed canonical bridge") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  return(/std/collections/vector/count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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

TEST_CASE("to_aos method fallback validates with same-path helper present on struct helper return receivers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>] shadowed{vector<Particle>()}
  return(shadowed)
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  [vector<Particle>] unpacked{holder.cloneValues().to_aos()}
  return(/std/collections/vector/count(unpacked))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos method fallback keeps same-path helper shadow for auto inference through") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<int>]
/to_aos([SoaVector<Particle>] values) {
  return(7i32)
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] item{holder.cloneValues().to_aos()}
  return(item)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper method-form validates with soa user-helper parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([soa<Particle>] values) {
  return(6i32)
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(values.to_aos())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper-return builtin soa forms reject non-templated retired path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa<Particle>())
}

[return<int>]
/to_aos([soa<Particle>] values) {
  return(9i32)
}

[return<int>]
main() {
  [Holder] holder{Holder{}}
  [auto] itemA{to_aos(holder.cloneValues())}
  [auto] itemB{/to_aos(holder.cloneValues())}
  [auto] itemC{holder.cloneValues().to_aos()}
  [auto] itemD{holder.cloneValues()./to_aos()}
  return(plus(plus(itemA, itemB), plus(itemC, itemD)))
}
)";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected array") !=
        std::string::npos);
}

TEST_CASE("builtin soa global helper-return read helpers reject non-templated retired path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa<Particle>>]
cloneValues() {
  [soa<Particle>] values{soa<Particle>(Particle(7i32))}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] countBare{count(cloneValues())}
  [i32] countMethod{cloneValues().count()}
  [i32] getBare{get(cloneValues(), 0i32).x}
  [i32] getMethod{cloneValues().get(0i32).x}
  [i32] refBare{ref(cloneValues(), 0i32).x}
  [i32] refMethod{cloneValues().ref(0i32).x}
  return(plus(countBare,
              plus(countMethod,
                   plus(getBare,
                        plus(getMethod,
                             plus(refBare, refMethod))))))
}
  )";
  std::string error;
  INFO(error);
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: get") !=
        std::string::npos);
}

TEST_CASE("builtin soa method-like helper-return read helpers reject primitive metadata first") {
  const std::string source = R"(
Holder() {}

[effects(heap_alloc), return<soa<i32>>]
/Holder/cloneValues([Holder] self) {
  [soa<i32>, mut] values{soa<i32>()}
  values.push(7i32)
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [i32] countBare{count(holder.cloneValues())}
  [i32] countMethod{holder.cloneValues().count()}
  [i32] getBare{get(holder.cloneValues(), 0i32)}
  [i32] getMethod{holder.cloneValues().get(0i32)}
  [Reference<i32>] refBare{ref(holder.cloneValues(), 0i32)}
  [Reference<i32>] refMethod{holder.cloneValues().ref(0i32)}
  return(plus(countBare,
              plus(countMethod,
                   plus(getBare,
                        plus(getMethod,
                             plus(dereference(refBare),
                                  dereference(refMethod)))))))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa return type requires struct element type") !=
        std::string::npos);
}

TEST_CASE("builtin soa method-like helper-return read helpers reject non-reflect Particle metadata first") {
  const std::string source = R"(
import /std/collections/soa/*

Particle() {
  [i32] x{1i32}
}

Holder() {}

[effects(heap_alloc), return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  [soa<Particle>, mut] values{soa<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [soa<Particle>] cloned{holder.cloneValues()}
  return(count(cloned))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires reflect-enabled struct type argument: /Particle") !=
        std::string::npos);
}

TEST_CASE("aos and soa containers validate soa parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/consumeSoa([soa<Particle>] values) {
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
  INFO(error);
  CHECK(error.find("argument type mismatch") !=
        std::string::npos);
}

TEST_CASE("ecs style soa update loop validates retired parameter spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa<Particle> mut] particles, [vector<Particle> mut] spawnQueue) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }

  [soa<Particle>] stagedSpawns{to_soa(spawnQueue)}
  reserve(particles, plus(count(particles), count(stagedSpawns)))
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] particles{soa<Particle>()}
  [vector<Particle> mut] spawnQueue{vector<Particle>()}
  simulateStep(particles, spawnQueue)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown call target") !=
        std::string::npos);
}

TEST_CASE("ecs style update loop validates retired soa parameter before conversion mismatch") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa<Particle> mut] particles) {
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
  INFO(error);
  CHECK(error.find("argument type mismatch") !=
        std::string::npos);
}


TEST_SUITE_END();

#include <cstring>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/Ast.h"
#include "primec/Ir.h"
#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/Vm.h"
#include "test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.conversions");

TEST_CASE("ir lowerer materializes variadic borrowed vector packs with indexed dereference statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_refs([args<Reference<vector<i32>>>] values) {
  [vector<i32> mut] head{dereference(at(values, 0i32))}
  [vector<i32> mut] middle{dereference(at(values, 1i32))}
  [vector<i32> mut] tail{dereference(at(values, 2i32))}
  head.push(9i32)
  head.pop()
  middle.reserve(6i32)
  middle.clear()
  tail.remove_at(1i32)
  tail.remove_swap(0i32)
  return(plus(/std/collections/vector/count(head),
              plus(/std/collections/vector/capacity(middle),
                   /std/collections/vector/count(tail))))
}

[effects(heap_alloc), return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(mutate_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(20i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(mutate_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(mutate_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 25);
}

TEST_CASE("ir lowerer materializes variadic borrowed soa_vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
score_refs([args<Reference<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] head{dereference(at(values, 0i32))}
  [soa_vector<Particle>] tail{dereference(at(values, 2i32))}
  return(plus(count(values), plus(count(head), count(tail))))
}

[return<int>]
forward([args<Reference<soa_vector<Particle>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] extra{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [soa_vector<Particle>] a0{soa_vector<Particle>()}
  [soa_vector<Particle>] a1{soa_vector<Particle>()}
  [soa_vector<Particle>] a2{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] r0{location(a0)}
  [Reference<soa_vector<Particle>>] r1{location(a1)}
  [Reference<soa_vector<Particle>>] r2{location(a2)}

  [soa_vector<Particle>] b0{soa_vector<Particle>()}
  [soa_vector<Particle>] b1{soa_vector<Particle>()}
  [soa_vector<Particle>] b2{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] s0{location(b0)}
  [Reference<soa_vector<Particle>>] s1{location(b1)}
  [Reference<soa_vector<Particle>>] s2{location(b2)}

  [soa_vector<Particle>] c0{soa_vector<Particle>()}
  [soa_vector<Particle>] c1{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] t0{location(c0)}
  [Reference<soa_vector<Particle>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes variadic pointer soa_vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
score_ptrs([args<Pointer<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] head{dereference(at(values, 0i32))}
  [soa_vector<Particle>] tail{dereference(at(values, 2i32))}
  return(plus(count(values), plus(count(head), count(tail))))
}

[return<int>]
forward([args<Pointer<soa_vector<Particle>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] extra{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [soa_vector<Particle>] a0{soa_vector<Particle>()}
  [soa_vector<Particle>] a1{soa_vector<Particle>()}
  [soa_vector<Particle>] a2{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] r0{location(a0)}
  [Pointer<soa_vector<Particle>>] r1{location(a1)}
  [Pointer<soa_vector<Particle>>] r2{location(a2)}

  [soa_vector<Particle>] b0{soa_vector<Particle>()}
  [soa_vector<Particle>] b1{soa_vector<Particle>()}
  [soa_vector<Particle>] b2{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] s0{location(b0)}
  [Pointer<soa_vector<Particle>>] s1{location(b1)}
  [Pointer<soa_vector<Particle>>] s2{location(b2)}

  [soa_vector<Particle>] c0{soa_vector<Particle>()}
  [soa_vector<Particle>] c1{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] t0{location(c0)}
  [Pointer<soa_vector<Particle>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes variadic soa_vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
score_soas([args<soa_vector<Particle>>] values) {
  [soa_vector<Particle>] head{values[0i32]}
  [soa_vector<Particle>] tail{values[2i32]}
  return(plus(count(values), plus(count(head), count(tail))))
}

[return<int>]
forward([args<soa_vector<Particle>>] values) {
  return(score_soas([spread] values))
}

[return<int>]
forward_mixed([args<soa_vector<Particle>>] values) {
  return(score_soas(soa_vector<Particle>(), [spread] values))
}

[return<int>]
main() {
  return(plus(score_soas(soa_vector<Particle>(), soa_vector<Particle>(), soa_vector<Particle>()),
              plus(forward(soa_vector<Particle>(), soa_vector<Particle>(), soa_vector<Particle>()),
                   forward_mixed(soa_vector<Particle>(), soa_vector<Particle>()))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes variadic map packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_maps([args<map<i32, i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(13i32, 14i32),
                           map<i32, i32>(15i32, 16i32, 17i32, 18i32),
                           map<i32, i32>(19i32, 20i32)),
                   forward_mixed(map<i32, i32>(21i32, 22i32, 23i32, 24i32),
                                 map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic map packs with indexed tryAt inference") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_maps([args<map<i32, i32>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 8i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(1i32, 2i32, 3i32, 6i32),
                           map<i32, i32>(5i32, 6i32),
                           map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)),
                   forward_mixed(map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 60);
}

TEST_CASE("ir lowerer materializes variadic experimental map packs with indexed canonical count calls") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
score_maps([args<Map<i32, i32>>] values) {
  [Map<i32, i32>] head{at(values, 0i32)}
  [Map<i32, i32>] tail{at(values, 2i32)}
  return(plus(/std/collections/experimental_map/mapCount<i32, i32>(head),
              /std/collections/experimental_map/mapCount<i32, i32>(tail)))
}

[return<int> effects(heap_alloc)]
forward([args<Map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<Map<i32, i32>>] values) {
  return(score_maps(mapSingle(1i32, 2i32), [spread] values))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(score_maps(mapPair(1i32, 2i32, 3i32, 4i32),
                         mapSingle(5i32, 6i32),
                         mapTriple(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(mapSingle(13i32, 14i32),
                           mapPair(15i32, 16i32, 17i32, 18i32),
                           mapSingle(19i32, 20i32)),
                   forward_mixed(mapPair(21i32, 22i32, 23i32, 24i32),
                                 mapTriple(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_SUITE_END();

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

TEST_CASE("ir lowerer materializes variadic pointer array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] r0{location(a0)}
  [Pointer<array<i32>>] r1{location(a1)}
  [Pointer<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Pointer<array<i32>>] s0{location(b0)}
  [Pointer<array<i32>>] s1{location(b1)}
  [Pointer<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] t0{location(c0)}
  [Pointer<array<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  INFO(error);
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic pointer array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] r0{location(a0)}
  [Pointer<array<i32>>] r1{location(a1)}
  [Pointer<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Pointer<array<i32>>] s0{location(b0)}
  [Pointer<array<i32>>] s1{location(b1)}
  [Pointer<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] t0{location(c0)}
  [Pointer<array<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  INFO(error);
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  INFO(error);
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed dereference count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  INFO(error);
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed lookup helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{at(values, 0i32).at_unsafe(3i32)}
  if(at(values, 2i32).contains(11i32),
     then(){ return(plus(head, at(values, 2i32).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  INFO(error);
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 48);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 48);
}

TEST_CASE("ir lowerer rejects variadic pointer map packs with indexed helper inference") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find(
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/"
            "saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(error.find("call=/std/collections/map/at_unsafe") != std::string::npos);
}

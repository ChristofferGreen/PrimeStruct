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

TEST_CASE("ir lowerer materializes variadic struct pointer packs from borrowed pack reference fields") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[struct]
Holder() {
  [mut Pair] pair_storage{Pair{}}
  [Reference<Pair>] pair_ref{location(pair_storage)}
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_ptrs(location(values.at(0i32).pair_ref),
                    location(at(values, 1i32).pair_ref),
                    location(values.at_unsafe(2i32).pair_ref)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [Pair] extra_value{Pair{5i32}}
  [Holder] extra
  assign(extra.pair_storage, extra_value)
  return(score_ptrs(location(values.at(0i32).pair_ref),
                    location(extra.pair_ref),
                    location(at(values, 1i32).pair_ref)))
}

[return<int>]
main() {
  [Pair] a0{Pair{7i32}}
  [Pair] a1{Pair{8i32}}
  [Pair] a2{Pair{9i32}}
  [Holder] h0
  [Holder] h1
  [Holder] h2
  assign(h0.pair_storage, a0)
  assign(h1.pair_storage, a1)
  assign(h2.pair_storage, a2)
  [Reference<Holder>] r0{location(h0)}
  [Reference<Holder>] r1{location(h1)}
  [Reference<Holder>] r2{location(h2)}

  [Pair] b0{Pair{11i32}}
  [Pair] b1{Pair{12i32}}
  [Pair] b2{Pair{13i32}}
  [Holder] i0
  [Holder] i1
  [Holder] i2
  assign(i0.pair_storage, b0)
  assign(i1.pair_storage, b1)
  assign(i2.pair_storage, b2)
  [Reference<Holder>] s0{location(i0)}
  [Reference<Holder>] s1{location(i1)}
  [Reference<Holder>] s2{location(i2)}

  [Pair] c0{Pair{15i32}}
  [Pair] c1{Pair{17i32}}
  [Holder] j0
  [Holder] j1
  assign(j0.pair_storage, c0)
  assign(j1.pair_storage, c1)
  [Reference<Holder>] t0{location(j0)}
  [Reference<Holder>] t1{location(j1)}

  return(plus(score_from_fields(r0, r1, r2),
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
  CHECK(result == 75);
}

TEST_CASE("ir lowerer materializes variadic pointer uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Pointer<uninitialized<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_ptrs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_ptrs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
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
  CHECK(result == 27);
}

TEST_CASE("ir lowerer materializes variadic borrowed uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Reference<uninitialized<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_refs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
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
  CHECK(result == 27);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed dereference count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
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

TEST_CASE("ir lowerer rejects variadic borrowed map packs with indexed tryAt inference") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
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
  CHECK(error.find("native backend only supports arithmetic/comparison") !=
        std::string::npos);
  CHECK(error.find("call=/std/collections/map/at_unsafe") != std::string::npos);
}

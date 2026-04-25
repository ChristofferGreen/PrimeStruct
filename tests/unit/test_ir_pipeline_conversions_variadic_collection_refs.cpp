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

TEST_CASE("ir lowerer materializes variadic vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_vectors([args<vector<i32>>] values) {
  [vector<i32>] head{values[0i32]}
  [vector<i32>] tail{values[2i32]}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 99i32)}
  return(score_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32, 4i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [vector<i32>] c0{vector<i32>(11i32, 12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  return(plus(score_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer materializes variadic vector packs with indexed capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_vectors([args<vector<i32>>] values) {
  [vector<i32>] head{values[0i32]}
  [vector<i32>] tail{values[2i32]}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 99i32)}
  return(score_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32, 4i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [vector<i32>] c0{vector<i32>(11i32, 12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  return(plus(score_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer materializes variadic vector packs with indexed statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_vectors([args<vector<i32>>] values) {
  [vector<i32> mut] head{values[0i32]}
  [vector<i32> mut] middle{values[1i32]}
  [vector<i32> mut] tail{values[2i32]}
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
forward([args<vector<i32>>] values) {
  return(mutate_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(20i32)}
  return(mutate_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}

  [vector<i32>] b0{vectorSingle<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}

  return(plus(mutate_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer materializes variadic array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_arrays([args<array<i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<array<i32>>] values) {
  return(score_arrays([spread] values))
}

[return<int>]
forward_mixed([args<array<i32>>] values) {
  return(score_arrays(array<i32>(1i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_arrays(array<i32>(1i32, 2i32), array<i32>(3i32), array<i32>(4i32, 5i32, 6i32)),
              plus(forward(array<i32>(7i32), array<i32>(8i32, 9i32), array<i32>(10i32)),
                   forward_mixed(array<i32>(11i32, 12i32), array<i32>(13i32, 14i32, 15i32)))))
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
  CHECK(result == 7488);
}

TEST_CASE("ir lowerer materializes variadic borrowed array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

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

TEST_CASE("ir lowerer materializes variadic borrowed array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

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
  CHECK(result == 11344);
}

TEST_CASE("ir lowerer materializes variadic scalar reference packs with indexed dereference") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

[return<int>]
score_refs([args<Reference<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Reference<i32>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_refs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Reference<i32>] r0{location(a0)}
  [Reference<i32>] r1{location(a1)}
  [Reference<i32>] r2{location(a2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Reference<i32>] s0{location(b0)}
  [Reference<i32>] s1{location(b1)}
  [Reference<i32>] s2{location(b2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Reference<i32>] t0{location(c0)}
  [Reference<i32>] t1{location(c1)}

  return(plus(score_refs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
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
  CHECK(result == 1888);
}

TEST_CASE("ir lowerer rejects variadic struct reference packs with indexed field and helper access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<Reference<Pair>>]
borrow_ref([Reference<Pair>] value) {
  return(value)
}

[return<int>]
score_refs([args<Reference<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Reference<Pair>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_refs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_refs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
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
  CHECK(error == "variadic parameter type mismatch");
}

TEST_CASE("ir lowerer rejects variadic struct pointer packs with indexed field and helper access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<Reference<Pair>>]
borrow_ref([Reference<Pair>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Pointer<Pair>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_ptrs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
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
  CHECK(error == "variadic parameter type mismatch");
}

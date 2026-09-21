#include <cstring>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrLowerer.h"
#include "primec/ir/IrSerializer.h"
#include "../test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.conversions");

namespace {

void checkMaterializedIndirectVectorPack(const primec::IrModule &module,
                                         bool requireStore) {
  bool sawIndirectLoad = false;
  bool sawIndirectStore = false;
  for (const auto &function : module.functions) {
    for (const auto &instruction : function.instructions) {
      sawIndirectLoad = sawIndirectLoad || instruction.op == primec::IrOpcode::LoadIndirect;
      sawIndirectStore =
          sawIndirectStore || instruction.op == primec::IrOpcode::StoreIndirect;
    }
  }

  CHECK(sawIndirectLoad);
  if (requireStore) {
    CHECK(sawIndirectStore);
  }
}

} // namespace

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{/std/collections/vector/vector<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{/std/collections/vector/vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{/std/collections/vector/vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

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
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  checkMaterializedIndirectVectorPack(module, false);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed dereference capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{/std/collections/vector/vector<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{/std/collections/vector/vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{/std/collections/vector/vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

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
  INFO(error);
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  checkMaterializedIndirectVectorPack(module, false);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed dereference access helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 2i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{/std/collections/vector/vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{/std/collections/vector/vector<i32>(9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{/std/collections/vector/vector<i32>(12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

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
  INFO(error);
  // TODO-5302: a nested at_unsafe(dereference(at(values, i)), j)/
  // dereference(at(values, i)).at(j) chain on an args<Pointer<vector<i32>>>
  // pack element used to pin a real gap as "expected" (a rejection with
  // "native backend only supports at() on numeric/bool/string arrays or
  // vectors"). Confirmed via a standalone `primec --emit=vm` run of this
  // exact source (all three call shapes: a direct positional pack, a
  // forwarded spread pack, and a spread pack mixed with a local pointer
  // argument) that lowering now succeeds and the program runs to
  // completion with the arithmetically correct result - this test's
  // rejection expectation was stale, not a real compiler gap.
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  checkMaterializedIndirectVectorPack(module, false);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed dereference statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_ptrs([args<Pointer<vector<i32>>>] values) {
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
forward([args<Pointer<vector<i32>>>] values) {
  return(mutate_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{/std/collections/vector/vector<i32>(20i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(mutate_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{/std/collections/vector/vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{/std/collections/vector/vector<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(mutate_ptrs(r0, r1, r2),
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
  INFO(error);
  // TODO-4753: .remove_at(...)/.remove_swap(...) method-call sugar (like
  // .push/.pop/.reserve/.clear above them) now lowers correctly - this used
  // to pin a real gap (method-call sugar never resolved a monomorphized
  // definition for these two names specifically) as "expected" until fixed.
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  checkMaterializedIndirectVectorPack(module, true);
}


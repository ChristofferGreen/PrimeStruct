#include <cstring>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrLowerer.h"
#include "primec/ir/IrSerializer.h"
#include "primec/runtime/Vm.h"
#include "../test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.conversions");

TEST_CASE("ir lowerer materializes variadic scalar pointer packs with indexed dereference") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Pointer<i32>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
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
  CHECK(result == 23);
}

TEST_CASE("ir lowerer rejects variadic scalar pointer packs from borrowed pack access without local binding") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
score_from_at([args<Reference<i32>>] values) {
  return(score_ptrs(location(at(values, 0i32)),
                    location(values.at(1i32)),
                    location(values.at_unsafe(2i32))))
}

[return<int>]
forward([args<Reference<i32>>] values) {
  return(score_from_at([spread] values))
}

[return<int>]
forward_mixed([args<Reference<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_ptrs(location(at(values, 0i32)),
                    location(extra_ref),
                    location(values.at_unsafe(1i32))))
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

  return(plus(score_from_at(r0, r1, r2),
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
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("semantic-product method-call target missing lowered definition: /array/at") !=
        std::string::npos);
}

TEST_CASE("ir lowerer rejects variadic struct pointer packs from borrowed pack access without local binding") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
score_from_at([args<Reference<Pair>>] values) {
  return(score_ptrs(location(values.at(0i32)),
                    location(at(values, 1i32)),
                    location(values.at_unsafe(2i32))))
}

[return<int>]
forward([args<Reference<Pair>>] values) {
  return(score_from_at([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Pair>>] values) {
  [Pair] extra{Pair{5i32}}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_ptrs(location(values.at(0i32)),
                    location(extra_ref),
                    location(at(values, 1i32))))
}

[return<int>]
main() {
  [Pair] a0{Pair{7i32}}
  [Pair] a1{Pair{8i32}}
  [Pair] a2{Pair{9i32}}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair{11i32}}
  [Pair] b1{Pair{12i32}}
  [Pair] b2{Pair{13i32}}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair{15i32}}
  [Pair] c1{Pair{17i32}}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_from_at(r0, r1, r2),
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
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("semantic-product method-call target missing lowered definition: /array/at") !=
        std::string::npos);
}


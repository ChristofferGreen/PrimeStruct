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

#if defined(EACCES) && defined(ENOENT) && defined(EEXIST)

TEST_CASE("ir lowerer materializes variadic FileError packs with indexed why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_errors([args<FileError>] values) {\n"
      "  return(plus(count(values[0i32].why()), count(values[2i32].why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<FileError>] values) {\n"
      "  return(score_errors([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<FileError>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  return(score_errors(extra, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  return(plus(score_errors(a0, a1, a2),\n"
      "              plus(forward(b0, b1, b2),\n"
      "                   forward_mixed(c0, c1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes variadic borrowed FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer rejects prefix spread borrowed FileError packs") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs([spread] values, extra_ref))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("spread argument must be final") != std::string::npos);
}

TEST_CASE("ir lowerer materializes variadic pointer FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] r0{location(a0)}\n"
      "  [Pointer<FileError>] r1{location(a1)}\n"
      "  [Pointer<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] s0{location(b0)}\n"
      "  [Pointer<FileError>] s1{location(b1)}\n"
      "  [Pointer<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] t0{location(c0)}\n"
      "  [Pointer<FileError>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes variadic wrapped FileError packs with named free builtin at receivers") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(at([values] values, [index] 0i32).why()),\n"
      "              count(at([values] values, [index] minus(count(values), 1i32)).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_refs([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_refs_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(at([values] values, [index] 0i32).why()),\n"
      "              count(at([values] values, [index] minus(count(values), 1i32)).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_ptrs_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  [FileError] d0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] d1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] d2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] p0{location(d0)}\n"
      "  [Pointer<FileError>] p1{location(d1)}\n"
      "  [Pointer<FileError>] p2{location(d2)}\n"
      "  [FileError] e0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] e1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] e2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] q0{location(e0)}\n"
      "  [Pointer<FileError>] q1{location(e1)}\n"
      "  [Pointer<FileError>] q2{location(e2)}\n"
      "  [FileError] f0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] f1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] u0{location(f0)}\n"
      "  [Pointer<FileError>] u1{location(f1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward_refs(s0, s1, s2),\n"
      "                   plus(forward_refs_mixed(t0, t1),\n"
      "                        plus(score_ptrs(p0, p1, p2),\n"
      "                             plus(forward_ptrs(q0, q1, q2),\n"
      "                                  forward_ptrs_mixed(u0, u1)))))))\n"
      "}\n";
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
  CHECK(result == 72);
}


TEST_CASE("ir lowerer rejects prefix spread pointer FileError packs") {
  const std::string source =
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs([spread] values, extra_ptr))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] r0{location(a0)}\n"
      "  [Pointer<FileError>] r1{location(a1)}\n"
      "  [Pointer<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] s0{location(b0)}\n"
      "  [Pointer<FileError>] s1{location(b1)}\n"
      "  [Pointer<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] t0{location(c0)}\n"
      "  [Pointer<FileError>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("spread argument must be final") != std::string::npos);
}

#endif

TEST_SUITE_END();

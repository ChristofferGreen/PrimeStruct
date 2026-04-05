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

TEST_CASE("ir lowerer materializes variadic File handle packs with indexed file methods") {
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string pathA0 = "/tmp/primec_vm_variadic_file_handle_a0.txt";
  const std::string pathA1 = "/tmp/primec_vm_variadic_file_handle_a1.txt";
  const std::string pathA2 = "/tmp/primec_vm_variadic_file_handle_a2.txt";
  const std::string pathB0 = "/tmp/primec_vm_variadic_file_handle_b0.txt";
  const std::string pathB1 = "/tmp/primec_vm_variadic_file_handle_b1.txt";
  const std::string pathB2 = "/tmp/primec_vm_variadic_file_handle_b2.txt";
  const std::string pathC0 = "/tmp/primec_vm_variadic_file_handle_c0.txt";
  const std::string pathC1 = "/tmp/primec_vm_variadic_file_handle_c1.txt";
  const std::string pathExtra = "/tmp/primec_vm_variadic_file_handle_extra.txt";
  const auto readFile = [](const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  };

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_files([args<File<Write>>] values) {\n"
      "  values[0i32].write_line(\"alpha\"utf8)?\n"
      "  values[minus(count(values), 1i32)].write_line(\"omega\"utf8)?\n"
      "  values[0i32].flush()?\n"
      "  values[minus(count(values), 1i32)].flush()?\n"
      "  values[0i32].close()?\n"
      "  values[minus(count(values), 1i32)].close()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<File<Write>>] values) {\n"
      "  return(score_files([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<File<Write>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  return(score_files(extra, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  return(plus(score_files(a0, a1, a2), plus(forward(b0, b1, b2), forward_mixed(c0, c1))))\n"
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
  CHECK(result == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathC0).empty());
  CHECK(readFile(pathC1) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");

  const std::string wrappedPathA0 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_handle_a0.txt";
  const std::string wrappedPathA1 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_handle_a1.txt";
  const std::string wrappedPathB0 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_handle_b0.txt";
  const std::string wrappedPathB1 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_handle_b1.txt";
  const std::string wrappedPathC0 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_handle_c0.txt";
  const std::string wrappedPathExtra = "/tmp/primec_vm_variadic_wrapped_borrowed_file_handle_extra.txt";

  const std::string wrappedSource =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_refs([args<Reference<File<Write>>>] values) {\n"
      "  at([values] values, [index] 0i32).write_line(\"alpha\"utf8)?\n"
      "  at([values] values, [index] minus(count(values), 1i32)).write_line(\"omega\"utf8)?\n"
      "  at([values] values, [index] 0i32).flush()?\n"
      "  at([values] values, [index] minus(count(values), 1i32)).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Reference<File<Write>>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Reference<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(wrappedPathExtra) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(wrappedPathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(wrappedPathA1) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] r0{location(a0)}\n"
      "  [Reference<File<Write>>] r1{location(a1)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(wrappedPathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(wrappedPathB1) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] s0{location(b0)}\n"
      "  [Reference<File<Write>>] s1{location(b1)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(wrappedPathC0) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] t0{location(c0)}\n"
      "  return(plus(score_refs(r0, r1), plus(forward(s0, s1), forward_mixed(t0))))\n"
      "}\n";
  primec::Program wrappedProgram;
  primec::SemanticProgram wrappedSemanticProgram;
  error.clear();
  REQUIRE(parseAndValidate(wrappedSource, wrappedProgram, wrappedSemanticProgram, error));
  CHECK(error.empty());

  primec::IrModule wrappedModule;
  REQUIRE(lowerer.lower(wrappedProgram, &wrappedSemanticProgram, "/main", {}, {}, wrappedModule, error));
  CHECK(error.empty());

  result = 0;
  REQUIRE(vm.execute(wrappedModule, result, error));
  CHECK(error.empty());
  CHECK(result == 36);
  CHECK(readFile(wrappedPathA0) == "alpha\n");
  CHECK(readFile(wrappedPathA1) == "omega\n");
  CHECK(readFile(wrappedPathB0) == "alpha\n");
  CHECK(readFile(wrappedPathB1) == "omega\n");
  CHECK(readFile(wrappedPathC0) == "omega\n");
  CHECK(readFile(wrappedPathExtra) == "alpha\n");

  const std::string readPathA0 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_read_a0.txt";
  const std::string readPathA1 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_read_a1.txt";
  const std::string readPathB0 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_read_b0.txt";
  const std::string readPathB1 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_read_b1.txt";
  const std::string readPathC0 = "/tmp/primec_vm_variadic_wrapped_borrowed_file_read_c0.txt";
  const std::string readPathExtra = "/tmp/primec_vm_variadic_wrapped_borrowed_file_read_extra.txt";
  const auto writeFile = [](const std::string &path, const std::string &contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
  };
  writeFile(readPathA0, "A");
  writeFile(readPathA1, "C");
  writeFile(readPathB0, "B");
  writeFile(readPathB1, "D");
  writeFile(readPathC0, "F");
  writeFile(readPathExtra, "E");

  const std::string wrappedReadSource =
      "[effects(file_read)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "score_refs([args<Reference<File<Read>>>] values) {\n"
      "  [i32 mut] first{0i32}\n"
      "  [i32 mut] last{0i32}\n"
      "  at([values] values, [index] 0i32).read_byte(first)?\n"
      "  at([values] values, [index] minus(count(values), 1i32)).read_byte(last)?\n"
      "  return(plus(plus(first, last), count(values)))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Reference<File<Read>>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Reference<File<Read>>>] values) {\n"
      "  [File<Read>] extra{File<Read>(\"" + escape(readPathExtra) + "\"utf8)?}\n"
      "  [Reference<File<Read>>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Read>] a0{File<Read>(\"" + escape(readPathA0) + "\"utf8)?}\n"
      "  [File<Read>] a1{File<Read>(\"" + escape(readPathA1) + "\"utf8)?}\n"
      "  [Reference<File<Read>>] r0{location(a0)}\n"
      "  [Reference<File<Read>>] r1{location(a1)}\n"
      "  [File<Read>] b0{File<Read>(\"" + escape(readPathB0) + "\"utf8)?}\n"
      "  [File<Read>] b1{File<Read>(\"" + escape(readPathB1) + "\"utf8)?}\n"
      "  [Reference<File<Read>>] s0{location(b0)}\n"
      "  [Reference<File<Read>>] s1{location(b1)}\n"
      "  [File<Read>] c0{File<Read>(\"" + escape(readPathC0) + "\"utf8)?}\n"
      "  [Reference<File<Read>>] t0{location(c0)}\n"
      "  return(plus(score_refs(r0, r1), plus(forward(s0, s1), forward_mixed(t0))))\n"
      "}\n";
  primec::Program wrappedReadProgram;
  primec::SemanticProgram wrappedReadSemanticProgram;
  error.clear();
  REQUIRE(parseAndValidate(wrappedReadSource, wrappedReadProgram, wrappedReadSemanticProgram, error));
  CHECK(error.empty());

  primec::IrModule wrappedReadModule;
  REQUIRE(lowerer.lower(wrappedReadProgram, &wrappedReadSemanticProgram, "/main", {}, {}, wrappedReadModule, error));
  CHECK(error.empty());

  result = 0;
  REQUIRE(vm.execute(wrappedReadModule, result, error));
  CHECK(error.empty());
  CHECK(result == 411);
}

TEST_SUITE_END();
TEST_CASE("ir lowerer materializes variadic borrowed File handle packs with indexed dereference file methods") {
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string pathA0 = "/tmp/primec_vm_variadic_borrowed_file_handle_a0.txt";
  const std::string pathA1 = "/tmp/primec_vm_variadic_borrowed_file_handle_a1.txt";
  const std::string pathA2 = "/tmp/primec_vm_variadic_borrowed_file_handle_a2.txt";
  const std::string pathB0 = "/tmp/primec_vm_variadic_borrowed_file_handle_b0.txt";
  const std::string pathB1 = "/tmp/primec_vm_variadic_borrowed_file_handle_b1.txt";
  const std::string pathB2 = "/tmp/primec_vm_variadic_borrowed_file_handle_b2.txt";
  const std::string pathC0 = "/tmp/primec_vm_variadic_borrowed_file_handle_c0.txt";
  const std::string pathC1 = "/tmp/primec_vm_variadic_borrowed_file_handle_c1.txt";
  const std::string pathExtra = "/tmp/primec_vm_variadic_borrowed_file_handle_extra.txt";
  const auto readFile = [](const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  };

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_refs([args<Reference<File<Write>>>] values) {\n"
      "  dereference(values[0i32]).write_line(\"alpha\"utf8)?\n"
      "  dereference(values[minus(count(values), 1i32)]).write_line(\"omega\"utf8)?\n"
      "  dereference(values[0i32]).flush()?\n"
      "  dereference(values[minus(count(values), 1i32)]).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Reference<File<Write>>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Reference<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] r0{location(a0)}\n"
      "  [Reference<File<Write>>] r1{location(a1)}\n"
      "  [Reference<File<Write>>] r2{location(a2)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] s0{location(b0)}\n"
      "  [Reference<File<Write>>] s1{location(b1)}\n"
      "  [Reference<File<Write>>] s2{location(b2)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] t0{location(c0)}\n"
      "  [Reference<File<Write>>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))\n"
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
  CHECK(result == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathC0).empty());
  CHECK(readFile(pathC1) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");

  const std::string wrappedPathA0 = "/tmp/primec_vm_variadic_wrapped_pointer_file_handle_a0.txt";
  const std::string wrappedPathA1 = "/tmp/primec_vm_variadic_wrapped_pointer_file_handle_a1.txt";
  const std::string wrappedPathB0 = "/tmp/primec_vm_variadic_wrapped_pointer_file_handle_b0.txt";
  const std::string wrappedPathB1 = "/tmp/primec_vm_variadic_wrapped_pointer_file_handle_b1.txt";
  const std::string wrappedPathC0 = "/tmp/primec_vm_variadic_wrapped_pointer_file_handle_c0.txt";
  const std::string wrappedPathExtra = "/tmp/primec_vm_variadic_wrapped_pointer_file_handle_extra.txt";

  const std::string wrappedSource =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_ptrs([args<Pointer<File<Write>>>] values) {\n"
      "  at([values] values, [index] 0i32).write_line(\"alpha\"utf8)?\n"
      "  at([values] values, [index] minus(count(values), 1i32)).write_line(\"omega\"utf8)?\n"
      "  at([values] values, [index] 0i32).flush()?\n"
      "  at([values] values, [index] minus(count(values), 1i32)).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Pointer<File<Write>>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Pointer<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(wrappedPathExtra) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(wrappedPathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(wrappedPathA1) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] r0{location(a0)}\n"
      "  [Pointer<File<Write>>] r1{location(a1)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(wrappedPathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(wrappedPathB1) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] s0{location(b0)}\n"
      "  [Pointer<File<Write>>] s1{location(b1)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(wrappedPathC0) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] t0{location(c0)}\n"
      "  return(plus(score_ptrs(r0, r1), plus(forward(s0, s1), forward_mixed(t0))))\n"
      "}\n";
  primec::Program wrappedProgram;
  primec::SemanticProgram wrappedSemanticProgram;
  error.clear();
  REQUIRE(parseAndValidate(wrappedSource, wrappedProgram, wrappedSemanticProgram, error));
  CHECK(error.empty());

  primec::IrModule wrappedModule;
  REQUIRE(lowerer.lower(wrappedProgram, &wrappedSemanticProgram, "/main", {}, {}, wrappedModule, error));
  CHECK(error.empty());

  result = 0;
  REQUIRE(vm.execute(wrappedModule, result, error));
  CHECK(error.empty());
  CHECK(result == 36);
  CHECK(readFile(wrappedPathA0) == "alpha\n");
  CHECK(readFile(wrappedPathA1) == "omega\n");
  CHECK(readFile(wrappedPathB0) == "alpha\n");
  CHECK(readFile(wrappedPathB1) == "omega\n");
  CHECK(readFile(wrappedPathC0) == "omega\n");
  CHECK(readFile(wrappedPathExtra) == "alpha\n");

  const std::string readPathA0 = "/tmp/primec_vm_variadic_wrapped_pointer_file_read_a0.txt";
  const std::string readPathA1 = "/tmp/primec_vm_variadic_wrapped_pointer_file_read_a1.txt";
  const std::string readPathB0 = "/tmp/primec_vm_variadic_wrapped_pointer_file_read_b0.txt";
  const std::string readPathB1 = "/tmp/primec_vm_variadic_wrapped_pointer_file_read_b1.txt";
  const std::string readPathC0 = "/tmp/primec_vm_variadic_wrapped_pointer_file_read_c0.txt";
  const std::string readPathExtra = "/tmp/primec_vm_variadic_wrapped_pointer_file_read_extra.txt";
  const auto writeFile = [](const std::string &path, const std::string &contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
  };
  writeFile(readPathA0, "A");
  writeFile(readPathA1, "C");
  writeFile(readPathB0, "B");
  writeFile(readPathB1, "D");
  writeFile(readPathC0, "F");
  writeFile(readPathExtra, "E");

  const std::string wrappedReadSource =
      "[effects(file_read)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "score_ptrs([args<Pointer<File<Read>>>] values) {\n"
      "  [i32 mut] first{0i32}\n"
      "  [i32 mut] last{0i32}\n"
      "  at([values] values, [index] 0i32).read_byte(first)?\n"
      "  at([values] values, [index] minus(count(values), 1i32)).read_byte(last)?\n"
      "  return(plus(plus(first, last), count(values)))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Pointer<File<Read>>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Pointer<File<Read>>>] values) {\n"
      "  [File<Read>] extra{File<Read>(\"" + escape(readPathExtra) + "\"utf8)?}\n"
      "  [Pointer<File<Read>>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Read>] a0{File<Read>(\"" + escape(readPathA0) + "\"utf8)?}\n"
      "  [File<Read>] a1{File<Read>(\"" + escape(readPathA1) + "\"utf8)?}\n"
      "  [Pointer<File<Read>>] r0{location(a0)}\n"
      "  [Pointer<File<Read>>] r1{location(a1)}\n"
      "  [File<Read>] b0{File<Read>(\"" + escape(readPathB0) + "\"utf8)?}\n"
      "  [File<Read>] b1{File<Read>(\"" + escape(readPathB1) + "\"utf8)?}\n"
      "  [Pointer<File<Read>>] s0{location(b0)}\n"
      "  [Pointer<File<Read>>] s1{location(b1)}\n"
      "  [File<Read>] c0{File<Read>(\"" + escape(readPathC0) + "\"utf8)?}\n"
      "  [Pointer<File<Read>>] t0{location(c0)}\n"
      "  return(plus(score_ptrs(r0, r1), plus(forward(s0, s1), forward_mixed(t0))))\n"
      "}\n";
  primec::Program wrappedReadProgram;
  primec::SemanticProgram wrappedReadSemanticProgram;
  error.clear();
  REQUIRE(parseAndValidate(wrappedReadSource, wrappedReadProgram, wrappedReadSemanticProgram, error));
  CHECK(error.empty());

  primec::IrModule wrappedReadModule;
  REQUIRE(lowerer.lower(wrappedReadProgram, &wrappedReadSemanticProgram, "/main", {}, {}, wrappedReadModule, error));
  CHECK(error.empty());

  result = 0;
  REQUIRE(vm.execute(wrappedReadModule, result, error));
  CHECK(error.empty());
  CHECK(result == 411);
}

TEST_CASE("ir lowerer materializes variadic pointer File handle packs with indexed dereference file methods") {
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string pathA0 = "/tmp/primec_vm_variadic_pointer_file_handle_a0.txt";
  const std::string pathA1 = "/tmp/primec_vm_variadic_pointer_file_handle_a1.txt";
  const std::string pathA2 = "/tmp/primec_vm_variadic_pointer_file_handle_a2.txt";
  const std::string pathB0 = "/tmp/primec_vm_variadic_pointer_file_handle_b0.txt";
  const std::string pathB1 = "/tmp/primec_vm_variadic_pointer_file_handle_b1.txt";
  const std::string pathB2 = "/tmp/primec_vm_variadic_pointer_file_handle_b2.txt";
  const std::string pathC0 = "/tmp/primec_vm_variadic_pointer_file_handle_c0.txt";
  const std::string pathC1 = "/tmp/primec_vm_variadic_pointer_file_handle_c1.txt";
  const std::string pathExtra = "/tmp/primec_vm_variadic_pointer_file_handle_extra.txt";
  const auto readFile = [](const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  };

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_ptrs([args<Pointer<File<Write>>>] values) {\n"
      "  dereference(values[0i32]).write_line(\"alpha\"utf8)?\n"
      "  dereference(values[minus(count(values), 1i32)]).write_line(\"omega\"utf8)?\n"
      "  dereference(values[0i32]).flush()?\n"
      "  dereference(values[minus(count(values), 1i32)]).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Pointer<File<Write>>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Pointer<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] r0{location(a0)}\n"
      "  [Pointer<File<Write>>] r1{location(a1)}\n"
      "  [Pointer<File<Write>>] r2{location(a2)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] s0{location(b0)}\n"
      "  [Pointer<File<Write>>] s1{location(b1)}\n"
      "  [Pointer<File<Write>>] s2{location(b2)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] t0{location(c0)}\n"
      "  [Pointer<File<Write>>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))\n"
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
  CHECK(result == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathC0).empty());
  CHECK(readFile(pathC1) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
}

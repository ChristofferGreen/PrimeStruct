#include "third_party/doctest.h"

#include "primec/IrSerializer.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
std::string writeTemp(const std::string &name, const std::string &contents) {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests";
  std::filesystem::create_directories(dir);
  auto path = dir / name;
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}

int runCommand(const std::string &command) {
  int code = std::system(command.c_str());
#if defined(__unix__) || defined(__APPLE__)
  if (code == -1) {
    return -1;
  }
  if (WIFEXITED(code)) {
    return WEXITSTATUS(code);
  }
  return -1;
#else
  return code;
#endif
}

std::string readFile(const std::string &path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}
} // namespace

TEST_SUITE_BEGIN("primestruct.compile.run");

TEST_CASE("compiles and runs simple main") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_simple.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_simple_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs string count and indexing in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text("abc"utf8)
  [i32] a(at(text, 0i32))
  [i32] b(at_unsafe(text, 1i32))
  [i32] len(count(text))
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs single-quoted strings in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text('{"k":"v"}'utf8)
  [i32] k(at(text, 2i32))
  [i32] v(at_unsafe(text, 6i32))
  [i32] len(count(text))
  return(plus(plus(k, v), len))
}
)";
  const std::string srcPath = writeTemp("compile_single_quoted_string.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_single_quoted_string_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (107 + 118 + 9));
}

TEST_CASE("compiles and runs pathspace builtins as no-ops") {
  const std::string source = R"(
[return<int> effects(pathspace_notify, pathspace_insert, pathspace_take)]
main() {
  [string] path("/events/test"utf8)
  [string] storePath("/store/value"utf8)
  notify(path, 1i32)
  insert(storePath, 2i32)
  take(storePath)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_pathspace_builtins.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pathspace_builtins_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_pathspace_builtins_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs binding without explicit type") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value(1i32)
  assign(value, plus(value, 2i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_default_type_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_default_type_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_default_type_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs binding inferring i64") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value(1i64)
  assign(value, plus(value, 2i64))
  return(convert<i32>(value))
}
)";
  const std::string srcPath = writeTemp("compile_infer_i64_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs binding inferring u64") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value(1u64)
  assign(value, plus(value, 2u64))
  return(convert<i32>(value))
}
)";
  const std::string srcPath = writeTemp("compile_infer_u64_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_u64_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_infer_u64_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs parameter inferring i64 from default initializer") {
  const std::string source = R"(
[return<i64>]
id([mut] value(3i64)) {
  return(value)
}

[return<int>]
main() {
  return(convert<i32>(id()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_i64_param_default.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_param_default_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_i64_param_default_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs map literal preserving assignment value") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(0i32)
  map<i32, i32>{1i32=value=2i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_map_value_assign.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_value_assign_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_value_assign_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("C++ emitter array access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32))
  return(values[9i32])
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_cpp_array_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_cpp_array_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("C++ emitter string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text("abc"utf8)
  return(at(text, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_string_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_cpp_string_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_cpp_string_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("runs program in vm") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("vm_simple.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("runs vm with string count and indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text("abc"utf8)
  [i32] a(text[0i32])
  [i32] b(at_unsafe(text, 1i32))
  [i32] len(text.count())
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("vm_string_index.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == (97 + 98 + 3));
}

TEST_CASE("runs vm with argv printing") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[0i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_print.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_print_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == srcPath + "\n");
}

TEST_CASE("runs vm with argv printing without newline") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print(args[0i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_print_no_newline.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_print_no_newline_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == srcPath);
}

TEST_CASE("runs vm with forwarded argv") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1i32])
  } else {
    print_line("missing"utf8)
  }
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_forward_argv.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_forward_argv_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("vm_argv_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with argv i64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i64])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_i64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_i64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1u64])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_error_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("runs vm with argv error output without newline") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_no_newline.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_no_newline_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_u64.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_u64_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv unsafe error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv unsafe line error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_line_error_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_line_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("runs vm with method call result") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value(5i32)
  return(plus(value.inc(), 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with literal method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("vm_method_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with chained method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("vm_method_chain.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with numeric array literals") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  print_line(values.count())
  print_line(values[1i64])
  print_line(values[2u64])
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_array_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(1i32, 2i32, 3i32))
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_array_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm map literal odd args") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_odd.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map literal type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm array access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32))
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_array_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_array_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm array access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(plus(100i32, values[1u64]))
}
)";
  const std::string srcPath = writeTemp("vm_array_u64.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 107);
}

TEST_CASE("vm array access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32))
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_array_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_array_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm array unsafe access reads element") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  print_line(at_unsafe(values, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_unsafe_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm array unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  print_line(at_unsafe(values, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_unsafe_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm argv access checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv access rejects negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[-1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv unsafe access skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_bounds.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("vm argv unsafe access skips negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, -1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_negative.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_negative_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv binding checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value(args[9i32])
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_bounds.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv binding unsafe skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value(at_unsafe(args, 9i32))
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv unsafe binding copy skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first(at_unsafe(args, 9i32))
  [string] second(first)
  print_line(second)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_unsafe_copy.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_copy_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_copy_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv call argument checks bounds") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_call_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv call argument unsafe skips bounds") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_call_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("writes serialized ir output") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ir_simple.prime", source);
  const std::string irPath = (std::filesystem::temp_directory_path() / "primec_ir_simple.psir").string();
  const std::string compileCmd = "./primec --emit=ir " + srcPath + " -o " + irPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  primec::IrModule module;
  std::string error;
  REQUIRE(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
  CHECK(module.entryIndex == 0);
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
}

TEST_CASE("no transforms rejects sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_no_transforms_exe").string();

  const std::string compileCmd = "./primec --emit=cpp --no-transforms " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) != 0);
}

TEST_CASE("writes outputs under out dir") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_out_dir.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_out_dir_test";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  const std::string outFile = "primec_out_dir.cpp";
  const std::string expectedPath = (outDir / outFile).string();
  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outFile + " --out-dir " + outDir.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(expectedPath));
}

TEST_CASE("compiles and runs void main") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_void_main.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_main_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_void_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs implicit void main") {
  const std::string source = R"(
main() {
  [i32] value(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_void_implicit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_args_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv error output without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_no_newline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_no_newline_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv error output u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_u64_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe line error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv print in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(args[1i32])
    print_line(args[2i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv print without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print(args[1i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_no_newline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_no_newline_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_args_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs argv print with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_u64_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(at_unsafe(args, 1i32))
    print_line(at_unsafe(args, 2i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(at_unsafe(args, 1u64))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_u64_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs array literal") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_array_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(1i32, 2i32, 3i32))
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_array_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs literal method call in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs chained method calls in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("compile_method_chain.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs array index sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_array_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array index sugar with u64") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_array_index_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_index_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_unsafe_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(at_unsafe(values, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_array_unsafe_u64_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_unsafe_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs repeat loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(0i32)
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_repeat_loop.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_repeat_loop_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_CASE("compiles and runs native executable") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_native.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs if expression in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, 4i32, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_if_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native definition call") {
  const std::string source = R"(
[return<int>]
inc([i32] x) {
  return(plus(x, 1i32))
}

[return<int>]
main() {
  return(inc(6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_def_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_def_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value(5i32)
  return(value.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native method count call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] value) {
    return(plus(value, 2i32))
  }
}

[return<int>]
main() {
  [i32] value(3i32)
  return(value.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_count.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native literal method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native chained method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_chain.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native void call with string param") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    echo(args[1i32])
  } else {
    echo("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_call_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_call_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_args_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs native argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_native_args_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs native argv error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_args_error_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv error output without newline") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error_no_newline.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_no_newline_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs native argv error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error_u64.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_u64_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs native argv unsafe error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs native argv unsafe line error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv print") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1i32])
  } else {
    print_line("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_argv_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv print without newline") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print(args[1i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_print_no_newline.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_print_no_newline_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs native argv print with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
    print_line("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_print_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_print_u64_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv access checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_argv_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native argv access rejects negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[-1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_negative_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_argv_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native argv unsafe access skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_unsafe_u64.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_u64_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv unsafe access skips negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, -1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_unsafe_negative.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_negative_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_negative_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv binding") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    [string] first(args[1i32])
    [string] copy(first)
    print_line(copy)
  } else {
    print_line("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_binding_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv binding unsafe skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value(at_unsafe(args, 9i32))
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv unsafe binding copy skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first(at_unsafe(args, 9i32))
  [string] second(first)
  print_line(second)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding_unsafe_copy.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_copy_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_copy_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_copy_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv call argument unsafe skips bounds") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_call_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native void executable") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_void.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_native_void_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native locals") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  assign(value, plus(value, 3i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_locals.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_locals_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native if/else") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(4i32)
  if(greater_equal(value, 4i32)) {
    return(9i32)
  } else {
    return(2i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_native_if.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_if_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native repeat loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(0i32)
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_repeat_loop.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_repeat_loop_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native pointer helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [Pointer<i32> mut] ptr(location(value))
  assign(dereference(ptr), 6i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native pointer plus") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(5i32)
  return(dereference(plus(location(value), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native print output") {
  const std::string source = R"(
[return<int>]
main() {
  print(42i32)
  print_line("hello"utf8)
  print_error("oops"utf8)
  print_line_error(7i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_print_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_print_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=io_out,io_err";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "42hello\n");
  CHECK(readFile(errPath) == "oops7\n");
}

TEST_CASE("compiles and runs native string binding print") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string] greeting("hi"ascii)
  print_line(greeting)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hi\n");
}

TEST_CASE("compiles and runs native string binding copy") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string] greeting("hey"utf8)
  [string] copy(greeting)
  print_line(copy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_copy.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_copy_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_copy_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hey\n");
}

TEST_CASE("compiles and runs native string count and indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text("abc"utf8)
  [i32] a(text[0i32])
  [i32] b(at_unsafe(text, 1i32))
  [i32] len(text.count())
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs native hello world example") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::string srcPath = (repoRoot / "examples" / "hello_world.prime").string();
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_hello_world_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_hello_world_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "Hello, world!\n");
}

TEST_CASE("compiles and runs native pointer plus offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(first), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_offset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_offset_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer plus on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  [Reference<i32>] ref(location(first))
  return(dereference(plus(location(ref), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer minus offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(second), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_offset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_minus_offset_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native pointer minus u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(second), 16u64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_minus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native pointer minus negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(first), -16i64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_neg_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_minus_neg_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer plus u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(first), 16u64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer plus negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(second), -16i64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_neg_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_neg_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native references") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(4i32)
  [Reference<i32> mut] ref(location(value))
  assign(ref, 7i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_native_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native location on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(8i32)
  [Reference<i32> mut] ref(location(value))
  [Pointer<i32>] ptr(location(ref))
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_native_ref_location.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ref_location_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs native reference arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(4i32)
  [Reference<i32> mut] ref(location(value))
  assign(ref, plus(ref, 3i32))
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_native_ref_arith.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ref_arith_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native clamp") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(9i32, 2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_clamp.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native clamp i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(clamp(9i64, 2i64, 6i64), 6i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_clamp_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_clamp_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native i64 arithmetic") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(plus(4000000000i64, 2i64), 4000000002i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native u64 division") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(divide(10u64, 2u64), 5u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(0xFFFFFFFFFFFFFFFFu64, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_u64_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_u64_compare_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native bool return") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_native_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native implicit void main") {
  const std::string source = R"(
main() {
  [i32] value(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_void_implicit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native boolean ops") {
  const std::string source = R"(
[return<int>]
main() {
  return(or(and(true, false), not(false)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_ops.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_ops_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native numeric boolean ops") {
  const std::string source = R"(
[return<int>]
main() {
  return(or(and(0i32, 5i32), not(0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_numeric_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [i32 mut] witness(0i32)
  assign(value, and(equal(value, 0i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_native_and_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_and_short_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [i32 mut] witness(0i32)
  assign(value, or(equal(value, 1i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_native_or_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_or_short_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native convert") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native convert bool") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<bool>(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_bool_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native convert i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<i64>(9i64), 9i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native convert u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(10u64), 10u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects native float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_native_float_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value(1.5f)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_float_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native non-literal string bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native array literals") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_native_array_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32))
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_negative_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_array_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native array unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array unsafe access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(4i32, 7i32, 9i32))
  return(at_unsafe(values, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_unsafe_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_unsafe_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_count.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(1i32, 2i32, 3i32))
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native map literals") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native typed map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values(map<i32, i32>{1i32=2i32, 3i32=4i32})
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_native_map_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("rejects native map literal odd args") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_odd.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_literal_odd_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native map literal type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}
#endif

TEST_CASE("compiles and runs namespace entry") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(9i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_ns.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ns_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /demo/main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs include expansion") {
  const std::string libPath = writeTemp("compile_lib.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "include<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs operator rewrite with calls") {
  const std::string source = R"(
[return<int>]
helper() {
  return(5i32)
}

[return<int>]
main() {
  return(helper()+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_call_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs operator rewrite with parentheses") {
  const std::string source = R"(
[return<int>]
main() {
  return((1i32+2i32)*3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops_paren.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs operator rewrite with unary minus operand") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(2i32)
  return(3i32+-value)
}
)";
  const std::string srcPath = writeTemp("compile_ops_unary_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects negate on u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(negate(2u64))
}
)";
  const std::string srcPath = writeTemp("compile_negate_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_negate_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [i32 mut] witness(0i32)
  assign(value, and(equal(value, 0i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_and_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_and_short_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [i32 mut] witness(0i32)
  assign(value, or(equal(value, 1i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_or_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_or_short_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs numeric boolean ops") {
  const std::string source = R"(
[return<int>]
main() {
  return(or(0i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_bool_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_numeric_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs convert<bool>") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<bool>(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs convert<i64>") {
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(9i64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_i64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs convert<u64>") {
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(10u64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs convert<bool> from u64") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<bool>(1u64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs convert<bool> from negative i64") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<bool>(-1i64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_i64_neg.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_i64_neg_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs pointer helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(4i32)
  return(dereference(location(value)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_helpers.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs pointer plus helper") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(5i32)
  return(dereference(plus(location(value), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs pointer plus on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(8i32)
  [Reference<i32>] ref(location(value))
  return(dereference(plus(location(ref), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs pointer minus helper") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(second), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs pointer minus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(5i32)
  return(dereference(minus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_minus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_minus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs collection literals in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  map<i32, i32>{1i32=10i32, 2i32=20i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string-keyed map literals in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  map<string, i32>{"a"utf8=1i32, "b"utf8=2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_collections_string_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles with executions using collection arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([i32] items, [i32] pairs) {
  return(1i32)
}

execute_task(items = array<i32>(1i32, 2i32), pairs = map<i32, i32>(1i32, 2i32)) { }
)";
  const std::string srcPath = writeTemp("compile_exec_collections.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles with execution body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat([i32] count) {
  return(1i32)
}

execute_repeat(2i32) {
  main(),
  main()
}
)";
  const std::string srcPath = writeTemp("compile_exec_body.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_body_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs pointer plus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(5i32)
  return(dereference(plus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs i64 literals") {
  const std::string source = R"(
[return<i64>]
main() {
  return(9i64)
}
)";
  const std::string srcPath = writeTemp("compile_i64_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_i64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs u64 literals") {
  const std::string source = R"(
[return<u64>]
main() {
  return(10u64)
}
)";
  const std::string srcPath = writeTemp("compile_u64_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_u64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs assignment operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  value=2i32
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs comparison operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32>1i32)
}
)";
  const std::string srcPath = writeTemp("compile_gt_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_gt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_than operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32<2i32)
}
)";
  const std::string srcPath = writeTemp("compile_lt_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs greater_equal operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32>=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ge_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ge_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_equal operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32<=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_le_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_le_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs and operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32&&1i32)
}
)";
  const std::string srcPath = writeTemp("compile_and_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_and_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs or operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32||1i32)
}
)";
  const std::string srcPath = writeTemp("compile_or_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_or_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(!0i32)
}
)";
  const std::string srcPath = writeTemp("compile_not_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator with parentheses") {
  const std::string source = R"(
[return<int>]
main() {
  return(!(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_not_paren.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs unary minus operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(3i32)
  return(plus(-value, 5i32))
}
)";
  const std::string srcPath = writeTemp("compile_unary_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs equality operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32==2i32)
}
)";
  const std::string srcPath = writeTemp("compile_eq_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_eq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not_equal operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32!=3i32)
}
)";
  const std::string srcPath = writeTemp("compile_neq_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_neq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs implicit i32 suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_suffix_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_suffix_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs implicit utf8 suffix by default") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("hello")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_implicit_utf8.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_implicit_utf8_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_implicit_utf8_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs implicit hex literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(0x2A)
}
)";
  const std::string srcPath = writeTemp("compile_hex.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_hex_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 42);
}

TEST_CASE("compiles and runs float binding") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value(1.5f)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs float comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1.5f, 0.5f))
}
)";
  const std::string srcPath = writeTemp("compile_float_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects mixed int/float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_mixed_int_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_mixed_int_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on array") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values(array<i32>(1i32, 2i32))
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_array.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_array_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  [Pointer<i32>] ptr(location(value))
  return(ptr.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_pointer.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_pointer_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  [Reference<i32>] ref(location(value))
  return(ref.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_reference.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_reference_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on map") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values(map<i32, i32>{1i32=2i32})
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_map.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("implicit suffix disabled by default") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix_off.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_suffix_off_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs if") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(false, then{
    [i32] temp(4i32)
    assign(value, temp)
  }, else{ assign(value, 9i32) })
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_if.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs if expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, 4i32, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_if_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("runs if expression in vm") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, 4i32, 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_if_expr.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);
}

TEST_CASE("compiles and runs if block sugar in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { 4i32 } else { 9i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_sugar.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_sugar_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_expr_sugar_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs if expr block statements") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { [i32] x(4i32) plus(x, 1i32) } else { 0i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_block_stmts.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_block_stmts_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_if_expr_block_stmts_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("compiles and runs with comments") {
  const std::string source = R"(
// comment at top
[return<int>]
main() {
  /* comment in body */
  return(4i32) // trailing comment
}
)";
  const std::string srcPath = writeTemp("compile_comments.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_comments_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_comments_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs block expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] y(2i32)
  return(block { [i32] x(4i32) plus(x, y) })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_block_expr_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 6);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 6);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("compiles and runs greater_than") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(2i32, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_greater_than.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_gt_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_than") {
  const std::string source = R"(
[return<int>]
main() {
  return(less_than(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_less_than.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lt_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs equal") {
  const std::string source = R"(
[return<int>]
main() {
  return(equal(3i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_equal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_equal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not_equal") {
  const std::string source = R"(
[return<int>]
main() {
  return(not_equal(3i32, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_not_equal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_equal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs clamp") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(5i32, 1i32, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_clamp.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs clamp i64") {
  const std::string source = R"(
[return<i64>]
main() {
  return(clamp(9i64, 2i64, 6i64))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_i64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs clamp mixed i32/i64") {
  const std::string source = R"(
[return<i64>]
main() {
  return(clamp(9i32, 2i64, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_i64_mixed.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_i64_mixed_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs clamp u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(clamp(9u64, 2u64, 6u64))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs clamp f32") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(clamp(1.5f, 0.5f, 1.2f)))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_f32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_f32_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs clamp f64") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(clamp(2.5f64, 1.0f64, 2.0f64)))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_f64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_f64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs boolean literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs bool return") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_bool_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs bool comparison") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(true, false))
}
)";
  const std::string srcPath = writeTemp("compile_bool_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs bool and signed int comparison") {
  const std::string source = R"(
[return<int>]
main() {
  return(equal(true, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_bool_int_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_int_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects bool and u64 comparison") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(true, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_bool_u64_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_u64_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs string binding") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message("hello"utf8)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_string_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_binding_exe").string();
  const std::string cppPath =
      std::filesystem::path(exePath).replace_extension(".cpp").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(std::filesystem::exists(cppPath));
  CHECK(readFile(cppPath).find("const const char *") == std::string::npos);
}

TEST_CASE("compiles and runs array literal") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_array.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32, 2i32, 3i32, 4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs map literal pairs") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_pairs.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_pairs_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs named-arg call") {
  const std::string source = R"(
[return<int>]
add([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int>]
main() {
  return(add(b = 2i32, a = 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs convert builtin") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs mixed named args") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b, [i32] c) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3(1i32, c = 3i32, b = 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_mixed.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_mixed_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs reordered named args") {
  const std::string source = R"(
[return<int>]
pack([i32] a, [i32] b, [i32] c) {
  return(plus(plus(multiply(a, 100i32), multiply(b, 10i32)), c))
}

[return<int>]
main() {
  return(pack(c = 3i32, a = 1i32, b = 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_reorder.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 123);
}

TEST_CASE("compiles and runs map literal with named-arg value") {
  const std::string source = R"(
[return<int>]
make_color([i32] hue, [i32] value) {
  return(plus(hue, value))
}

[return<int>]
main() {
  map<i32, i32>{1i32=make_color(hue = 2i32, value = 3i32)}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_named_value.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_named_value_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(false) {
    assign(value, 4i32)
  } else {
    assign(value, 9i32)
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_if_sugar.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_sugar_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs early return in if") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) {
    return(5i32)
  } else {
    return(2i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_return_if.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_return_if_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs void main") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_void.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs local binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(5i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_binding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles with struct definition") {
  const std::string source = R"(
[struct]
data() {
  [i32] value(1i32)
}

[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_struct.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs assign to mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  assign(value, 6i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs assign to reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [Reference<i32> mut] ref(location(value))
  assign(ref, 9i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs location on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(8i32)
  [Reference<i32> mut] ref(location(value))
  [Pointer<i32>] ptr(location(ref))
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_ref_location.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ref_location_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs reference arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(4i32)
  [Reference<i32> mut] ref(location(value))
  assign(ref, plus(ref, 3i32))
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_ref_arith.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ref_arith_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_SUITE_END();

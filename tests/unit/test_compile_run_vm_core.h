#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");

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
  [i32] value{5i32}
  return(plus(value.inc(), 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("vm supports support-matrix binding types") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [i32] i{1i32}
  [i64] j{2i64}
  [u64] k{3u64}
  [bool] b{true}
  [f32] x{1.5f32}
  [f64] y{2.5f64}
  [array<i32>] arr{array<i32>(1i32, 2i32, 3i32)}
  [vector<i32>] vec{vector<i32>(4i32, 5i32)}
  [map<i32, i32>] table{map<i32, i32>(1i32, 7i32)}
  [string] text{"ok"utf8}
  return(plus(
    plus(
      plus(convert<int>(i), convert<int>(j)),
      plus(convert<int>(k), if(b, then() { 1i32 }, else() { 0i32 }))
    ),
    plus(
      plus(convert<int>(x), convert<int>(y)),
      plus(count(arr), plus(count(vec), plus(count(table), count(text))))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("vm_support_matrix_types.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("runs vm with raw string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\\nnext"raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_raw_string_literal.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_raw_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("runs vm with raw single-quoted string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\\nnext'raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_raw_string_literal_single.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_raw_string_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("runs vm with string literal indexing") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 101);
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

TEST_CASE("runs vm with match cases") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{2i32}
  return(match(value, case(1i32) { 10i32 }, case(2i32) { 20i32 }, else() { 30i32 }))
}
)";
  const std::string srcPath = writeTemp("vm_match_cases.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 20);
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

TEST_CASE("runs vm with import alias") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  const std::string srcPath = writeTemp("vm_import_alias.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with multiple imports") {
  const std::string source = R"(
import /util, /std/math/*
namespace util {
  [public return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_import_multiple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with whitespace-separated imports") {
  const std::string source = R"(
import /util /std/math/*
namespace util {
  [public return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_import_whitespace.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with capabilities and default effects") {
  const std::string source = R"(
[return<int> capabilities(io_out)]
main() {
  print_line("capabilities"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_capabilities_default_effects.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_capabilities_out.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "capabilities\n");
}

TEST_CASE("default effects none requires explicit effects in vm") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("no effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_print_no_effects.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_print_no_effects_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=none 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("print_line requires io_out effect") != std::string::npos);
}

TEST_CASE("default effects token does not enable io_err output in vm") {
  const std::string source = R"(
[return<int>]
main() {
  print_line_error("no stderr"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_default_effects_no_io_err.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_default_effects_no_io_err_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("print_line_error requires io_err effect") != std::string::npos);
}

TEST_CASE("runs vm with implicit utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_implicit_utf8.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_implicit_utf8_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("runs vm with implicit utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('implicit')
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_implicit_utf8_single.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_implicit_utf8_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("runs vm with escaped utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\nnext"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_utf8_escaped.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_utf8_escaped_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\nnext\n");
}

TEST_CASE("runs vm with raw utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\nnext'utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_utf8_escaped_single.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_utf8_escaped_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\nnext\n");
}

TEST_CASE("vm supports string return types") {
  const std::string source = R"(
[return<string>]
message() {
  return("hi"utf8)
}

[return<int> effects(io_out)]
main() {
  print_line(message())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_string_return.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_string_return_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hi\n");
}

TEST_CASE("vm supports Result.why hooks") {
  const std::string source = R"(
[struct]
MyError() {
  [i32] code{0i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    return("custom error"utf8)
  }
}

[return<Result<MyError>>]
make_error() {
  return(1i32)
}

[return<int> effects(io_out)]
main() {
  print_line(Result.why(make_error()))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_why_custom.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_result_why_custom_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

#if defined(EACCES)
TEST_CASE("vm maps FileError.why codes") {
  const std::string source =
      "[return<Result<FileError>>]\n"
      "make_error() {\n"
      "  return(" + std::to_string(EACCES) + "i32)\n"
      "}\n"
      "\n"
      "[return<int> effects(io_out)]\n"
      "main() {\n"
      "  print_line(Result.why(make_error()))\n"
      "  return(0i32)\n"
      "}\n";
  const std::string srcPath = writeTemp("vm_file_error_why.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_file_error_why_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("vm rejects recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("vm_recursive_call.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_recursive_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) == "VM lowering error: vm backend does not support recursive calls: /main\n");
}

TEST_CASE("vm rejects string pointers") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
  [Pointer<string>] ptr{location(name)}
}
)";
  const std::string srcPath = writeTemp("vm_string_pointer.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_pointer_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) == "VM lowering error: vm backend does not support string pointers or references\n");
}

TEST_CASE("vm ignores top-level executions") {
  const std::string source = R"(
[return<bool>]
unused() {
  return(equal("a"utf8, "b"utf8))
}

[return<int>]
main() {
  return(0i32)
}

unused()
)";
  const std::string srcPath = writeTemp("vm_exec_ignored.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_SUITE_END();

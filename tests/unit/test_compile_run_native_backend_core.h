#include <cerrno>

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");
TEST_CASE("compiles and runs native file io") {
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_file_io.txt").string();
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
  const std::string escapedPath = escape(outPath);
  const std::string source =
      "[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  [array<i32>] bytes{ array<i32>(65i32, 66i32, 67i32) }\n"
      "  file.write(\"Hello \"utf8, 123i32, \" world\"utf8)?\n"
      "  file.write_line(\"\"utf8)?\n"
      "  file.write_byte(10i32)?\n"
      "  file.write_bytes(bytes)?\n"
      "  file.flush()?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(\"file error\"utf8)\n"
      "}\n";
  const std::string srcPath = writeTemp("native_file_io.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_file_io_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

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

TEST_CASE("compiles and runs native float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_native_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs if expression in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
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
  [i32] value{5i32}
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
  [i32] value{3i32}
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

TEST_CASE("compiles and runs native Result.why hooks") {
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
  const std::string srcPath = writeTemp("compile_native_result_why_custom.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_result_why_custom_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_why_custom_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

#if defined(EACCES)
TEST_CASE("compiles and runs native FileError.why mapping") {
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
  const std::string srcPath = writeTemp("compile_native_file_error_why.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_file_error_why_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_file_error_why_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("compiles and runs native void call with string param") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main() {
  echo("alpha"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_call_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_call_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native string indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs native large frame") {
  std::ostringstream source;
  source << "[return<int>]\n";
  source << "main() {\n";
  for (int i = 0; i < 300; ++i) {
    source << "  [i32] v" << i << "{" << i << "i32}\n";
  }
  source << "  return(v0)\n";
  source << "}\n";
  const std::string srcPath = writeTemp("compile_native_large_frame.prime", source.str());
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_large_frame_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("compile_native_recursive.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_recursive_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Native lowering error: native backend does not support recursive calls: /main\n");
}

TEST_CASE("rejects native string pointers") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
  [Pointer<string>] ptr{location(name)}
}
)";
  const std::string srcPath = writeTemp("compile_native_string_ptr.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_string_ptr_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Native lowering error: native backend does not support string pointers or references\n");
}

TEST_CASE("native ignores top-level executions") {
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
  const std::string srcPath = writeTemp("compile_native_exec_ignored.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_exec_ignored_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_SUITE_END();
#endif

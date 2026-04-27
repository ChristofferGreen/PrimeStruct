#include "test_compile_run_helpers.h"

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.argv");

TEST_CASE("compiles and runs native argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_args.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_args_exe").string();

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
  const std::string exePath = (testScratchPath("") / "primec_native_args_count_helper_exe").string();

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
  const std::string exePath = (testScratchPath("") / "primec_native_args_error_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_native_args_error_err.txt").string();

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
      (testScratchPath("") / "primec_native_args_error_no_newline_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_args_error_no_newline_err.txt").string();

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
      (testScratchPath("") / "primec_native_args_error_u64_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_args_error_u64_err.txt").string();

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
      (testScratchPath("") / "primec_native_args_error_unsafe_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_args_error_unsafe_err.txt").string();

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
      (testScratchPath("") / "primec_native_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv access helpers") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] a{args[1i32]}
  [string] b{at(args, 2i32)}
  [string] c{at_unsafe(args, 3i32)}
  print_line(a)
  print_line(b)
  print_line(c)
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_args_access_helpers.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_args_access_helpers_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_args_access_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " aa bbb cccc > " + outPath) == 4);
  CHECK(readFile(outPath) == "aa\nbbb\ncccc\n");
}

TEST_CASE("compiles and runs native argv line output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[2i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_line_output.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_args_line_output_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_args_line_output.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta gamma > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "beta\n");
}

TEST_CASE("compiles and runs native argv inline string binding") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value{args[2i32]}
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_string_binding.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_args_string_binding_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_args_string_binding.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " alpha bravo > " + outPath) == 0);
  CHECK(readFile(outPath) == "bravo\n");
}

TEST_CASE("native argv rejects returning entry arg string directly") {
  const std::string source = R"(
[return<string>]
main([array<string>] args) {
  return(args[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_return_string.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_return_string_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find(": error: Semantic error: entry argument strings are only supported in print calls or string "
                 "bindings") != std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_CASE("native argv rejects passing entry arg string to helpers") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args[1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_count_string.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_count_string_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_CASE("native argv rejects assigning entry arg string into aggregates") {
  const std::string source = R"(
[struct]
Box() {
  [string] value{""utf8}
}

[return<int>]
main([array<string>] args) {
  [Box] box{Box{[value] args[1i32]}}
  return(box.value.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_box_string.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_box_string_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_CASE("native argv rejects helper forwarding entry arg string") {
  const std::string source = R"(
[return<int>]
/consume([string] value) {
  return(value.count())
}

[return<int>]
main([array<string>] args) {
  return(/consume(args[1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_helper_forward.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_helper_forward_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
}

TEST_CASE("native argv rejects returning unsafe helper string directly") {
  const std::string source = R"(
[return<string>]
main([array<string>] args) {
  return(at_unsafe(args, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_return_unsafe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_return_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
}

TEST_CASE("native argv rejects string comparisons on entry arg strings") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  if(equal(args[1i32], "alpha"utf8), then() { return(1i32) }, else() { return(0i32) })
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_string_compare.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_string_compare_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
}

TEST_CASE("native argv rejects echoing entry args from nested helper calls") {
  const std::string source = R"(
[return<string>]
/pick([array<string>] args) {
  return(args[1i32])
}

[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(/pick(args))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_nested_echo_err.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_nested_echo_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Native lowering error: native backend only supports count() on entry arguments") !=
        std::string::npos);
}

TEST_CASE("native argv rejects string results in call arguments") {
  const std::string source = R"(
[return<int>]
/len([string] value) {
  return(value.count())
}

[return<int>]
main([array<string>] args) {
  return(/len(at_unsafe(args, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_call_unsafe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_argv_call_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find(": error: Semantic error: entry argument strings are only supported in print calls or string "
                 "bindings") != std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_SUITE_END();
#endif

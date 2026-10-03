#include "../test_compile_run_helpers.h"

#if (defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))) || (defined(__linux__) && defined(__x86_64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.uninitialized");

TEST_CASE("native uninitialized string storage") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [uninitialized<string>] storage{uninitialized<string>()}
  init(storage, "hello"utf8)
  print_line(take(storage))
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_uninitialized_string_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_uninitialized_string_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("return requires uninitialized storage to be dropped") !=
        std::string::npos);
}

TEST_CASE("native uninitialized struct field") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] value{uninitialized<i32>()}

  [public]
  Create() {
  }
}

[return<int>]
main() {
  [Box mut] box{Box{}}
  [Reference<Box> mut] ref{location(box)}
  init(ref.value, 7i32)
  [i32] out{take(ref.value)}
  return(out)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_struct.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_struct_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_uninitialized_struct_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_SUITE_END();
#endif

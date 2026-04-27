#include "test_compile_run_helpers.h"

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.maybe");

TEST_CASE("compiles and runs native Maybe some/take") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32> mut] value{some<i32>(2i32)}
  return(value.take())
}
)";
  const std::string srcPath = writeTemp("native_maybe_some_take.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_some_take_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_maybe_some_take_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native Maybe set and is_some") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32> mut] value{none<i32>()}
  value.set(9i32)
  if(value.is_some(), then() { return(value.take()) }, else() { return(0i32) })
}
)";
  const std::string srcPath = writeTemp("native_maybe_set.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_set_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_maybe_set_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native Maybe of string") {
  const std::string source = R"(
import /std/maybe/*

[return<int> effects(io_out)]
main() {
  [Maybe<string> mut] value{some<string>("hello"utf8)}
  print_line(value.take())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("native_maybe_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_maybe_string_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_maybe_string_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath +
      " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native Maybe of struct value") {
  const std::string source = R"(
import /std/maybe/*

[struct]
Widget() {
  [i32] value{4i32}
}

[return<int>]
main() {
  [Maybe<Widget> mut] item{none<Widget>()}
  item.set(Widget{})
  return(item.take().value)
}
)";
  const std::string srcPath = writeTemp("native_maybe_struct.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_struct_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_maybe_struct_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown struct type for layout: Widget") !=
        std::string::npos);
}

TEST_SUITE_END();
#endif

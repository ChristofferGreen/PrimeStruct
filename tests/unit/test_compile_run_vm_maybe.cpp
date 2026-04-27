#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.maybe");

TEST_CASE("runs vm with Maybe some/take") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32> mut] value{some<i32>(2i32)}
  return(value.take())
}
)";
  const std::string srcPath = writeTemp("vm_maybe_some_take.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with Maybe set and is_some") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32> mut] value{none<i32>()}
  value.set(9i32)
  if(value.is_some(), then() { return(value.take()) }, else() { return(0i32) })
}
)";
  const std::string srcPath = writeTemp("vm_maybe_set.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_maybe_set_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_CASE("runs vm with Maybe of string") {
  const std::string source = R"(
import /std/maybe/*

[return<int> effects(io_out)]
main() {
  [Maybe<string> mut] value{some<string>("hello"utf8)}
  print_line(value.take())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_maybe_string.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_maybe_string_out.txt").string();
  const std::string errPath = (testScratchPath("") / "primec_vm_maybe_string_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_CASE("runs vm with Maybe of struct value") {
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
  const std::string srcPath = writeTemp("vm_maybe_struct.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_maybe_struct_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown struct type for layout: Widget") != std::string::npos);
}

TEST_SUITE_END();

#include "../test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.uninitialized");

TEST_CASE("runs vm with uninitialized string storage") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [uninitialized<string>] storage{uninitialized<string>()}
  init(storage, "hello"utf8)
  print_line(take(storage))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_string.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_uninitialized_string_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_vm_uninitialized_string_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string errorText = readFile(errPath);
  CHECK(errorText.find("return requires uninitialized storage to be dropped") !=
        std::string::npos);
}

TEST_CASE("runs vm with uninitialized borrow") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 9i32)
  [Reference<i32>] ref{borrow(storage)}
  [i32] out{dereference(ref)}
  drop(storage)
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_borrow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with dereferenced uninitialized borrow") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] storage_ref{location(storage)}
  init(dereference(storage_ref), 9i32)
  [Reference<i32>] ref{borrow(dereference(storage_ref))}
  [i32] out{dereference(ref)}
  drop(dereference(storage_ref))
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_deref_borrow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with uninitialized struct field") {
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
  const std::string srcPath = writeTemp("vm_uninitialized_struct.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_uninitialized_struct_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_SUITE_END();

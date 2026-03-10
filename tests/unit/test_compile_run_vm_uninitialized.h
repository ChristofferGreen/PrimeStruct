TEST_SUITE_BEGIN("primestruct.compile.run.vm.uninitialized");

TEST_CASE("runs vm with uninitialized local storage") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 3i32)
  drop(storage)
  init(storage, 5i32)
  return(take(storage))
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_local.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_uninitialized_string_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_uninitialized_string_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string errorText = readFile(errPath);
  CHECK(errorText.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("runs vm with uninitialized borrow") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 9i32)
  [Reference<i32>] ref{borrow(storage)}
  return(dereference(ref))
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_borrow.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_uninitialized_borrow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("Reference bindings require location(...)") != std::string::npos);
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
  [Box mut] box{Box()}
  [Reference<Box> mut] ref{location(box)}
  init(ref.value, 7i32)
  [i32] out{take(ref.value)}
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_struct.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_uninitialized_struct_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_SUITE_END();

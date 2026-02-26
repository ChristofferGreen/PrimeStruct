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
  [Box mut] box{Box()}
  [Reference<Box> mut] ref{location(box)}
  init(ref.value, 7i32)
  [i32] out{take(ref.value)}
  return(out)
}
)";
  const std::string srcPath = writeTemp("vm_uninitialized_struct.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_SUITE_END();

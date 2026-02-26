#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.uninitialized");

TEST_CASE("compiles and runs native uninitialized local storage") {
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
  const std::string srcPath = writeTemp("compile_native_uninitialized_local.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_uninitialized_local_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native uninitialized struct field") {
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
  const std::string srcPath = writeTemp("compile_native_uninitialized_struct.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_uninitialized_struct_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_SUITE_END();
#endif

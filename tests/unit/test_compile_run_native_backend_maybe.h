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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_maybe_some_take_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_maybe_set_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
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
  item.set(Widget())
  return(item.take().value)
}
)";
  const std::string srcPath = writeTemp("native_maybe_struct.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_maybe_struct_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_SUITE_END();
#endif

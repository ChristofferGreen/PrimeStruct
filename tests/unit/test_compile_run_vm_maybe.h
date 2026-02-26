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
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
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
  item.set(Widget())
  return(item.take().value)
}
)";
  const std::string srcPath = writeTemp("vm_maybe_struct.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_SUITE_END();

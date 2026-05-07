#include "test_compile_run_helpers.h"

namespace {

void checkRetiredMaybeMutableHelperDiagnostic(const std::string &fileStem,
                                              const std::string &statement,
                                              const std::string &helper,
                                              const std::string &replacement) {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32> mut] value{none<i32>()}
  )" + statement + R"(
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(fileStem + ".prime", source);
  const std::string errPath =
      (testScratchPath("") / (fileStem + ".err")).string();
  const std::string runCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) +
      " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) != 0);
  const std::string error = readFile(errPath);
  CHECK(error.find("sum-backed Maybe<T> has no mutable helper") !=
        std::string::npos);
  CHECK(error.find(helper) != std::string::npos);
  CHECK(error.find(replacement) != std::string::npos);
}

} // namespace

TEST_SUITE_BEGIN("primestruct.compile.run.vm.maybe");

TEST_CASE("runs vm with Maybe some and pick") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] value{[some] 2i32}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  const std::string srcPath = writeTemp("vm_maybe_some_pick.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with Maybe none and helper methods") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] empty{}
  [Maybe<i32>] value{[some] 7i32}
  if(not(empty.is_empty())) {
    return(0i32)
  }
  if(not(value.isSome())) {
    return(0i32)
  }
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  const std::string srcPath = writeTemp("vm_maybe_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with Maybe present variant payload") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] value{[some] 9i32}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  const std::string srcPath = writeTemp("vm_maybe_present_variant_payload.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects retired Maybe mutable helpers with migration diagnostics") {
  checkRetiredMaybeMutableHelperDiagnostic(
      "vm_maybe_retired_set_helper", "value.set(9i32)", "set",
      "use some<T>(value) or Maybe<T>{[some] value} instead");
  checkRetiredMaybeMutableHelperDiagnostic(
      "vm_maybe_retired_clear_helper", "value.clear()", "clear",
      "use Maybe<T>{} or none<T>() instead");
  checkRetiredMaybeMutableHelperDiagnostic(
      "vm_maybe_retired_take_helper", "[i32] out{value.take()}", "take",
      "use pick(value) and rebind the Maybe explicitly instead");
}

TEST_SUITE_END();

#include "test_compile_run_helpers.h"

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.maybe");

TEST_CASE("compiles and runs native Maybe some and pick") {
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
  const std::string srcPath = writeTemp("native_maybe_some_pick.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_some_pick_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native Maybe none and helper methods") {
  const std::string source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] empty{}
  [Maybe<i32>] value{[some] 7i32}
  if(not(empty.isEmpty())) {
    return(0i32)
  }
  if(not(value.is_some())) {
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
  const std::string srcPath = writeTemp("native_maybe_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_helpers_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native Maybe present variant payload") {
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
  const std::string srcPath = writeTemp("native_maybe_present_variant_payload.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_maybe_present_variant_payload_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();
#endif

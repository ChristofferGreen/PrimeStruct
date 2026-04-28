#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("compiles and runs simple main") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_simple.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_simple_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("compiles and runs float arithmetic in VM") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_float_vm.prime", source);

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("compiles and runs primitive brace constructors") {
  const std::string source = R"(
[return<bool>]
truthy() {
  return(bool{ 35i32 })
}

[return<int>]
main() {
  return(convert<int>(truthy()))
}
)";
  const std::string srcPath = writeTemp("compile_brace_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_brace_convert_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(runVmCmd) == 1);
}

TEST_CASE("default entry path is main") {
  const std::string source = R"(
[return<int>]
main() {
  return(4i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_entry.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_default_entry_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primevm " + srcPath;
  CHECK(runCommand(runVmCmd) == 4);
}

TEST_CASE("enum value access lowers across backends") {
  const std::string source = R"(
[enum]
Colors() {
  assign(Blue, 5i32)
  Red
}

[return<int>]
main() {
  return(Colors.Blue.value)
}
)";
  const std::string srcPath = writeTemp("compile_enum_access.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_enum_access_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_enum_access_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("scalar sum construction and pick lower across backends") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[return<int>]
main() {
  [Choice] choice{[right] 41i32}
  [i32] result{pick(choice) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  }}
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_scalar_sum_pick.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_scalar_sum_pick_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_scalar_sum_pick_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 43);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 43);
}

TEST_CASE("aggregate sum payloads bind only the active pick branch") {
  const std::string source = R"(
[struct]
PayloadLeft {
  [i32] value
}

[struct]
PayloadRight {
  [i32] value
}

[sum]
Choice {
  [PayloadLeft] left
  [PayloadRight] right
}

[return<int>]
main() {
  [Choice] choice{[right] PayloadRight{41i32}}
  [i32] result{pick(choice) {
    left(payload) {
      plus(payload.value, 1i32)
    }
    right(payload) {
      plus(payload.value, 2i32)
    }
  }}
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_aggregate_sum_pick.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_aggregate_sum_pick_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_aggregate_sum_pick_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 43);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 43);
}

TEST_CASE("aggregate sum pick results copy active payloads before escape") {
  const std::string source = R"(
[struct]
Payload {
  [i32] value
  [i32] bonus
}

[sum]
Choice {
  [Payload] left
  [Payload] right
}

[return<int>]
score([Payload] payload) {
  return(plus(payload.value, payload.bonus))
}

[return<int>]
main() {
  [Choice] choice{[right] Payload{40i32 3i32}}
  [Payload] picked{pick(choice) {
    left(payload) {
      Payload{1i32 2i32}
    }
    right(payload) {
      payload
    }
  }}
  [i32] fromBinding{score(picked)}
  [i32] fromDirect{score(pick(choice) {
    left(payload) {
      Payload{10i32 20i32}
    }
    right(payload) {
      payload
    }
  })}
  return(plus(fromBinding, fromDirect))
}
)";
  const std::string srcPath = writeTemp("compile_aggregate_sum_pick_escape.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_aggregate_sum_pick_escape_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_aggregate_sum_pick_escape_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 86);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 86);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 86);
}

TEST_CASE("nested sum payloads report deterministic lowerer diagnostic") {
  const std::string source = R"(
[sum]
Inner {
  [i32] value
}

[sum]
Outer {
  [Inner] inner
}

[return<int>]
main() {
  [Outer] outer{[inner] Inner{[value] 7i32}}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_nested_sum_payload_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_nested_sum_payload_reject.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_nested_sum_payload_reject.err.txt").string();

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main > " +
                                 outPath + " 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string diagnostics = readFile(outPath) + readFile(errPath);
  CHECK(diagnostics.find("native backend does not support sum payload type: /Outer/inner (Inner)") !=
        std::string::npos);
}


TEST_SUITE_END();

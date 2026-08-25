#include "../test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("rejects stdlib version flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_stdlib_version_invalid.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_stdlib_version_err.txt").string();

  const std::string compileCmd =
      "./primec --stdlib-version=1.2.0 " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("primec and primevm usage prefer text transforms and import flags") {
  const std::string primecErrPath =
      (testScratchPath("") / "primec_usage_modern_flags_err.txt").string();
  const std::string primevmErrPath =
      (testScratchPath("") / "primevm_usage_modern_flags_err.txt").string();

  CHECK(runCommand("./primec --unknown-option 2> " + quoteShellArg(primecErrPath)) == 2);
  const std::string primecErr = readFile(primecErrPath);
  CHECK(primecErr.find("Usage: primec") != std::string::npos);
  CHECK(primecErr.find("--emit=cpp|cpp-ir|exe|exe-ir|native|ir|vm|glsl|spirv|wasm|glsl-ir|spirv-ir") !=
        std::string::npos);
  CHECK(primecErr.find("--import-path <dir>, -I <dir>") != std::string::npos);
  CHECK(primecErr.find("--wasm-profile wasi|browser") != std::string::npos);
  CHECK(primecErr.find("--text-transforms <list>") != std::string::npos);
  CHECK(primecErr.find("--ir-inline") != std::string::npos);
  CHECK(primecErr.find("--text-filters <list>") == std::string::npos);

  CHECK(runCommand("./primevm --unknown-option 2> " + quoteShellArg(primevmErrPath)) == 2);
  const std::string primevmErr = readFile(primevmErrPath);
  CHECK(primevmErr.find("Usage: primevm") != std::string::npos);
  CHECK(primevmErr.find("--import-path <dir>, -I <dir>") != std::string::npos);
  CHECK(primevmErr.find("--text-transforms <list>") != std::string::npos);
  CHECK(primevmErr.find("--ir-inline") != std::string::npos);
  CHECK(primevmErr.find("--debug-json") != std::string::npos);
  CHECK(primevmErr.find("--debug-json-snapshots [none|stop|all]") != std::string::npos);
  CHECK(primevmErr.find("--debug-trace <path>") != std::string::npos);
  CHECK(primevmErr.find("--debug-dap") != std::string::npos);
  CHECK(primevmErr.find("--debug-replay <trace>") != std::string::npos);
  CHECK(primevmErr.find("--debug-replay-sequence <n>") != std::string::npos);
  CHECK(primevmErr.find("--text-filters <list>") == std::string::npos);
}

TEST_CASE("primec and primevm accept ir inline flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_ir_inline_flag.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_ir_inline_flag_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --ir-inline";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main --ir-inline";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("primevm accepts explicit emit vm compatibility flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("primevm_emit_vm_flag.prime", source);
  const std::string runVmCmd = "./primevm --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);
}

TEST_CASE("primevm debug-json emits stable NDJSON schema") {
  const std::string source = R"(
[return<int>]
double() {
  return(plus(4i32, 4i32))
}

[return<int>]
main() {
  return(plus(double(), 1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_schema.prime", source);
  const std::string outPathA =
      (testScratchPath("") / "primevm_debug_json_schema_a.ndjson").string();
  const std::string outPathB =
      (testScratchPath("") / "primevm_debug_json_schema_b.ndjson").string();

  const std::string cmdA =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-json > " + quoteShellArg(outPathA);
  const std::string cmdB =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-json > " + quoteShellArg(outPathB);
  CHECK(runCommand(cmdA) == 9);
  CHECK(runCommand(cmdB) == 9);

  const std::string outA = readFile(outPathA);
  const std::string outB = readFile(outPathB);
  CHECK(outA == outB);
  CHECK(outA.find("Usage: primevm") == std::string::npos);

  std::vector<std::string> lines;
  std::stringstream stream(outA);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  REQUIRE(lines.size() >= 6);

  CHECK(lines.front().find("\"version\":1") != std::string::npos);
  CHECK(lines.front().find("\"event\":\"session_start\"") != std::string::npos);
  CHECK(lines.front().find("\"snapshot\":{") != std::string::npos);

  bool sawBefore = false;
  bool sawAfter = false;
  bool sawCallPush = false;
  bool sawCallPop = false;
  for (const auto &entry : lines) {
    CHECK(entry.front() == '{');
    CHECK(entry.back() == '}');
    if (entry.find("\"event\":\"before_instruction\"") != std::string::npos) {
      sawBefore = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"opcode\":") != std::string::npos);
      CHECK(entry.find("\"immediate\":") != std::string::npos);
    }
    if (entry.find("\"event\":\"after_instruction\"") != std::string::npos) {
      sawAfter = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"opcode\":") != std::string::npos);
      CHECK(entry.find("\"immediate\":") != std::string::npos);
    }
    if (entry.find("\"event\":\"call_push\"") != std::string::npos) {
      sawCallPush = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"function_index\":") != std::string::npos);
      CHECK(entry.find("\"returns_value_to_caller\":") != std::string::npos);
    }
    if (entry.find("\"event\":\"call_pop\"") != std::string::npos) {
      sawCallPop = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"function_index\":") != std::string::npos);
      CHECK(entry.find("\"returns_value_to_caller\":") != std::string::npos);
    }
  }
  CHECK(sawBefore);
  CHECK(sawAfter);
  const bool sawCallEventsOrInstructionEvents = sawCallPush || sawCallPop || (sawBefore && sawAfter);
  CHECK(sawCallEventsOrInstructionEvents);

  CHECK(lines.back().find("\"event\":\"stop\"") != std::string::npos);
  CHECK(lines.back().find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(lines.back().find("\"snapshot\":{") != std::string::npos);
  CHECK(lines.back().find("\"result\":9") != std::string::npos);
}

TEST_SUITE_END();

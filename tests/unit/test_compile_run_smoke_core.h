TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("compiles and runs simple main") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_simple.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_simple_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_brace_convert_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_default_entry_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_enum_access_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_enum_access_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("count forwards to type method across backends") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 2i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_count_method_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_count_method_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("semicolons act as separators") {
  const std::string source = R"(
;
[return<int>]
add([i32] a; [i32] b) {
  return(plus(a, b));
}
;
[return<int>]
main() {
  [i32] value{
    add(1i32; 2i32);
  };
  return(value);
}
)";
  const std::string srcPath = writeTemp("compile_semicolon_separators.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_semicolon_sep_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_semicolon_sep_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("rejects non-argv entry parameter in exe") {
  const std::string source = R"(
[return<int>]
main([i32] value) {
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_entry_bad_param.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_entry_bad_param_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("rejects unsupported emit kinds") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_invalid.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_emit_invalid_err.txt").string();

  const std::string emitMetalCmd =
      "./primec --emit=metal " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(emitMetalCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);

  const std::string emitLlvmCmd =
      "./primec --emit=llvm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(emitLlvmCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("rejects stdlib version flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_stdlib_version_invalid.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_stdlib_version_err.txt").string();

  const std::string compileCmd =
      "./primec --stdlib-version=1.2.0 " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("primec and primevm usage prefer text transforms and import flags") {
  const std::string primecErrPath =
      (std::filesystem::temp_directory_path() / "primec_usage_modern_flags_err.txt").string();
  const std::string primevmErrPath =
      (std::filesystem::temp_directory_path() / "primevm_usage_modern_flags_err.txt").string();

  CHECK(runCommand("./primec --unknown-option 2> " + quoteShellArg(primecErrPath)) == 2);
  const std::string primecErr = readFile(primecErrPath);
  CHECK(primecErr.find("Usage: primec") != std::string::npos);
  CHECK(primecErr.find("[--import-path <dir>] [-I <dir>]") != std::string::npos);
  CHECK(primecErr.find("[--text-transforms <list>]") != std::string::npos);
  CHECK(primecErr.find("[--ir-inline]") != std::string::npos);
  CHECK(primecErr.find("--text-filters <list>") == std::string::npos);

  CHECK(runCommand("./primevm --unknown-option 2> " + quoteShellArg(primevmErrPath)) == 2);
  const std::string primevmErr = readFile(primevmErrPath);
  CHECK(primevmErr.find("Usage: primevm") != std::string::npos);
  CHECK(primevmErr.find("[--import-path <dir>] [-I <dir>]") != std::string::npos);
  CHECK(primevmErr.find("[--text-transforms <list>]") != std::string::npos);
  CHECK(primevmErr.find("[--ir-inline]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-json]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-json-snapshots [none|stop|all]]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-dap]") != std::string::npos);
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ir_inline_flag_exe").string();

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
      (std::filesystem::temp_directory_path() / "primevm_debug_json_schema_a.ndjson").string();
  const std::string outPathB =
      (std::filesystem::temp_directory_path() / "primevm_debug_json_schema_b.ndjson").string();

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

TEST_CASE("primevm debug-json snapshots include payloads across step boundaries") {
  const std::string source = R"(
[return<int>]
main() {
  [int mut] value{3i32}
  assign(value, plus(value, 4i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_snapshot_payloads.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primevm_debug_json_snapshot_payloads.ndjson").string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-json --debug-json-snapshots=all > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 7);

  const std::string output = readFile(outPath);
  std::vector<std::string> lines;
  std::stringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  REQUIRE(lines.size() >= 4);

  bool sawPayload = false;
  bool sawNonEmptyLocals = false;
  bool sawNonEmptyOperandStack = false;
  bool sawAdvancedInstructionPointer = false;
  uint64_t firstPayloadIp = 0;
  bool firstPayloadIpSet = false;

  for (const auto &entry : lines) {
    if (entry.find("\"snapshot_payload\":{") == std::string::npos) {
      continue;
    }
    sawPayload = true;
    CHECK(entry.find("\"instruction_pointer\":") != std::string::npos);
    CHECK(entry.find("\"call_stack\":[") != std::string::npos);
    CHECK(entry.find("\"frame_locals\":[") != std::string::npos);
    CHECK(entry.find("\"current_frame_locals\":[") != std::string::npos);
    CHECK(entry.find("\"operand_stack\":[") != std::string::npos);

    if (entry.find("\"current_frame_locals\":[]") == std::string::npos) {
      sawNonEmptyLocals = true;
    }
    if (entry.find("\"operand_stack\":[]") == std::string::npos) {
      sawNonEmptyOperandStack = true;
    }

    const size_t ipKey = entry.find("\"snapshot_payload\":{\"instruction_pointer\":");
    if (ipKey != std::string::npos) {
      const size_t valueStart = ipKey + std::string("\"snapshot_payload\":{\"instruction_pointer\":").size();
      const size_t valueEnd = entry.find(",", valueStart);
      REQUIRE(valueEnd != std::string::npos);
      const uint64_t payloadIp = static_cast<uint64_t>(std::stoull(entry.substr(valueStart, valueEnd - valueStart)));
      if (!firstPayloadIpSet) {
        firstPayloadIp = payloadIp;
        firstPayloadIpSet = true;
      } else if (payloadIp != firstPayloadIp) {
        sawAdvancedInstructionPointer = true;
      }
    }
  }

  CHECK(sawPayload);
  CHECK(sawNonEmptyLocals);
  CHECK(sawNonEmptyOperandStack);
  CHECK(sawAdvancedInstructionPointer);
  CHECK(lines.back().find("\"event\":\"stop\"") != std::string::npos);
  CHECK(lines.back().find("\"snapshot_payload\":{") != std::string::npos);
}

TEST_CASE("primevm debug-json snapshots mode requires debug-json") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_snapshot_requires_mode.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_debug_json_snapshot_requires_mode.err").string();
  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-json-snapshots=all 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-json-snapshots requires --debug-json") != std::string::npos);
}

TEST_CASE("primevm rejects invalid debug-json snapshots mode") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_snapshot_invalid_mode.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_debug_json_snapshot_invalid_mode.err").string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --debug-json --debug-json-snapshots=weird 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unsupported --debug-json-snapshots value: weird (expected none|stop|all)") !=
        std::string::npos);
}

TEST_CASE("primec rejects debug-json option") {
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_reject_debug_json_err.txt").string();
  const std::string cmd = "./primec --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unknown option: --debug-json") != std::string::npos);
  CHECK(diagnostics.find("Usage: primec") != std::string::npos);

  const std::string snapshotErrPath =
      (std::filesystem::temp_directory_path() / "primec_reject_debug_json_snapshots_err.txt").string();
  const std::string snapshotCmd = "./primec --debug-json-snapshots=all 2> " + quoteShellArg(snapshotErrPath);
  CHECK(runCommand(snapshotCmd) == 2);
  const std::string snapshotDiagnostics = readFile(snapshotErrPath);
  CHECK(snapshotDiagnostics.find("unknown option: --debug-json-snapshots=all") != std::string::npos);

  const std::string dapErrPath =
      (std::filesystem::temp_directory_path() / "primec_reject_debug_dap_err.txt").string();
  const std::string dapCmd = "./primec --debug-dap 2> " + quoteShellArg(dapErrPath);
  CHECK(runCommand(dapCmd) == 2);
  const std::string dapDiagnostics = readFile(dapErrPath);
  CHECK(dapDiagnostics.find("unknown option: --debug-dap") != std::string::npos);
}

TEST_CASE("primevm debug-dap rejects incompatible debug-json mode") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_conflict.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primevm_debug_dap_conflict.err").string();
  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-dap --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-dap cannot be combined with --debug-json") != std::string::npos);
}

TEST_CASE("primevm debug-dap emits deterministic framed transcripts") {
  const std::string source = R"(
[return<int>]
main() {
  [int mut] value{1i32}
  assign(value, plus(value, 1i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_transcript.prime", source);

  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"setBreakpoints","arguments":{"breakpoints":[{"line":4,"column":3}]}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"configurationDone","arguments":{}})") +
                               makeDapFrame(R"({"seq":5,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":6,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":7,"type":"request","command":"scopes","arguments":{"frameId":1}})") +
                               makeDapFrame(R"({"seq":8,"type":"request","command":"variables","arguments":{"variablesReference":257}})") +
                               makeDapFrame(R"({"seq":9,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_transcript.requests", requests);
  const std::string outPathA =
      (std::filesystem::temp_directory_path() / "primevm_debug_dap_transcript_a.out").string();
  const std::string outPathB =
      (std::filesystem::temp_directory_path() / "primevm_debug_dap_transcript_b.out").string();

  const std::string cmdA = "./primevm " + quoteShellArg(srcPath) +
                           " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPathA);
  const std::string cmdB = "./primevm " + quoteShellArg(srcPath) +
                           " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPathB);
  CHECK(runCommand(cmdA) == 0);
  CHECK(runCommand(cmdB) == 0);

  const std::string outA = readFile(outPathA);
  const std::string outB = readFile(outPathB);
  CHECK(outA == outB);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(outA, framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 12);

  CHECK(frames[0].find("\"type\":\"response\"") != std::string::npos);
  CHECK(frames[0].find("\"command\":\"initialize\"") != std::string::npos);
  CHECK(frames[1].find("\"command\":\"launch\"") != std::string::npos);
  CHECK(frames[2].find("\"type\":\"event\"") != std::string::npos);
  CHECK(frames[2].find("\"event\":\"initialized\"") != std::string::npos);
  CHECK(frames[3].find("\"type\":\"event\"") != std::string::npos);
  CHECK(frames[3].find("\"event\":\"stopped\"") != std::string::npos);
  CHECK(frames[3].find("\"reason\":\"entry\"") != std::string::npos);
  CHECK(frames[4].find("\"command\":\"setBreakpoints\"") != std::string::npos);
  CHECK(frames[4].find("\"verified\":true") != std::string::npos);
  CHECK(frames[6].find("\"command\":\"continue\"") != std::string::npos);
  CHECK(frames[7].find("\"event\":\"stopped\"") != std::string::npos);
  CHECK(frames[7].find("\"reason\":\"breakpoint\"") != std::string::npos);
  CHECK(frames[8].find("\"command\":\"stackTrace\"") != std::string::npos);
  CHECK(frames[8].find("\"stackFrames\":[") != std::string::npos);
  CHECK(frames[9].find("\"command\":\"scopes\"") != std::string::npos);
  CHECK(frames[9].find("\"scopes\":[") != std::string::npos);
  CHECK(frames[10].find("\"command\":\"variables\"") != std::string::npos);
  CHECK(frames[10].find("\"variables\":[") != std::string::npos);
  CHECK(frames[11].find("\"command\":\"disconnect\"") != std::string::npos);
}

TEST_CASE("primevm debug-dap end-to-end process smoke emits exit events") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_smoke.prime", source);
  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_smoke.requests", requests);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_debug_dap_smoke.out").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  const std::string output = readFile(outPath);
  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(output, framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 8);
  CHECK(frames[5].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[5].find("\"exitCode\":9") != std::string::npos);
  CHECK(frames[6].find("\"event\":\"terminated\"") != std::string::npos);
}

TEST_CASE("primevm debug-dap exposes non-top frame locals in variables") {
  const std::string source = R"(
[return<int>]
helper() {
  [int mut] inner{7i32}
  return(inner)
}

[return<int>]
main() {
  [int mut] outer{9i32}
  return(helper())
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_non_top_locals.prime", source);

  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"setInstructionBreakpoints","arguments":{"breakpoints":[{"instructionReference":"f1:ip2"}]}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":5,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":6,"type":"request","command":"scopes","arguments":{"frameId":2}})") +
                               makeDapFrame(R"({"seq":7,"type":"request","command":"variables","arguments":{"variablesReference":513}})") +
                               makeDapFrame(R"({"seq":8,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_non_top_locals.requests", requests);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_debug_dap_non_top_locals.out").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(readFile(outPath), framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 11);
  CHECK(frames[4].find("\"command\":\"setInstructionBreakpoints\"") != std::string::npos);
  CHECK(frames[6].find("\"event\":\"stopped\"") != std::string::npos);
  CHECK(frames[7].find("\"command\":\"stackTrace\"") != std::string::npos);
  CHECK(frames[7].find("\"totalFrames\":2") != std::string::npos);
  CHECK(frames[8].find("\"command\":\"scopes\"") != std::string::npos);
  CHECK(frames[8].find("\"variablesReference\":513") != std::string::npos);
  CHECK(frames[9].find("\"command\":\"variables\"") != std::string::npos);
  CHECK(frames[9].find("\"name\":\"outer\"") != std::string::npos);
  CHECK(frames[9].find("\"value\":\"9\"") != std::string::npos);
}

TEST_CASE("primevm rejects primec output flags") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_reject_output_flags.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_reject_output_flags_err.txt").string();

  const std::string outputPathCmd = "./primevm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(outputPathCmd) == 2);
  const std::string outputPathErr = readFile(errPath);
  CHECK(outputPathErr.find("unknown option: -o") != std::string::npos);
  CHECK(outputPathErr.find("Usage: primevm") != std::string::npos);

  const std::string outDirCmd = "./primevm " + srcPath + " --out-dir /tmp --entry /main 2> " + errPath;
  CHECK(runCommand(outDirCmd) == 2);
  const std::string outDirErr = readFile(errPath);
  CHECK(outDirErr.find("unknown option: --out-dir") != std::string::npos);
  CHECK(outDirErr.find("Usage: primevm") != std::string::npos);
}

TEST_CASE("defaults to native output with stem name") {
  const std::string source = R"(
[return<int>]
main() {
  return(5i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_output.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec " + quoteShellArg(srcPath) + " --out-dir " + quoteShellArg(outDir.string()) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  CHECK(std::filesystem::exists(outputPath));
  CHECK(runCommand(quoteShellArg(outputPath.string())) == 5);
}

TEST_CASE("emits PSIR bytecode with --emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_ir.prime", source);
  const std::string irPath = (std::filesystem::temp_directory_path() / "primec_emit_ir.psir").string();

  const std::string compileCmd =
      "./primec --emit=ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(irPath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(irPath));

  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data;
  if (size > 0) {
    data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
  }
  CHECK(!data.empty());
  REQUIRE(data.size() >= 8);
  auto readU32 = [&](size_t offset) -> uint32_t {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  };
  const uint32_t magic = readU32(0);
  const uint32_t version = readU32(4);
  CHECK(magic == 0x50534952u);
  CHECK(version == 16u);

  primec::IrModule module;
  std::string error;
  CHECK(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
}

TEST_CASE("primevm forwards entry args") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_args.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_args_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("primevm supports argv string bindings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{args[1i32]}
  print_line(first)
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("primevm_args_binding.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_args_binding_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs with line comments after expressions") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{7i32}// comment with a+b and a/b should be ignored
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_line_comment.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_line_comment_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_line_comment_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs string count and indexing in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs single-quoted strings in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{'{"k":"v"}'utf8}
  [i32] k{at(text, 2i32)}
  [i32] v{at_unsafe(text, 6i32)}
  [i32] len{count(text)}
  return(plus(plus(k, v), len))
}
)";
  const std::string srcPath = writeTemp("compile_single_quoted_string.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_single_quoted_string_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (107 + 118 + 9));
}

TEST_CASE("compiles and runs method calls via type namespaces") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_call_i32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_call_i32_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_method_call_i32_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("compiles and runs count forwarding to method") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 4i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_forward.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_count_forward_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_count_forward_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs method call on constructor") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/ping([Foo] self) {
  return(9i32)
}

[return<int>]
main() {
  return(Foo().ping())
}
)";
  const std::string srcPath = writeTemp("compile_constructor_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_constructor_method_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_constructor_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs call with body block") {
  const std::string source = R"(
[return<int>]
sum([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int>]
main() {
  [i32 mut] value{0i32}
  [i32] total{ sum(2i32, 3i32) { assign(value, 7i32) } }
  return(plus(total, value))
}
)";
  const std::string srcPath = writeTemp("compile_call_body_block.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_call_body_block_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_call_body_block_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 12);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 12);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 12);
}

TEST_CASE("compiles and runs templated method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  pick<T>([i32] self, [T] other) {
    return(self)
  }
}

[return<int>]
main() {
  return(3i32.pick<int>(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_template_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_template_method_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_template_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() {
    [i32] value{1i32}
    return(plus(value, 6i32))
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_block_expr_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs boolean ops with conversions") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(convert<bool>(1i32), or(convert<bool>(0i32), not(convert<bool>(0i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_int.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_int_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_int_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs integer width converts") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(-1i32), 18446744073709551615u64))
}
)";
  const std::string srcPath = writeTemp("compile_integer_width_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_integer_width_convert_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_integer_width_convert_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs convert bool from negative integer") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_negative_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_convert_bool_negative_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs boolean ops short-circuit") {
  const std::string source = R"(
[return<int>]
main() {
  [bool mut] witness{false}
  or(true, assign(witness, true))
  and(false, assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_short_circuit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_short_circuit_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_bool_ops_short_circuit_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

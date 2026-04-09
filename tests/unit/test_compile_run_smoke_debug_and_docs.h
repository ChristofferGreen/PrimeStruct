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
      (testScratchPath("") / "primevm_debug_json_snapshot_payloads.ndjson").string();
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
      (testScratchPath("") / "primevm_debug_json_snapshot_requires_mode.err").string();
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
      (testScratchPath("") / "primevm_debug_json_snapshot_invalid_mode.err").string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --debug-json --debug-json-snapshots=weird 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unsupported --debug-json-snapshots value: weird (expected none|stop|all)") !=
        std::string::npos);
}

TEST_CASE("primec rejects debug-json option") {
  const std::string errPath =
      (testScratchPath("") / "primec_reject_debug_json_err.txt").string();
  const std::string cmd = "./primec --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unknown option: --debug-json") != std::string::npos);
  CHECK(diagnostics.find("Usage: primec") != std::string::npos);

  const std::string snapshotErrPath =
      (testScratchPath("") / "primec_reject_debug_json_snapshots_err.txt").string();
  const std::string snapshotCmd = "./primec --debug-json-snapshots=all 2> " + quoteShellArg(snapshotErrPath);
  CHECK(runCommand(snapshotCmd) == 2);
  const std::string snapshotDiagnostics = readFile(snapshotErrPath);
  CHECK(snapshotDiagnostics.find("unknown option: --debug-json-snapshots=all") != std::string::npos);

  const std::string dapErrPath =
      (testScratchPath("") / "primec_reject_debug_dap_err.txt").string();
  const std::string dapCmd = "./primec --debug-dap 2> " + quoteShellArg(dapErrPath);
  CHECK(runCommand(dapCmd) == 2);
  const std::string dapDiagnostics = readFile(dapErrPath);
  CHECK(dapDiagnostics.find("unknown option: --debug-dap") != std::string::npos);

  const std::string traceErrPath =
      (testScratchPath("") / "primec_reject_debug_trace_err.txt").string();
  const std::string traceCmd = "./primec --debug-trace=trace.ndjson 2> " + quoteShellArg(traceErrPath);
  CHECK(runCommand(traceCmd) == 2);
  const std::string traceDiagnostics = readFile(traceErrPath);
  CHECK(traceDiagnostics.find("unknown option: --debug-trace=trace.ndjson") != std::string::npos);

  const std::string replayErrPath =
      (testScratchPath("") / "primec_reject_debug_replay_err.txt").string();
  const std::string replayCmd = "./primec --debug-replay=trace.ndjson 2> " + quoteShellArg(replayErrPath);
  CHECK(runCommand(replayCmd) == 2);
  const std::string replayDiagnostics = readFile(replayErrPath);
  CHECK(replayDiagnostics.find("unknown option: --debug-replay=trace.ndjson") != std::string::npos);

  const std::string replaySequenceErrPath =
      (testScratchPath("") / "primec_reject_debug_replay_sequence_err.txt").string();
  const std::string replaySequenceCmd =
      "./primec --debug-replay-sequence=7 2> " + quoteShellArg(replaySequenceErrPath);
  CHECK(runCommand(replaySequenceCmd) == 2);
  const std::string replaySequenceDiagnostics = readFile(replaySequenceErrPath);
  CHECK(replaySequenceDiagnostics.find("unknown option: --debug-replay-sequence=7") != std::string::npos);
}

TEST_CASE("primevm debug-trace requires path and mode exclusivity") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_trace_errors.prime", source);
  const std::string errPath = (testScratchPath("") / "primevm_debug_trace_errors.err").string();

  const std::string missingPathCmd = "./primevm " + quoteShellArg(srcPath) + " --debug-trace 2> " + quoteShellArg(errPath);
  CHECK(runCommand(missingPathCmd) == 2);
  std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-trace requires an output path") != std::string::npos);

  const std::string conflictJsonCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-trace=trace.ndjson --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictJsonCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-json cannot be combined with --debug-trace") != std::string::npos);

  const std::string conflictDapCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-trace=trace.ndjson --debug-dap 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictDapCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-dap cannot be combined with --debug-trace") != std::string::npos);
}

TEST_CASE("primevm debug-trace writes deterministic complete event logs") {
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
  const std::string srcPath = writeTemp("primevm_debug_trace_deterministic.prime", source);
  const std::string tracePathA =
      (testScratchPath("") / "primevm_debug_trace_deterministic_a.ndjson").string();
  const std::string tracePathB =
      (testScratchPath("") / "primevm_debug_trace_deterministic_b.ndjson").string();

  const std::string cmdA =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePathA);
  const std::string cmdB =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePathB);
  CHECK(runCommand(cmdA) == 9);
  CHECK(runCommand(cmdB) == 9);

  const std::string traceA = readFile(tracePathA);
  const std::string traceB = readFile(tracePathB);
  CHECK(traceA == traceB);
  CHECK(traceA.find("\"event\":\"session_start\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"before_instruction\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"after_instruction\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"call_pop\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"stop\"") != std::string::npos);
  CHECK(traceA.find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(traceA.find("\"snapshot_payload\":{") != std::string::npos);
  CHECK(traceA.find("\"frame_locals\":[") != std::string::npos);

  std::vector<std::string> lines;
  std::stringstream stream(traceA);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  REQUIRE(lines.size() >= 6);

  int64_t lastSequence = -1;
  size_t observedSequenceCount = 0;
  for (const std::string &entry : lines) {
    const size_t key = entry.find("\"sequence\":");
    if (key == std::string::npos) {
      continue;
    }
    const size_t start = key + std::string("\"sequence\":").size();
    size_t end = start;
    while (end < entry.size() && std::isdigit(static_cast<unsigned char>(entry[end])) != 0) {
      ++end;
    }
    REQUIRE(end > start);
    const int64_t sequence = std::stoll(entry.substr(start, end - start));
    if (lastSequence >= 0) {
      CHECK(sequence == lastSequence + 1);
    }
    lastSequence = sequence;
    ++observedSequenceCount;
  }
  CHECK(observedSequenceCount > 0);
}

TEST_CASE("primevm debug-replay requires trace and mode exclusivity") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_replay_errors.prime", source);
  const std::string errPath = (testScratchPath("") / "primevm_debug_replay_errors.err").string();

  const std::string missingPathCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay 2> " + quoteShellArg(errPath);
  CHECK(runCommand(missingPathCmd) == 2);
  std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay requires an input trace path") != std::string::npos);

  const std::string conflictJsonCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay=trace.ndjson --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictJsonCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay cannot be combined with --debug-json") != std::string::npos);

  const std::string conflictDapCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay=trace.ndjson --debug-dap 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictDapCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay cannot be combined with --debug-dap") != std::string::npos);

  const std::string conflictTraceCmd = "./primevm " + quoteShellArg(srcPath) +
                                       " --debug-replay=trace.ndjson --debug-trace=trace_out.ndjson 2> " +
                                       quoteShellArg(errPath);
  CHECK(runCommand(conflictTraceCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay cannot be combined with --debug-trace") != std::string::npos);

  const std::string missingReplayCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay-sequence=3 2> " + quoteShellArg(errPath);
  CHECK(runCommand(missingReplayCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay-sequence requires --debug-replay") != std::string::npos);

  const std::string invalidSequenceCmd = "./primevm " + quoteShellArg(srcPath) +
                                         " --debug-replay=trace.ndjson --debug-replay-sequence=abc 2> " +
                                         quoteShellArg(errPath);
  CHECK(runCommand(invalidSequenceCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("invalid --debug-replay-sequence value: abc") != std::string::npos);
}

TEST_CASE("primevm debug-replay restores checkpoint snapshots at requested sequence") {
  const auto extractUnsignedField = [](const std::string &json, const std::string &key, uint64_t &outValue) -> bool {
    const std::string needle = "\"" + key + "\":";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
      return false;
    }
    const size_t valueStart = keyPos + needle.size();
    if (valueStart >= json.size() || std::isdigit(static_cast<unsigned char>(json[valueStart])) == 0) {
      return false;
    }
    size_t valueEnd = valueStart;
    while (valueEnd < json.size() && std::isdigit(static_cast<unsigned char>(json[valueEnd])) != 0) {
      ++valueEnd;
    }
    try {
      outValue = static_cast<uint64_t>(std::stoull(json.substr(valueStart, valueEnd - valueStart)));
      return true;
    } catch (...) {
      return false;
    }
  };
  const auto extractObjectField = [](const std::string &json, const std::string &key, std::string &outValue) -> bool {
    const std::string needle = "\"" + key + "\":";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
      return false;
    }
    const size_t objectStart = keyPos + needle.size();
    if (objectStart >= json.size() || json[objectStart] != '{') {
      return false;
    }
    size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = objectStart; i < json.size(); ++i) {
      const char c = json[i];
      if (inString) {
        if (!escaped && c == '"') {
          inString = false;
        }
        if (!escaped && c == '\\') {
          escaped = true;
        } else {
          escaped = false;
        }
        continue;
      }
      if (c == '"') {
        inString = true;
        escaped = false;
        continue;
      }
      if (c == '{') {
        ++depth;
      } else if (c == '}') {
        if (depth == 0) {
          return false;
        }
        --depth;
        if (depth == 0) {
          outValue = json.substr(objectStart, i - objectStart + 1);
          return true;
        }
      }
    }
    return false;
  };

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
  const std::string srcPath = writeTemp("primevm_debug_replay_equivalence.prime", source);
  const std::string tracePath =
      (testScratchPath("") / "primevm_debug_replay_equivalence_trace.ndjson").string();
  const std::string replayPath =
      (testScratchPath("") / "primevm_debug_replay_equivalence_replay.ndjson").string();

  const std::string traceCmd =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePath);
  CHECK(runCommand(traceCmd) == 9);

  const std::string trace = readFile(tracePath);
  std::stringstream traceStream(trace);
  std::string traceLine;
  uint64_t checkpointSequence = 0;
  std::string checkpointSnapshot;
  std::string checkpointSnapshotPayload;
  bool foundCheckpoint = false;
  while (std::getline(traceStream, traceLine)) {
    if (traceLine.find("\"event\":\"before_instruction\"") == std::string::npos) {
      continue;
    }
    if (!extractUnsignedField(traceLine, "sequence", checkpointSequence)) {
      continue;
    }
    if (!extractObjectField(traceLine, "snapshot", checkpointSnapshot)) {
      continue;
    }
    if (!extractObjectField(traceLine, "snapshot_payload", checkpointSnapshotPayload)) {
      continue;
    }
    foundCheckpoint = true;
    break;
  }
  REQUIRE(foundCheckpoint);

  const std::string replayCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " +
                                quoteShellArg(tracePath) + " --debug-replay-sequence " +
                                std::to_string(checkpointSequence) + " > " + quoteShellArg(replayPath);
  CHECK(runCommand(replayCmd) == 0);

  const std::string replayOutput = readFile(replayPath);
  std::stringstream replayStream(replayOutput);
  std::string replayLine;
  bool foundReplayLine = false;
  while (std::getline(replayStream, replayLine)) {
    if (!replayLine.empty()) {
      foundReplayLine = true;
      break;
    }
  }
  REQUIRE(foundReplayLine);

  CHECK(replayLine.find("\"event\":\"replay_checkpoint\"") != std::string::npos);
  CHECK(replayLine.find("\"target_sequence\":" + std::to_string(checkpointSequence)) != std::string::npos);
  CHECK(replayLine.find("\"checkpoint_sequence\":" + std::to_string(checkpointSequence)) != std::string::npos);

  std::string replaySnapshot;
  std::string replaySnapshotPayload;
  REQUIRE(extractObjectField(replayLine, "snapshot", replaySnapshot));
  REQUIRE(extractObjectField(replayLine, "snapshot_payload", replaySnapshotPayload));
  CHECK(replaySnapshot == checkpointSnapshot);
  CHECK(replaySnapshotPayload == checkpointSnapshotPayload);
}

TEST_CASE("primevm debug-replay is deterministic and rejects invalid traces") {
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
  const std::string srcPath = writeTemp("primevm_debug_replay_determinism.prime", source);
  const std::string tracePath =
      (testScratchPath("") / "primevm_debug_replay_determinism_trace.ndjson").string();
  const std::string replayPathA =
      (testScratchPath("") / "primevm_debug_replay_determinism_a.ndjson").string();
  const std::string replayPathB =
      (testScratchPath("") / "primevm_debug_replay_determinism_b.ndjson").string();
  const std::string errPath = (testScratchPath("") / "primevm_debug_replay_determinism.err").string();

  const std::string traceCmd =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePath);
  CHECK(runCommand(traceCmd) == 9);

  const std::string replayCmdA =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " + quoteShellArg(tracePath) + " > " +
      quoteShellArg(replayPathA);
  const std::string replayCmdB =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " + quoteShellArg(tracePath) + " > " +
      quoteShellArg(replayPathB);
  CHECK(runCommand(replayCmdA) == 9);
  CHECK(runCommand(replayCmdB) == 9);

  const std::string replayA = readFile(replayPathA);
  const std::string replayB = readFile(replayPathB);
  CHECK(replayA == replayB);
  CHECK(replayA.find("\"event\":\"replay_checkpoint\"") != std::string::npos);
  CHECK(replayA.find("\"checkpoint_event\":\"stop\"") != std::string::npos);
  CHECK(replayA.find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(replayA.find("\"snapshot\":{") != std::string::npos);
  CHECK(replayA.find("\"snapshot_payload\":{") != std::string::npos);

  const std::string invalidTracePath = writeTemp("primevm_debug_replay_invalid_trace.ndjson", "{\"version\":1}\n");
  const std::string invalidReplayCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " +
                                       quoteShellArg(invalidTracePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(invalidReplayCmd) == 3);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("replay trace has no checkpoint-capable events") != std::string::npos);
}

TEST_CASE("primevm debug-replay bypasses source compilation on trace-only path") {
  const std::string invalidSource = R"(
[return<int>]
main( {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_replay_trace_only_invalid_source.prime", invalidSource);
  const std::string traceText =
      "{\"version\":1,\"event\":\"stop\",\"sequence\":12,\"reason\":\"Exit\",\"snapshot\":"
      "{\"state\":\"Stopped\",\"function_index\":0,\"instruction_pointer\":0,\"call_depth\":0,"
      "\"operand_stack_size\":0,\"result\":17},\"snapshot_payload\":"
      "{\"instruction_pointer\":0,\"call_stack\":[],\"frame_locals\":[],\"current_frame_locals\":[],"
      "\"operand_stack\":[]}}\n";
  const std::string tracePath = writeTemp("primevm_debug_replay_trace_only.ndjson", traceText);
  const std::string replayPath =
      (testScratchPath("") / "primevm_debug_replay_trace_only.out").string();

  const std::string replayCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " +
                                quoteShellArg(tracePath) + " > " + quoteShellArg(replayPath);
  CHECK(runCommand(replayCmd) == 17);

  const std::string replayOutput = readFile(replayPath);
  CHECK(replayOutput.find("\"event\":\"replay_checkpoint\"") != std::string::npos);
  CHECK(replayOutput.find("\"checkpoint_event\":\"stop\"") != std::string::npos);
  CHECK(replayOutput.find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(replayOutput.find("\"result\":17") != std::string::npos);
}

TEST_CASE("primevm debug-dap rejects incompatible debug-json mode") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_conflict.prime", source);
  const std::string errPath = (testScratchPath("") / "primevm_debug_dap_conflict.err").string();
  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-dap --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-dap cannot be combined with --debug-json") != std::string::npos);
}

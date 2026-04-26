#pragma once

TEST_CASE("ir serializes execution metadata") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = primec::EffectIoOut | primec::EffectFileWrite;
  fn.metadata.capabilityMask = primec::EffectIoOut;
  fn.metadata.schedulingScope = primec::IrSchedulingScope::Default;
  fn.metadata.instrumentationFlags = 3;
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  REQUIRE(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  const auto &decodedFn = decoded.functions.front();
  CHECK(decodedFn.metadata.effectMask == fn.metadata.effectMask);
  CHECK(decodedFn.metadata.capabilityMask == fn.metadata.capabilityMask);
  CHECK(decodedFn.metadata.schedulingScope == fn.metadata.schedulingScope);
  CHECK(decodedFn.metadata.instrumentationFlags == fn.metadata.instrumentationFlags);
}

TEST_CASE("ir serializes instruction debug ids") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1, 11});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 2, 12});
  fn.instructions.push_back({primec::IrOpcode::AddI32, 0, 13});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 14});
  module.functions.push_back(fn);

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  REQUIRE(decoded.functions[0].instructions.size() == 4);
  CHECK(decoded.functions[0].instructions[0].debugId == 11u);
  CHECK(decoded.functions[0].instructions[1].debugId == 12u);
  CHECK(decoded.functions[0].instructions[2].debugId == 13u);
  CHECK(decoded.functions[0].instructions[3].debugId == 14u);
}

TEST_CASE("ir serializes instruction source map metadata") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1, 11});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 12});
  module.functions.push_back(fn);
  module.instructionSourceMap.push_back({11u, 4u, 2u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({12u, 5u, 3u, primec::IrSourceMapProvenance::SyntheticIr});

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.instructionSourceMap.size() == 2);
  CHECK(decoded.instructionSourceMap[0].debugId == 11u);
  CHECK(decoded.instructionSourceMap[0].line == 4u);
  CHECK(decoded.instructionSourceMap[0].column == 2u);
  CHECK(decoded.instructionSourceMap[0].provenance == primec::IrSourceMapProvenance::CanonicalAst);
  CHECK(decoded.instructionSourceMap[1].debugId == 12u);
  CHECK(decoded.instructionSourceMap[1].line == 5u);
  CHECK(decoded.instructionSourceMap[1].column == 3u);
  CHECK(decoded.instructionSourceMap[1].provenance == primec::IrSourceMapProvenance::SyntheticIr);
}

TEST_CASE("ir serializes local debug slot metadata") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.localDebugSlots.push_back({0u, "value", "i32"});
  fn.localDebugSlots.push_back({1u, "counter", "u64"});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  const auto &decodedFn = decoded.functions.front();
  REQUIRE(decodedFn.localDebugSlots.size() == 2);
  CHECK(decodedFn.localDebugSlots[0].slotIndex == 0u);
  CHECK(decodedFn.localDebugSlots[0].name == "value");
  CHECK(decodedFn.localDebugSlots[0].typeName == "i32");
  CHECK(decodedFn.localDebugSlots[1].slotIndex == 1u);
  CHECK(decodedFn.localDebugSlots[1].name == "counter");
  CHECK(decodedFn.localDebugSlots[1].typeName == "u64");
}

TEST_CASE("ir deserialization rejects malformed local debug metadata") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.localDebugSlots.push_back({0u, "value", "i32"});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  REQUIRE(error.empty());

  auto writeU32 = [&](size_t offset, uint32_t value) {
    REQUIRE(offset + 4 <= data.size());
    data[offset] = static_cast<uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  };

  size_t offset = 0;
  offset += 4 * 5; // magic, version, function count, entry index, string count
  offset += 4;     // struct count
  offset += 4 + fn.name.size(); // function name length + bytes
  offset += 8 + 8 + 4 + 4;      // function metadata
  offset += 4;                  // local debug metadata count
  offset += 4;                  // local slot index
  offset += 4 + std::string("value").size(); // local name length + bytes
  writeU32(offset, 0xFFFFFFFFu); // local type length

  primec::IrModule decoded;
  CHECK_FALSE(primec::deserializeIr(data, decoded, error));
  CHECK(error == "truncated IR local debug metadata type");
}

TEST_CASE("ir deserialization rejects malformed instruction debug id metadata") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 10});
  module.functions.push_back(fn);

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  REQUIRE(error.empty());

  size_t offset = 0;
  offset += 4 * 5;          // magic, version, function count, entry index, string count
  offset += 4;              // struct count
  offset += 4 + fn.name.size(); // function name length + bytes
  offset += 8 + 8 + 4 + 4;  // function metadata
  offset += 4;              // local debug metadata count
  offset += 4;              // instruction count
  offset += 1 + 8;          // first instruction opcode + imm
  REQUIRE(offset < data.size());
  data.resize(offset + 3); // truncate first instruction debug id payload

  primec::IrModule decoded;
  CHECK_FALSE(primec::deserializeIr(data, decoded, error));
  CHECK(error == "truncated IR instruction debug id");
}

TEST_CASE("ir deserialization rejects malformed instruction source map metadata") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 10});
  module.functions.push_back(fn);
  module.instructionSourceMap.push_back({9u, 3u, 1u, primec::IrSourceMapProvenance::CanonicalAst});

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  REQUIRE(error.empty());
  REQUIRE(!data.empty());
  data.pop_back();

  primec::IrModule decoded;
  CHECK_FALSE(primec::deserializeIr(data, decoded, error));
  CHECK(error == "truncated IR instruction source map entry");
}

TEST_CASE("ir deserialization rejects unsupported instruction source map provenance") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 10});
  module.functions.push_back(fn);
  module.instructionSourceMap.push_back({9u, 3u, 1u, primec::IrSourceMapProvenance::CanonicalAst});

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  REQUIRE(error.empty());
  REQUIRE(!data.empty());
  data.back() = 0xFF;

  primec::IrModule decoded;
  CHECK_FALSE(primec::deserializeIr(data, decoded, error));
  CHECK(error == "unsupported IR instruction source map provenance");
}

TEST_CASE("ir deserialization rejects unsupported version") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  REQUIRE(error.empty());
  REQUIRE(data.size() >= 8);

  auto writeU32 = [&](size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  };

  writeU32(4, 0xFFFFFFFFu);

  primec::IrModule decoded;
  CHECK_FALSE(primec::deserializeIr(data, decoded, error));
  CHECK(error == "unsupported IR version");
}

TEST_CASE("ir marks tail execution metadata") {
  const std::string source = R"(
[return<int>]
callee() {
  return(7i32)
}

[return<int>]
main() {
  return(callee())
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK((module.functions[0].metadata.instrumentationFlags & primec::InstrumentationTailExecution) != 0u);
}

TEST_CASE("ir lowerer assigns stable deterministic instruction debug ids") {
  const std::string source = R"(
[return<int>]
helper([int] x) {
  return(plus(x, 2i32))
}

[return<int>]
main() {
  [int] a{1i32}
  [int] b{helper(a)}
  if(greater_than(b, 0i32), then() {
    return(b)
  }, else() {
    return(0i32)
  })
}
)";

  primec::Program firstProgram;
  primec::SemanticProgram firstSemanticProgram;
  primec::Program secondProgram;
  primec::SemanticProgram secondSemanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, firstProgram, firstSemanticProgram, error));
  CHECK(error.empty());
  REQUIRE(parseAndValidate(source, secondProgram, secondSemanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule firstModule;
  primec::IrModule secondModule;
  REQUIRE(lowerer.lower(firstProgram, &firstSemanticProgram, "/main", {}, {}, firstModule, error));
  CHECK(error.empty());
  REQUIRE(lowerer.lower(secondProgram, &secondSemanticProgram, "/main", {}, {}, secondModule, error));
  CHECK(error.empty());

  REQUIRE(firstModule.functions.size() == secondModule.functions.size());
  bool sawAnyInstruction = false;
  uint32_t previousDebugId = 0;
  for (size_t functionIndex = 0; functionIndex < firstModule.functions.size(); ++functionIndex) {
    const auto &firstFn = firstModule.functions[functionIndex];
    const auto &secondFn = secondModule.functions[functionIndex];
    REQUIRE(firstFn.instructions.size() == secondFn.instructions.size());
    for (size_t instructionIndex = 0; instructionIndex < firstFn.instructions.size(); ++instructionIndex) {
      const uint32_t firstDebugId = firstFn.instructions[instructionIndex].debugId;
      const uint32_t secondDebugId = secondFn.instructions[instructionIndex].debugId;
      CHECK(firstDebugId == secondDebugId);
      CHECK(firstDebugId > previousDebugId);
      previousDebugId = firstDebugId;
      sawAnyInstruction = true;
    }
  }
  CHECK(sawAnyInstruction);
}

TEST_CASE("ir lowerer emits deterministic instruction source map provenance") {
  const std::string source = R"(
[return<int>]
helper([int] x) {
  return(plus(x, 2i32))
}

[return<int>]
main() {
  [int] a{1i32}
  [int] b{helper(a)}
  return(b)
}
)";

  primec::Program firstProgram;
  primec::SemanticProgram firstSemanticProgram;
  primec::Program secondProgram;
  primec::SemanticProgram secondSemanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, firstProgram, firstSemanticProgram, error));
  CHECK(error.empty());
  REQUIRE(parseAndValidate(source, secondProgram, secondSemanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule firstModule;
  primec::IrModule secondModule;
  REQUIRE(lowerer.lower(firstProgram, &firstSemanticProgram, "/main", {}, {}, firstModule, error));
  CHECK(error.empty());
  REQUIRE(lowerer.lower(secondProgram, &secondSemanticProgram, "/main", {}, {}, secondModule, error));
  CHECK(error.empty());

  size_t totalInstructionCount = 0;
  std::unordered_map<uint32_t, const primec::IrInstructionSourceMapEntry *> sourceMapByDebugId;
  bool sawCanonicalProvenance = false;
  for (const auto &entry : firstModule.instructionSourceMap) {
    CHECK(entry.debugId > 0u);
    if (entry.provenance == primec::IrSourceMapProvenance::CanonicalAst) {
      sawCanonicalProvenance = true;
    }
    CHECK(sourceMapByDebugId.emplace(entry.debugId, &entry).second);
  }
  CHECK(sawCanonicalProvenance);
  for (const auto &fn : firstModule.functions) {
    totalInstructionCount += fn.instructions.size();
  }
  CHECK(firstModule.instructionSourceMap.size() == totalInstructionCount);

  std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> definitionSpanByPath;
  std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> firstStatementSpanByPath;
  for (const auto &def : firstProgram.definitions) {
    const uint32_t line = def.sourceLine > 0 ? static_cast<uint32_t>(def.sourceLine) : 0u;
    const uint32_t column = def.sourceColumn > 0 ? static_cast<uint32_t>(def.sourceColumn) : 0u;
    definitionSpanByPath.emplace(def.fullPath, std::make_pair(line, column));
    if (!def.statements.empty()) {
      const uint32_t statementLine = def.statements.front().sourceLine > 0 ? static_cast<uint32_t>(def.statements.front().sourceLine) : 0u;
      const uint32_t statementColumn =
          def.statements.front().sourceColumn > 0 ? static_cast<uint32_t>(def.statements.front().sourceColumn) : 0u;
      firstStatementSpanByPath.emplace(def.fullPath, std::make_pair(statementLine, statementColumn));
    }
  }

  std::unordered_set<uint32_t> mainLines;
  bool sawMainInstructionOutsideDefinitionLine = false;
  bool sawInlinedHelperStatementLine = false;
  const auto mainDefSpan = definitionSpanByPath.find("/main");
  const auto helperStatementSpan = firstStatementSpanByPath.find("/helper");
  for (const auto &fn : firstModule.functions) {
    for (const auto &inst : fn.instructions) {
      auto sourceIt = sourceMapByDebugId.find(inst.debugId);
      REQUIRE(sourceIt != sourceMapByDebugId.end());
      if (fn.name == "/main") {
        mainLines.insert(sourceIt->second->line);
        if (mainDefSpan != definitionSpanByPath.end() && sourceIt->second->line != mainDefSpan->second.first) {
          sawMainInstructionOutsideDefinitionLine = true;
        }
        if (helperStatementSpan != firstStatementSpanByPath.end() && sourceIt->second->line == helperStatementSpan->second.first) {
          sawInlinedHelperStatementLine = true;
        }
      }
    }
  }
  CHECK(mainLines.size() > 1);
  CHECK(sawMainInstructionOutsideDefinitionLine);
  CHECK(sawInlinedHelperStatementLine);

  REQUIRE(firstModule.instructionSourceMap.size() == secondModule.instructionSourceMap.size());
  for (size_t i = 0; i < firstModule.instructionSourceMap.size(); ++i) {
    const auto &firstEntry = firstModule.instructionSourceMap[i];
    const auto &secondEntry = secondModule.instructionSourceMap[i];
    CHECK(firstEntry.debugId == secondEntry.debugId);
    CHECK(firstEntry.line == secondEntry.line);
    CHECK(firstEntry.column == secondEntry.column);
    CHECK(firstEntry.provenance == secondEntry.provenance);
  }
}

TEST_CASE("ir lowerer marks implicit return source map provenance as synthetic") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello"utf8)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out"}));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {"io_out"}, {"io_out"}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  REQUIRE(!module.functions[0].instructions.empty());
  CHECK(module.functions[0].instructions.back().op == primec::IrOpcode::ReturnVoid);

  std::unordered_map<uint32_t, const primec::IrInstructionSourceMapEntry *> sourceMapByDebugId;
  bool sawCanonical = false;
  bool sawSynthetic = false;
  for (const auto &entry : module.instructionSourceMap) {
    sourceMapByDebugId.emplace(entry.debugId, &entry);
    if (entry.provenance == primec::IrSourceMapProvenance::CanonicalAst) {
      sawCanonical = true;
    }
    if (entry.provenance == primec::IrSourceMapProvenance::SyntheticIr) {
      sawSynthetic = true;
    }
  }
  CHECK(sawCanonical);
  CHECK(sawSynthetic);

  const uint32_t returnDebugId = module.functions[0].instructions.back().debugId;
  auto sourceIt = sourceMapByDebugId.find(returnDebugId);
  REQUIRE(sourceIt != sourceMapByDebugId.end());
  CHECK(sourceIt->second->provenance == primec::IrSourceMapProvenance::SyntheticIr);
}

TEST_CASE("vm debug adapter preserves lowered source map provenance") {
  const std::string source = R"(
[return<int>]
helper([int] x) {
  return(plus(x, 2i32))
}

[return<int>]
main() {
  [int] base{4i32}
  [int] result{helper(base)}
  return(result)
}
)";

  auto provenanceName = [](primec::IrSourceMapProvenance provenance) {
    switch (provenance) {
      case primec::IrSourceMapProvenance::CanonicalAst:
        return "canonical_ast";
      case primec::IrSourceMapProvenance::SyntheticIr:
        return "synthetic_ir";
      case primec::IrSourceMapProvenance::Unknown:
        break;
    }
    return "unknown";
  };
  auto formatSourcePoint = [&](uint32_t debugId,
                               uint32_t line,
                               uint32_t column,
                               primec::IrSourceMapProvenance provenance) {
    std::ostringstream out;
    out << "debug_id=" << debugId << " source=" << line << ":" << column
        << " provenance=" << provenanceName(provenance);
    return out.str();
  };

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  const primec::SemanticProgramDefinition *mainSemanticDefinition = nullptr;
  for (const auto &definition : semanticProgram.definitions) {
    if (definition.fullPath == "/main") {
      mainSemanticDefinition = &definition;
      break;
    }
  }
  REQUIRE(mainSemanticDefinition != nullptr);
  CHECK(mainSemanticDefinition->semanticNodeId != 0u);
  CHECK(mainSemanticDefinition->provenanceHandle != 0u);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  std::unordered_map<uint32_t, const primec::IrInstructionSourceMapEntry *> sourceMapByDebugId;
  for (const auto &entry : module.instructionSourceMap) {
    CHECK(sourceMapByDebugId.emplace(entry.debugId, &entry).second);
  }

  size_t mainFunctionIndex = module.functions.size();
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (module.functions[functionIndex].name == "/main") {
      mainFunctionIndex = functionIndex;
      break;
    }
  }
  REQUIRE(mainFunctionIndex < module.functions.size());

  std::vector<primec::VmResolvedSourceBreakpoint> expectedBreakpoints;
  primec::VmResolvedSourceBreakpoint expectedBreakpoint;
  bool selectedBreakpoint = false;
  const auto &mainFunction = module.functions[mainFunctionIndex];
  for (const auto &instruction : mainFunction.instructions) {
    auto sourceIt = sourceMapByDebugId.find(instruction.debugId);
    if (sourceIt == sourceMapByDebugId.end()) {
      continue;
    }
    const primec::IrInstructionSourceMapEntry &entry = *sourceIt->second;
    if (entry.provenance != primec::IrSourceMapProvenance::CanonicalAst || entry.line == 0u ||
        entry.column == 0u || entry.line <= static_cast<uint32_t>(mainSemanticDefinition->sourceLine)) {
      continue;
    }
    expectedBreakpoints.clear();
    error.clear();
    REQUIRE(primec::resolveSourceBreakpoints(
        module, entry.line, entry.column, expectedBreakpoints, error));
    CHECK(error.empty());
    if (!expectedBreakpoints.empty() && expectedBreakpoints.front().functionIndex == mainFunctionIndex) {
      expectedBreakpoint = expectedBreakpoints.front();
      selectedBreakpoint = true;
      break;
    }
  }
  REQUIRE(selectedBreakpoint);
  const std::string expectedSource =
      formatSourcePoint(expectedBreakpoint.debugId,
                        expectedBreakpoint.line,
                        expectedBreakpoint.column,
                        expectedBreakpoint.provenance);

  primec::VmDebugAdapter adapter;
  REQUIRE(adapter.launch(module, error));
  CHECK(error.empty());

  std::vector<primec::VmDebugAdapterBreakpointResult> breakpointResults;
  REQUIRE(adapter.setSourceBreakpoints(
      {{expectedBreakpoint.line, expectedBreakpoint.column}}, breakpointResults, error));
  CHECK(error.empty());
  REQUIRE(breakpointResults.size() == 1);
  CHECK(breakpointResults.front().verified);
  CHECK(breakpointResults.front().resolvedCount == expectedBreakpoints.size());

  primec::VmDebugAdapterStopEvent stopEvent;
  REQUIRE(adapter.continueExecution(stopEvent, error));
  CHECK(error.empty());
  CHECK(stopEvent.reason == primec::VmDebugStopReason::Breakpoint);

  std::vector<primec::VmDebugAdapterStackFrame> frames;
  REQUIRE(adapter.stackTrace(1, frames, error));
  CHECK(error.empty());
  REQUIRE(!frames.empty());
  const primec::VmDebugAdapterStackFrame &frame = frames.front();
  const std::string actualSource =
      formatSourcePoint(frame.debugId, frame.line, frame.column, frame.provenance);
  INFO("expected source-map provenance: " << expectedSource);
  INFO("debug adapter frame provenance: " << actualSource);
  CHECK(frame.functionIndex == expectedBreakpoint.functionIndex);
  CHECK(frame.instructionPointer == expectedBreakpoint.instructionPointer);
  CHECK(frame.debugId == expectedBreakpoint.debugId);
  CHECK(frame.line == expectedBreakpoint.line);
  CHECK(frame.column == expectedBreakpoint.column);
  CHECK(frame.provenance == expectedBreakpoint.provenance);
}

TEST_CASE("ir leaves tail metadata unset for builtin return") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  CHECK((module.functions[0].metadata.instrumentationFlags & primec::InstrumentationTailExecution) == 0u);
}

TEST_CASE("ir marks tail execution metadata for void direct call tail statement") {
  const std::string source = R"(
[return<void>]
callee() {
  return()
}

[return<void>]
main() {
  callee()
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK((module.functions[0].metadata.instrumentationFlags & primec::InstrumentationTailExecution) != 0u);
}

TEST_CASE("ir lowers pointer helper calls") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  [Pointer<i32> mut] ptr{location(value)}
  assign(dereference(ptr), 9i32)
  return(value)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawAddressOf = false;
  bool sawStoreIndirect = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      sawAddressOf = true;
    }
    if (inst.op == primec::IrOpcode::StoreIndirect) {
      sawStoreIndirect = true;
    }
  }
  CHECK(sawAddressOf);
  CHECK(sawStoreIndirect);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

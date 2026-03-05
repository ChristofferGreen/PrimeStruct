TEST_CASE("ir lowers if/else to jumps") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  if(less_equal(value, 1i32)) {
    return(7i32)
  } else {
    return(3i32)
  }
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawJump = false;
  bool sawJumpIfZero = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    } else if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    }
  }
  CHECK(sawJump);
  CHECK(sawJumpIfZero);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowers if expression to jumps") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawJump = false;
  bool sawJumpIfZero = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    } else if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    }
  }
  CHECK(sawJump);
  CHECK(sawJumpIfZero);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}


TEST_CASE("ir lowers repeat to jumps") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawJump = false;
  bool sawJumpIfZero = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    } else if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    }
  }
  CHECK(sawJump);
  CHECK(sawJumpIfZero);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 6);
}

TEST_CASE("repeat count <= 0 skips body") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(-2i32) {
    assign(value, 9i32)
  }
  return(value)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers location/dereference assignments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32> mut] ptr{location(value)}
  assign(dereference(ptr), 7i32)
  return(dereference(ptr))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawAddress = false;
  bool sawLoadIndirect = false;
  bool sawStoreIndirect = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      sawAddress = true;
    } else if (inst.op == primec::IrOpcode::LoadIndirect) {
      sawLoadIndirect = true;
    } else if (inst.op == primec::IrOpcode::StoreIndirect) {
      sawStoreIndirect = true;
    }
  }
  CHECK(sawAddress);
  CHECK(sawLoadIndirect);
  CHECK(sawStoreIndirect);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir deserialization rejects unknown opcode") {
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

  const uint32_t nameLen = static_cast<uint32_t>(fn.name.size());
  size_t offset = 0;
  offset += 4 * 5; // magic, version, function count, entry index, string count
  offset += 4;     // struct count
  offset += 4 + nameLen; // function name length + bytes
  offset += 8 + 8 + 4 + 4; // effect mask, capability mask, scheduling, instrumentation
  offset += 4;          // local debug metadata count
  offset += 4;          // instruction count
  REQUIRE(offset < data.size());

  data[offset] = 0xFF;

  primec::IrModule decoded;
  CHECK_FALSE(primec::deserializeIr(data, decoded, error));
  CHECK(error == "unsupported IR opcode");
}

TEST_CASE("ir serialization round-trips call opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  std::vector<uint8_t> data;
  std::string error;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 2);
  REQUIRE(decoded.functions[0].instructions.size() == 5);
  CHECK(decoded.functions[0].instructions[0].op == primec::IrOpcode::Call);
  CHECK(decoded.functions[0].instructions[0].imm == 1);
  CHECK(decoded.functions[0].instructions[2].op == primec::IrOpcode::CallVoid);
  CHECK(decoded.functions[0].instructions[2].imm == 1);
}

TEST_CASE("vm executes call and callvoid opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 2});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction valueFn;
  valueFn.name = "/value";
  valueFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  valueFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction ignoredFn;
  ignoredFn.name = "/ignored";
  ignoredFn.instructions.push_back({primec::IrOpcode::PushI32, 999});
  ignoredFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(valueFn));
  module.functions.push_back(std::move(ignoredFn));

  primec::Vm vm;
  std::string error;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("vm executes recursive call opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  primec::Vm vm;
  std::string error;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 24);
}

TEST_CASE("vm reports missing return in called function") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 1});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::Vm vm;
  std::string error;
  uint64_t result = 0;
  CHECK_FALSE(vm.execute(module, result, error));
  CHECK(error.find("missing return in IR function /helper") != std::string::npos);
}

TEST_CASE("ir inlining pass inlines straight-line call opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  std::string error;
  REQUIRE(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  REQUIRE(primec::inlineIrModuleCalls(module, error));
  CHECK(error.empty());

  bool sawCall = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::Call || inst.op == primec::IrOpcode::CallVoid) {
      sawCall = true;
      break;
    }
  }
  CHECK_FALSE(sawCall);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir inlining pass keeps recursive call opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  std::string error;
  REQUIRE(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  REQUIRE(primec::inlineIrModuleCalls(module, error));
  CHECK(error.empty());

  bool sawRecursiveCall = false;
  for (const auto &inst : module.functions[1].instructions) {
    if (inst.op == primec::IrOpcode::Call && inst.imm == 1) {
      sawRecursiveCall = true;
      break;
    }
  }
  CHECK(sawRecursiveCall);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 24);
}

TEST_CASE("ir inlining pass rejects invalid call targets") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 9});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  CHECK_FALSE(primec::inlineIrModuleCalls(module, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("virtual-register lowering preserves vm behavior across control flow and calls") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40400000u}); // 3.0f
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  helperFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::Vm vm;
  std::string error;
  uint64_t baselineResult = 0;
  REQUIRE(vm.execute(module, baselineResult, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());
  REQUIRE(virtualModule.functions.size() == module.functions.size());
  const auto &virtualMain = virtualModule.functions[0];
  REQUIRE(virtualMain.blocks.size() >= 2);
  CHECK(virtualMain.blocks[0].reachable);

  bool sawConditionalSplit = false;
  bool sawUseDefAssignments = false;
  for (const auto &block : virtualMain.blocks) {
    if (block.successorEdges.size() == 2) {
      sawConditionalSplit = true;
    }
    for (const auto &instruction : block.instructions) {
      if (!instruction.useRegisters.empty() || !instruction.defRegisters.empty()) {
        sawUseDefAssignments = true;
      }
    }
    for (const auto &edge : block.successorEdges) {
      REQUIRE(edge.successorBlockIndex < virtualMain.blocks.size());
      const auto &successor = virtualMain.blocks[edge.successorBlockIndex];
      CHECK(edge.stackMoves.size() == successor.entryRegisters.size());
    }
  }
  CHECK(sawConditionalSplit);
  CHECK(sawUseDefAssignments);

  primec::IrModule loweredBackModule;
  REQUIRE(primec::liftBlockVirtualRegistersToIrModule(virtualModule, loweredBackModule, error));
  CHECK(error.empty());
  REQUIRE(primec::validateIrModule(loweredBackModule, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());

  uint64_t roundTripResult = 0;
  REQUIRE(vm.execute(loweredBackModule, roundTripResult, error));
  CHECK(error.empty());
  CHECK(roundTripResult == baselineResult);
}

TEST_CASE("virtual-register lowering rejects inconsistent stack depth merges") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 4});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::IrVirtualRegisterModule virtualModule;
  std::string error;
  CHECK_FALSE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.find("inconsistent stack depth") != std::string::npos);
}

TEST_CASE("virtual-register liveness builds deterministic loop intervals") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());
  REQUIRE(virtualModule.functions.size() == 1);

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());
  REQUIRE(liveness.functions.size() == 1);

  const auto &functionLiveness = liveness.functions[0];
  REQUIRE(functionLiveness.blocks.size() == 4);
  CHECK(functionLiveness.blocks[0].liveInRegisters.empty());
  CHECK(functionLiveness.blocks[0].liveOutRegisters == std::vector<uint32_t>{3});
  CHECK(functionLiveness.blocks[1].liveInRegisters == std::vector<uint32_t>{0});
  CHECK(functionLiveness.blocks[1].liveOutRegisters == std::vector<uint32_t>{0});
  CHECK(functionLiveness.blocks[2].liveInRegisters == std::vector<uint32_t>{1});
  CHECK(functionLiveness.blocks[2].liveOutRegisters == std::vector<uint32_t>{6});
  CHECK(functionLiveness.blocks[3].liveInRegisters == std::vector<uint32_t>{2});
  CHECK(functionLiveness.blocks[3].liveOutRegisters.empty());

  REQUIRE(functionLiveness.intervals.size() == 7);
  CHECK(functionLiveness.intervals[0].virtualRegister == 3);
  CHECK(functionLiveness.intervals[0].ranges[0].startPosition == 1u);
  CHECK(functionLiveness.intervals[0].ranges[0].endPosition == 1u);
  CHECK(functionLiveness.intervals[1].virtualRegister == 0);
  CHECK(functionLiveness.intervals[1].ranges[0].startPosition == 2u);
  CHECK(functionLiveness.intervals[1].ranges[0].endPosition == 5u);
  CHECK(functionLiveness.intervals[2].virtualRegister == 4);
  CHECK(functionLiveness.intervals[2].ranges[0].startPosition == 3u);
  CHECK(functionLiveness.intervals[2].ranges[0].endPosition == 4u);
  CHECK(functionLiveness.intervals[3].virtualRegister == 1);
  CHECK(functionLiveness.intervals[3].ranges[0].startPosition == 6u);
  CHECK(functionLiveness.intervals[3].ranges[0].endPosition == 8u);
  CHECK(functionLiveness.intervals[4].virtualRegister == 5);
  CHECK(functionLiveness.intervals[4].ranges[0].startPosition == 7u);
  CHECK(functionLiveness.intervals[4].ranges[0].endPosition == 8u);
  CHECK(functionLiveness.intervals[5].virtualRegister == 6);
  CHECK(functionLiveness.intervals[5].ranges[0].startPosition == 9u);
  CHECK(functionLiveness.intervals[5].ranges[0].endPosition == 11u);
  CHECK(functionLiveness.intervals[6].virtualRegister == 2);
  CHECK(functionLiveness.intervals[6].ranges[0].startPosition == 12u);
  CHECK(functionLiveness.intervals[6].ranges[0].endPosition == 12u);
}

TEST_CASE("virtual-register liveness tie-breaks equal intervals by register id") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());
  REQUIRE(liveness.functions.size() == 1);
  const auto &intervals = liveness.functions[0].intervals;

  REQUIRE(intervals.size() == 5);
  CHECK(intervals[2].virtualRegister == 0);
  CHECK(intervals[3].virtualRegister == 1);
  CHECK(intervals[2].ranges[0].startPosition == intervals[3].ranges[0].startPosition);
  CHECK(intervals[2].ranges[0].endPosition == intervals[3].ranges[0].endPosition);
}

TEST_CASE("linear-scan allocator has deterministic spill placement on loop cfg") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());
  REQUIRE(allocation.functions.size() == 1);

  const auto &functionAllocation = allocation.functions[0];
  CHECK(functionAllocation.spillSlotCount == 2u);
  REQUIRE(functionAllocation.assignments.size() == 7);

  CHECK(functionAllocation.assignments[0].virtualRegister == 0u);
  CHECK(functionAllocation.assignments[0].spilled);
  CHECK(functionAllocation.assignments[0].spillSlot == 0u);
  CHECK(functionAllocation.assignments[1].virtualRegister == 1u);
  CHECK_FALSE(functionAllocation.assignments[1].spilled);
  CHECK(functionAllocation.assignments[1].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[2].virtualRegister == 2u);
  CHECK_FALSE(functionAllocation.assignments[2].spilled);
  CHECK(functionAllocation.assignments[2].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[3].virtualRegister == 3u);
  CHECK_FALSE(functionAllocation.assignments[3].spilled);
  CHECK(functionAllocation.assignments[3].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[4].virtualRegister == 4u);
  CHECK_FALSE(functionAllocation.assignments[4].spilled);
  CHECK(functionAllocation.assignments[4].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[5].virtualRegister == 5u);
  CHECK(functionAllocation.assignments[5].spilled);
  CHECK(functionAllocation.assignments[5].spillSlot == 1u);
  CHECK(functionAllocation.assignments[6].virtualRegister == 6u);
  CHECK_FALSE(functionAllocation.assignments[6].spilled);
  CHECK(functionAllocation.assignments[6].physicalRegister == 0u);
}

TEST_CASE("linear-scan allocator tie-break keeps lower register id resident") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {1, {{0, 4}}},
      {0, {{0, 4}}},
      {2, {{5, 6}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());
  REQUIRE(allocation.functions.size() == 1);
  const auto &assignments = allocation.functions[0].assignments;
  REQUIRE(assignments.size() == 3);

  CHECK(assignments[0].virtualRegister == 0u);
  CHECK_FALSE(assignments[0].spilled);
  CHECK(assignments[0].physicalRegister == 0u);
  CHECK(assignments[1].virtualRegister == 1u);
  CHECK(assignments[1].spilled);
  CHECK(assignments[1].spillSlot == 0u);
  CHECK(assignments[2].virtualRegister == 2u);
  CHECK_FALSE(assignments[2].spilled);
  CHECK(assignments[2].physicalRegister == 0u);
}

TEST_CASE("linear-scan allocator rejects unknown spill policy") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {0, {{0, 1}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = static_cast<primec::IrLinearScanSpillPolicy>(99);

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  CHECK_FALSE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.find("spill policy") != std::string::npos);
}

TEST_CASE("spill insertion verifies branch and call spill correctness") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 9});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  helperFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 2);

  bool sawReload = false;
  bool sawSpill = false;
  bool sawEdgeOp = false;
  bool sawCallSpill = false;
  for (const auto &functionPlan : plan.functions) {
    for (const auto &blockPlan : functionPlan.blocks) {
      for (const auto &instructionPlan : blockPlan.instructions) {
        if (!instructionPlan.beforeInstructionOps.empty()) {
          sawReload = true;
        }
        if (!instructionPlan.afterInstructionOps.empty()) {
          sawSpill = true;
        }
        if (instructionPlan.instruction.instruction.op == primec::IrOpcode::Call &&
            !instructionPlan.afterInstructionOps.empty()) {
          sawCallSpill = true;
        }
      }
      for (const auto &edgePlan : blockPlan.successorEdges) {
        if (!edgePlan.edgeOps.empty()) {
          sawEdgeOp = true;
        }
      }
    }
  }
  CHECK(sawReload);
  CHECK(sawSpill);
  CHECK(sawEdgeOp);
  CHECK(sawCallSpill);
}

TEST_CASE("spill insertion uses deterministic op ordering for spilled add uses") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(plan.functions[0].blocks.size() == 1);
  REQUIRE(plan.functions[0].blocks[0].instructions.size() == 4);

  const auto &addInstructionPlan = plan.functions[0].blocks[0].instructions[2];
  REQUIRE(addInstructionPlan.instruction.instruction.op == primec::IrOpcode::AddI32);
  REQUIRE(addInstructionPlan.beforeInstructionOps.size() == 2);
  CHECK(addInstructionPlan.beforeInstructionOps[0].kind == primec::IrVirtualRegisterSpillOpKind::ReloadFromSpill);
  CHECK(addInstructionPlan.beforeInstructionOps[1].kind == primec::IrVirtualRegisterSpillOpKind::ReloadFromSpill);
  CHECK(addInstructionPlan.beforeInstructionOps[0].virtualRegister <
        addInstructionPlan.beforeInstructionOps[1].virtualRegister);
}

TEST_CASE("spill insertion verifier rejects missing reload ops") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(plan.functions[0].blocks.size() == 1);
  REQUIRE(plan.functions[0].blocks[0].instructions.size() == 4);

  plan.functions[0].blocks[0].instructions[2].beforeInstructionOps.clear();
  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.find("missing reload op") != std::string::npos);
}

TEST_CASE("scheduler is dependency-safe and latency-aware") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 100});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::DivI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 2;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterScheduledModule scheduledOnce;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduledOnce, error));
  CHECK(error.empty());
  REQUIRE(scheduledOnce.functions.size() == 1);
  REQUIRE(scheduledOnce.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduledOnce.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 8);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 3, 4, 5, 2, 6, 7});

  std::vector<size_t> positionByInstruction(8, 0);
  for (size_t position = 0; position < scheduledBlock.instructions.size(); ++position) {
    positionByInstruction[scheduledBlock.instructions[position].originalInstructionIndex] = position;
  }
  for (const auto &instruction : scheduledBlock.instructions) {
    const size_t instructionPos = positionByInstruction[instruction.originalInstructionIndex];
    for (size_t dependency : instruction.dependencyInstructionIndices) {
      CHECK(positionByInstruction[dependency] < instructionPos);
    }
  }

  primec::IrVirtualRegisterScheduledModule scheduledAgain;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduledAgain, error));
  CHECK(error.empty());
  REQUIRE(scheduledAgain.functions.size() == 1);
  REQUIRE(scheduledAgain.functions[0].blocks.size() == 1);
  REQUIRE(scheduledAgain.functions[0].blocks[0].instructions.size() == 8);
  for (size_t index = 0; index < 8; ++index) {
    CHECK(scheduledAgain.functions[0].blocks[0].instructions[index].originalInstructionIndex ==
          scheduledBlock.instructions[index].originalInstructionIndex);
  }
}

TEST_CASE("scheduler prioritizes spilled-register latency penalty") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 11};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 22};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 33};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 44};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::AddI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = (reg == 2);
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 2, 3, 5, 4, 6});

  uint32_t addNormalLatency = 0;
  uint32_t addSpilledLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      addNormalLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      addSpilledLatency = instruction.latencyScore;
    }
  }
  CHECK(addNormalLatency == 2u);
  CHECK(addSpilledLatency == 4u);
}

TEST_CASE("scheduler ties pick lower original instruction index") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 11};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 22};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 33};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 44};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::SubI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = false;
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 2, 3, 4, 5, 6});

  uint32_t addLatency = 0;
  uint32_t subLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      addLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      subLatency = instruction.latencyScore;
    }
  }
  CHECK(addLatency == 2u);
  CHECK(subLatency == 2u);
}

TEST_CASE("scheduler enforces barrier ordering boundaries") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 8;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 12};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 3};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::DivI32, 0};
  inst2.useRegisters = {0, 1};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 9};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  // Barrier: should remain after all prior instructions.
  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::PrintI32, 0};
  inst4.useRegisters = {2};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::PushI32, 5};
  inst5.defRegisters = {4};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::AddI32, 0};
  inst6.useRegisters = {3, 4};
  inst6.defRegisters = {5};
  block.instructions.push_back(inst6);

  // Barrier: should remain after all previous instructions.
  primec::IrVirtualRegisterInstruction inst7;
  inst7.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst7.useRegisters = {5};
  block.instructions.push_back(inst7);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = false;
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 8);

  std::vector<size_t> positionByInstruction(8, 0);
  for (size_t position = 0; position < scheduledBlock.instructions.size(); ++position) {
    positionByInstruction[scheduledBlock.instructions[position].originalInstructionIndex] = position;
  }

  CHECK(positionByInstruction[0] < positionByInstruction[4]);
  CHECK(positionByInstruction[1] < positionByInstruction[4]);
  CHECK(positionByInstruction[2] < positionByInstruction[4]);
  CHECK(positionByInstruction[3] < positionByInstruction[4]);
  CHECK(positionByInstruction[4] < positionByInstruction[5]);
  CHECK(positionByInstruction[4] < positionByInstruction[6]);
  CHECK(positionByInstruction[4] < positionByInstruction[7]);
  CHECK(positionByInstruction[5] < positionByInstruction[7]);
  CHECK(positionByInstruction[6] < positionByInstruction[7]);
}

TEST_CASE("scheduler prioritizes spilled-def latency penalty") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 10};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 20};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 30};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 40};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::AddI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = (reg == 5);
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 2, 3, 5, 4, 6});

  uint32_t normalDefLatency = 0;
  uint32_t spilledDefLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      normalDefLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      spilledDefLatency = instruction.latencyScore;
    }
  }
  CHECK(normalDefLatency == 2u);
  CHECK(spilledDefLatency == 4u);
}

TEST_CASE("scheduler rejects allocation name mismatch") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/other";
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  CHECK_FALSE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.find("matching function names") != std::string::npos);
}

TEST_CASE("virtual-register verifier accepts valid scheduled allocation output") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  helperFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 2;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterScheduledModule scheduled;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());

  REQUIRE(primec::verifyIrVirtualRegisterScheduleAndAllocation(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());
}

TEST_CASE("virtual-register verifier rejects use-before-def schedules") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 2;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterScheduledModule scheduled;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  REQUIRE(scheduled.functions[0].blocks[0].instructions.size() == 4);

  auto &instructions = scheduled.functions[0].blocks[0].instructions;
  std::swap(instructions[0], instructions[2]);
  CHECK_FALSE(primec::verifyIrVirtualRegisterScheduleAndAllocation(virtualModule, allocation, scheduled, error));
  CHECK(error.find("uses register before definition") != std::string::npos);
}

TEST_CASE("virtual-register verifier rejects branch-edge disagreement") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 2;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterScheduledModule scheduled;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() >= 1);
  REQUIRE(!scheduled.functions[0].blocks[0].successorEdges.empty());

  scheduled.functions[0].blocks[0].successorEdges[0].successorBlockIndex += 1;
  CHECK_FALSE(primec::verifyIrVirtualRegisterScheduleAndAllocation(virtualModule, allocation, scheduled, error));
  CHECK(error.find("branch-edge value agreement") != std::string::npos);
}

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
namespace {
std::string quoteIrPipelineShellArg(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

int runIrPipelineNativeBinary(const std::string &path) {
  int code = std::system(quoteIrPipelineShellArg(path).c_str());
  if (code == -1) {
    return -1;
  }
  if (WIFEXITED(code)) {
    return WEXITSTATUS(code);
  }
  if (WIFSIGNALED(code)) {
    return 128 + WTERMSIG(code);
  }
  return -1;
}
} // namespace

TEST_CASE("native backend executes call and callvoid opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 2});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction valueFn;
  valueFn.name = "/value";
  valueFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  valueFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction ignoredFn;
  ignoredFn.name = "/ignored";
  ignoredFn.instructions.push_back({primec::IrOpcode::PushI32, 999});
  ignoredFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(valueFn));
  module.functions.push_back(std::move(ignoredFn));

  primec::NativeEmitter emitter;
  std::string error;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_call_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error));
  CHECK(error.empty());
  CHECK(runIrPipelineNativeBinary(exePath) == 7);
}

TEST_CASE("native backend executes recursive call opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  primec::NativeEmitter emitter;
  std::string error;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_recursive_call_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error));
  CHECK(error.empty());
  CHECK(runIrPipelineNativeBinary(exePath) == 24);
}

TEST_CASE("native backend rejects invalid callable IR target") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 9});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::NativeEmitter emitter;
  std::string error;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_invalid_call_exec").string();
  CHECK_FALSE(emitter.emitExecutable(module, exePath, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("virtual-register lowering preserves native baseline parity") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 9});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40400000u}); // 3.0f
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  helperFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::Vm vm;
  std::string error;
  uint64_t vmResult = 0;
  REQUIRE(vm.execute(module, vmResult, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrModule loweredBackModule;
  REQUIRE(primec::liftBlockVirtualRegistersToIrModule(virtualModule, loweredBackModule, error));
  CHECK(error.empty());

  primec::NativeEmitter emitter;
  const std::string baselinePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_virtual_baseline_exec").string();
  const std::string loweredPath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_virtual_lowered_exec").string();
  REQUIRE(emitter.emitExecutable(module, baselinePath, error));
  CHECK(error.empty());
  REQUIRE(emitter.emitExecutable(loweredBackModule, loweredPath, error));
  CHECK(error.empty());

  const int baselineExit = runIrPipelineNativeBinary(baselinePath);
  const int loweredExit = runIrPipelineNativeBinary(loweredPath);
  CHECK(baselineExit == static_cast<int>(vmResult));
  CHECK(loweredExit == static_cast<int>(vmResult));
  CHECK(loweredExit == baselineExit);
}

TEST_CASE("native backend reports instrumentation counters per function") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  std::string error;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_instrumentation_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  REQUIRE(instrumentation.perFunction.size() == 2);

  CHECK(instrumentation.perFunction[0].functionName == "/main");
  CHECK(instrumentation.perFunction[0].instructionTotal == 4u);
  CHECK(instrumentation.perFunction[0].valueStackPushCount == 3u);
  CHECK(instrumentation.perFunction[0].valueStackPopCount == 3u);
  CHECK(instrumentation.perFunction[0].spillCount == 1u);
  CHECK(instrumentation.perFunction[0].reloadCount == 1u);

  CHECK(instrumentation.perFunction[1].functionName == "/helper");
  CHECK(instrumentation.perFunction[1].instructionTotal == 2u);
  CHECK(instrumentation.perFunction[1].valueStackPushCount == 1u);
  CHECK(instrumentation.perFunction[1].valueStackPopCount == 1u);
  CHECK(instrumentation.perFunction[1].spillCount == 0u);
  CHECK(instrumentation.perFunction[1].reloadCount == 0u);

  CHECK(instrumentation.totalInstructionCount == 6u);
  CHECK(instrumentation.totalValueStackPushCount == 4u);
  CHECK(instrumentation.totalValueStackPopCount == 4u);
  CHECK(instrumentation.totalSpillCount == 1u);
  CHECK(instrumentation.totalReloadCount == 1u);
}

TEST_CASE("native backend instrumentation counts local load and store ops") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  std::string error;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_instrumentation_locals_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  REQUIRE(instrumentation.perFunction.size() == 1);

  CHECK(instrumentation.perFunction[0].instructionTotal == 4u);
  CHECK(instrumentation.perFunction[0].valueStackPushCount == 2u);
  CHECK(instrumentation.perFunction[0].valueStackPopCount == 2u);
  CHECK(instrumentation.perFunction[0].spillCount == 0u);
  CHECK(instrumentation.perFunction[0].reloadCount == 0u);
  CHECK(instrumentation.totalInstructionCount == 4u);
  CHECK(instrumentation.totalValueStackPushCount == 2u);
  CHECK(instrumentation.totalValueStackPopCount == 2u);
  CHECK(instrumentation.totalSpillCount == 0u);
  CHECK(instrumentation.totalReloadCount == 0u);
}

TEST_CASE("native backend integer stack cache preserves parity and reduces spills") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  std::string error;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_stack_cache_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  CHECK(runIrPipelineNativeBinary(exePath) == 13);
  REQUIRE(instrumentation.perFunction.size() == 1);
  const auto &mainStats = instrumentation.perFunction[0];
  CHECK(mainStats.valueStackPushCount == 7u);
  CHECK(mainStats.valueStackPopCount == 7u);
  CHECK(mainStats.spillCount == 3u);
  CHECK(mainStats.reloadCount == 3u);
  CHECK(mainStats.spillCount < mainStats.valueStackPushCount);
  CHECK(mainStats.reloadCount < mainStats.valueStackPopCount);
}

TEST_CASE("native backend float stack cache preserves parity and reduces spills") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40200000u}); // 2.5f
  mainFn.instructions.push_back({primec::IrOpcode::AddF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3F800000u}); // 1.0f
  mainFn.instructions.push_back({primec::IrOpcode::SubF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3FF8000000000000ull}); // 1.5
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x4014000000000000ull}); // 5.0
  mainFn.instructions.push_back({primec::IrOpcode::MulF64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::Vm vm;
  uint64_t vmResult = 0;
  std::string error;
  REQUIRE(vm.execute(module, vmResult, error));
  CHECK(error.empty());

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_float_stack_cache_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  const int nativeExit = runIrPipelineNativeBinary(exePath);
  CHECK(nativeExit == static_cast<int>(vmResult));

  REQUIRE(instrumentation.perFunction.size() == 1);
  const auto &mainStats = instrumentation.perFunction[0];
  CHECK(mainStats.valueStackPushCount == 11u);
  CHECK(mainStats.valueStackPopCount == 11u);
  CHECK(mainStats.spillCount == 4u);
  CHECK(mainStats.reloadCount == 4u);
  CHECK(mainStats.spillCount < mainStats.valueStackPushCount);
  CHECK(mainStats.reloadCount < mainStats.valueStackPopCount);
}

TEST_CASE("native backend cache toggle preserves dual-mode parity") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3F800000u}); // 1.0f
  helperFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  helperFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x4008000000000000ull}); // 3.0
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::Vm vm;
  uint64_t vmResult = 0;
  std::string error;
  REQUIRE(vm.execute(module, vmResult, error));
  CHECK(error.empty());

  primec::NativeEmitter emitter;
  primec::NativeEmitterOptions cacheOnOptions;
  cacheOnOptions.enableRegisterCache = true;
  primec::NativeEmitterOptions cacheOffOptions;
  cacheOffOptions.enableRegisterCache = false;
  primec::NativeEmitterInstrumentation cacheOnInstrumentation;
  primec::NativeEmitterInstrumentation cacheOffInstrumentation;
  const std::string exeOnPath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_cache_toggle_on_exec").string();
  const std::string exeOffPath =
      (std::filesystem::temp_directory_path() / "primec_native_ir_cache_toggle_off_exec").string();
  REQUIRE(emitter.emitExecutable(module, exeOnPath, error, &cacheOnInstrumentation, cacheOnOptions));
  CHECK(error.empty());
  REQUIRE(emitter.emitExecutable(module, exeOffPath, error, &cacheOffInstrumentation, cacheOffOptions));
  CHECK(error.empty());

  const int nativeOnExit = runIrPipelineNativeBinary(exeOnPath);
  const int nativeOffExit = runIrPipelineNativeBinary(exeOffPath);
  CHECK(nativeOnExit == static_cast<int>(vmResult));
  CHECK(nativeOffExit == static_cast<int>(vmResult));
  CHECK(nativeOnExit == nativeOffExit);
  CHECK(cacheOnInstrumentation.totalSpillCount < cacheOffInstrumentation.totalSpillCount);
  CHECK(cacheOnInstrumentation.totalReloadCount < cacheOffInstrumentation.totalReloadCount);
}

TEST_CASE("native backend cache mode regression matrix covers branches and call depth") {
  struct CacheModeCase {
    std::string name;
    primec::IrModule module;
  };

  std::vector<CacheModeCase> cases;

  {
    primec::IrModule module;
    module.entryIndex = 0;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 11});
    mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
    mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u}); // 2.0f
    mainFn.instructions.push_back({primec::IrOpcode::MulF32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    cases.push_back({"branch_float_conversion", std::move(module)});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction level1Fn;
    level1Fn.name = "/level1";
    level1Fn.instructions.push_back({primec::IrOpcode::Call, 2});
    level1Fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull}); // 2.0
    level1Fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
    level1Fn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    level1Fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction level2Fn;
    level2Fn.name = "/level2";
    level2Fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
    level2Fn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u}); // 2.0f
    level2Fn.instructions.push_back({primec::IrOpcode::AddF32, 0});
    level2Fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    level2Fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(level1Fn));
    module.functions.push_back(std::move(level2Fn));
    cases.push_back({"call_depth_float_int_mix", std::move(module)});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction branchFn;
    branchFn.name = "/branchFn";
    branchFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
    branchFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
    branchFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
    branchFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
    branchFn.instructions.push_back({primec::IrOpcode::Call, 2});
    branchFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    branchFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    branchFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction leafFn;
    leafFn.name = "/leaf";
    leafFn.instructions.push_back({primec::IrOpcode::PushI64, 8});
    leafFn.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
    leafFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
    leafFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    leafFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
    leafFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(branchFn));
    module.functions.push_back(std::move(leafFn));
    cases.push_back({"branch_and_call_depth", std::move(module)});
  }

  primec::NativeEmitter emitter;
  primec::NativeEmitterOptions cacheOnOptions;
  cacheOnOptions.enableRegisterCache = true;
  primec::NativeEmitterOptions cacheOffOptions;
  cacheOffOptions.enableRegisterCache = false;
  bool sawSpillOrReloadImprovement = false;

  for (const auto &testCase : cases) {
    primec::Vm vm;
    uint64_t vmResult = 0;
    std::string error;
    INFO("cache matrix case: " << testCase.name);
    REQUIRE(vm.execute(testCase.module, vmResult, error));
    CHECK(error.empty());

    primec::NativeEmitterInstrumentation cacheOnInstrumentation;
    primec::NativeEmitterInstrumentation cacheOffInstrumentation;
    const std::string exeOnPath = (std::filesystem::temp_directory_path() /
                                   ("primec_native_ir_cache_matrix_on_" + testCase.name))
                                      .string();
    const std::string exeOffPath = (std::filesystem::temp_directory_path() /
                                    ("primec_native_ir_cache_matrix_off_" + testCase.name))
                                       .string();

    REQUIRE(emitter.emitExecutable(testCase.module, exeOnPath, error, &cacheOnInstrumentation, cacheOnOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOffPath, error, &cacheOffInstrumentation, cacheOffOptions));
    CHECK(error.empty());

    const int nativeOnExit = runIrPipelineNativeBinary(exeOnPath);
    const int nativeOffExit = runIrPipelineNativeBinary(exeOffPath);
    CHECK(nativeOnExit == static_cast<int>(vmResult));
    CHECK(nativeOffExit == static_cast<int>(vmResult));
    CHECK(nativeOnExit == nativeOffExit);
    CHECK(cacheOnInstrumentation.totalValueStackPushCount == cacheOffInstrumentation.totalValueStackPushCount);
    CHECK(cacheOnInstrumentation.totalValueStackPopCount == cacheOffInstrumentation.totalValueStackPopCount);
    CHECK(cacheOnInstrumentation.totalSpillCount <= cacheOffInstrumentation.totalSpillCount);
    CHECK(cacheOnInstrumentation.totalReloadCount <= cacheOffInstrumentation.totalReloadCount);

    if (cacheOnInstrumentation.totalSpillCount < cacheOffInstrumentation.totalSpillCount ||
        cacheOnInstrumentation.totalReloadCount < cacheOffInstrumentation.totalReloadCount) {
      sawSpillOrReloadImprovement = true;
    }
  }

  CHECK(sawSpillOrReloadImprovement);
}

TEST_CASE("native backend optimization conformance perf gates enforce parity and thresholds") {
  struct PerfGateCase {
    std::string name;
    primec::IrModule module;
    uint64_t minSpillReduction = 0;
    uint64_t minReloadReduction = 0;
  };

  std::vector<PerfGateCase> cases;

  {
    primec::IrModule module;
    module.entryIndex = 0;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 10});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
    mainFn.instructions.push_back({primec::IrOpcode::DivI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
    mainFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    cases.push_back({"arithmetic_chain", std::move(module), 1u, 1u});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction helperFn;
    helperFn.name = "/helper";
    helperFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
    helperFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u}); // 2.0f
    helperFn.instructions.push_back({primec::IrOpcode::AddF32, 0});
    helperFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(helperFn));
    cases.push_back({"call_float_mix", std::move(module), 1u, 1u});
  }

  primec::NativeEmitter emitter;
  primec::NativeEmitterOptions cacheOnOptions;
  cacheOnOptions.enableRegisterCache = true;
  primec::NativeEmitterOptions cacheOffOptions;
  cacheOffOptions.enableRegisterCache = false;

  uint64_t totalSpillReduction = 0;
  uint64_t totalReloadReduction = 0;

  for (const auto &testCase : cases) {
    primec::Vm vm;
    uint64_t vmResult = 0;
    std::string error;
    INFO("perf gate case: " << testCase.name);
    REQUIRE(vm.execute(testCase.module, vmResult, error));
    CHECK(error.empty());

    primec::NativeEmitterInstrumentation cacheOffStatsA;
    primec::NativeEmitterInstrumentation cacheOnStatsA;
    primec::NativeEmitterInstrumentation cacheOffStatsB;
    primec::NativeEmitterInstrumentation cacheOnStatsB;

    const std::string exeOffPathA =
        (std::filesystem::temp_directory_path() / ("primec_native_ir_perf_gate_off_a_" + testCase.name)).string();
    const std::string exeOnPathA =
        (std::filesystem::temp_directory_path() / ("primec_native_ir_perf_gate_on_a_" + testCase.name)).string();
    const std::string exeOffPathB =
        (std::filesystem::temp_directory_path() / ("primec_native_ir_perf_gate_off_b_" + testCase.name)).string();
    const std::string exeOnPathB =
        (std::filesystem::temp_directory_path() / ("primec_native_ir_perf_gate_on_b_" + testCase.name)).string();

    REQUIRE(emitter.emitExecutable(testCase.module, exeOffPathA, error, &cacheOffStatsA, cacheOffOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOnPathA, error, &cacheOnStatsA, cacheOnOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOffPathB, error, &cacheOffStatsB, cacheOffOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOnPathB, error, &cacheOnStatsB, cacheOnOptions));
    CHECK(error.empty());

    const int offExitA = runIrPipelineNativeBinary(exeOffPathA);
    const int onExitA = runIrPipelineNativeBinary(exeOnPathA);
    const int offExitB = runIrPipelineNativeBinary(exeOffPathB);
    const int onExitB = runIrPipelineNativeBinary(exeOnPathB);
    CHECK(offExitA == static_cast<int>(vmResult));
    CHECK(onExitA == static_cast<int>(vmResult));
    CHECK(offExitB == static_cast<int>(vmResult));
    CHECK(onExitB == static_cast<int>(vmResult));
    CHECK(offExitA == onExitA);
    CHECK(offExitB == onExitB);

    CHECK(cacheOffStatsA.totalValueStackPushCount == cacheOnStatsA.totalValueStackPushCount);
    CHECK(cacheOffStatsA.totalValueStackPopCount == cacheOnStatsA.totalValueStackPopCount);
    CHECK(cacheOffStatsA.totalSpillCount >= cacheOnStatsA.totalSpillCount);
    CHECK(cacheOffStatsA.totalReloadCount >= cacheOnStatsA.totalReloadCount);

    const uint64_t spillReduction = cacheOffStatsA.totalSpillCount - cacheOnStatsA.totalSpillCount;
    const uint64_t reloadReduction = cacheOffStatsA.totalReloadCount - cacheOnStatsA.totalReloadCount;
    CHECK(spillReduction >= testCase.minSpillReduction);
    CHECK(reloadReduction >= testCase.minReloadReduction);
    totalSpillReduction += spillReduction;
    totalReloadReduction += reloadReduction;

    // Parity lock: repeated runs must produce stable optimization counters.
    CHECK(cacheOffStatsA.totalValueStackPushCount == cacheOffStatsB.totalValueStackPushCount);
    CHECK(cacheOffStatsA.totalValueStackPopCount == cacheOffStatsB.totalValueStackPopCount);
    CHECK(cacheOffStatsA.totalSpillCount == cacheOffStatsB.totalSpillCount);
    CHECK(cacheOffStatsA.totalReloadCount == cacheOffStatsB.totalReloadCount);
    CHECK(cacheOnStatsA.totalValueStackPushCount == cacheOnStatsB.totalValueStackPushCount);
    CHECK(cacheOnStatsA.totalValueStackPopCount == cacheOnStatsB.totalValueStackPopCount);
    CHECK(cacheOnStatsA.totalSpillCount == cacheOnStatsB.totalSpillCount);
    CHECK(cacheOnStatsA.totalReloadCount == cacheOnStatsB.totalReloadCount);
  }

  CHECK(totalSpillReduction >= 2u);
  CHECK(totalReloadReduction >= 2u);
}
#endif

TEST_CASE("native emitter debug dump format is deterministic and ordered") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 11;
  instrumentation.totalValueStackPushCount = 7;
  instrumentation.totalValueStackPopCount = 6;
  instrumentation.totalSpillCount = 5;
  instrumentation.totalReloadCount = 4;
  primec::NativeEmitterFunctionInstrumentation zetaStats;
  zetaStats.functionIndex = 2;
  zetaStats.functionName = "/zeta";
  zetaStats.instructionTotal = 1;
  zetaStats.valueStackPushCount = 1;
  zetaStats.valueStackPopCount = 1;
  zetaStats.spillCount = 1;
  zetaStats.reloadCount = 1;
  instrumentation.perFunction.push_back(zetaStats);

  primec::NativeEmitterFunctionInstrumentation alphaStats;
  alphaStats.functionIndex = 0;
  alphaStats.functionName = "/alpha";
  alphaStats.instructionTotal = 4;
  alphaStats.valueStackPushCount = 3;
  alphaStats.valueStackPopCount = 3;
  alphaStats.spillCount = 2;
  alphaStats.reloadCount = 2;
  instrumentation.perFunction.push_back(alphaStats);

  primec::NativeEmitterFunctionInstrumentation middleStats;
  middleStats.functionIndex = 1;
  middleStats.functionName = "/middle";
  middleStats.instructionTotal = 6;
  middleStats.valueStackPushCount = 3;
  middleStats.valueStackPopCount = 2;
  middleStats.spillCount = 2;
  middleStats.reloadCount = 1;
  instrumentation.perFunction.push_back(middleStats);

  primec::NativeEmitterOptimizationInstrumentation optimization;
  optimization.applied = true;
  optimization.instructionTotalBefore = 11;
  optimization.instructionTotalAfter = 9;
  optimization.valueStackPushCountBefore = 7;
  optimization.valueStackPushCountAfter = 5;
  optimization.valueStackPopCountBefore = 6;
  optimization.valueStackPopCountAfter = 5;
  optimization.spillCountBefore = 5;
  optimization.spillCountAfter = 2;
  optimization.reloadCountBefore = 4;
  optimization.reloadCountAfter = 2;

  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=11\n"
      "total_value_stack_push_count=7\n"
      "total_value_stack_pop_count=6\n"
      "total_spill_count=5\n"
      "total_reload_count=4\n"
      "function_count=3\n"
      "function[0].index=0\n"
      "function[0].name=/alpha\n"
      "function[0].instruction_total=4\n"
      "function[0].value_stack_push_count=3\n"
      "function[0].value_stack_pop_count=3\n"
      "function[0].spill_count=2\n"
      "function[0].reload_count=2\n"
      "function[1].index=1\n"
      "function[1].name=/middle\n"
      "function[1].instruction_total=6\n"
      "function[1].value_stack_push_count=3\n"
      "function[1].value_stack_pop_count=2\n"
      "function[1].spill_count=2\n"
      "function[1].reload_count=1\n"
      "function[2].index=2\n"
      "function[2].name=/zeta\n"
      "function[2].instruction_total=1\n"
      "function[2].value_stack_push_count=1\n"
      "function[2].value_stack_pop_count=1\n"
      "function[2].spill_count=1\n"
      "function[2].reload_count=1\n"
      "[optimization]\n"
      "applied=1\n"
      "instruction_total_before=11\n"
      "instruction_total_after=9\n"
      "value_stack_push_count_before=7\n"
      "value_stack_push_count_after=5\n"
      "value_stack_pop_count_before=6\n"
      "value_stack_pop_count_after=5\n"
      "spill_count_before=5\n"
      "spill_count_after=2\n"
      "reload_count_before=4\n"
      "reload_count_after=2\n";

  const std::string dumpA = primec::formatNativeEmitterDebugDump(instrumentation, optimization);
  const std::string dumpB = primec::formatNativeEmitterDebugDump(instrumentation, optimization);
  CHECK(dumpA == expected);
  CHECK(dumpB == expected);
}

TEST_CASE("native emitter debug dump includes optimization defaults") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 2;
  instrumentation.totalValueStackPushCount = 1;
  instrumentation.totalValueStackPopCount = 1;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;
  primec::NativeEmitterFunctionInstrumentation mainStats;
  mainStats.functionIndex = 0;
  mainStats.functionName = "/main";
  mainStats.instructionTotal = 2;
  mainStats.valueStackPushCount = 1;
  mainStats.valueStackPopCount = 1;
  mainStats.spillCount = 1;
  mainStats.reloadCount = 1;
  instrumentation.perFunction.push_back(mainStats);

  const std::string dump = primec::formatNativeEmitterDebugDump(instrumentation);
  CHECK(dump.find("[optimization]\n") != std::string::npos);
  CHECK(dump.find("applied=0\n") != std::string::npos);
  CHECK(dump.find("instruction_total_before=0\n") != std::string::npos);
  CHECK(dump.find("instruction_total_after=0\n") != std::string::npos);
}

TEST_CASE("native emitter debug dump sorts duplicate indices by name") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 6;
  instrumentation.totalValueStackPushCount = 4;
  instrumentation.totalValueStackPopCount = 4;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;

  primec::NativeEmitterFunctionInstrumentation zetaStats;
  zetaStats.functionIndex = 3;
  zetaStats.functionName = "/zeta";
  zetaStats.instructionTotal = 2;
  zetaStats.valueStackPushCount = 1;
  zetaStats.valueStackPopCount = 1;
  instrumentation.perFunction.push_back(zetaStats);

  primec::NativeEmitterFunctionInstrumentation alphaStats;
  alphaStats.functionIndex = 3;
  alphaStats.functionName = "/alpha";
  alphaStats.instructionTotal = 4;
  alphaStats.valueStackPushCount = 3;
  alphaStats.valueStackPopCount = 3;
  instrumentation.perFunction.push_back(alphaStats);

  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=6\n"
      "total_value_stack_push_count=4\n"
      "total_value_stack_pop_count=4\n"
      "total_spill_count=1\n"
      "total_reload_count=1\n"
      "function_count=2\n"
      "function[0].index=3\n"
      "function[0].name=/alpha\n"
      "function[0].instruction_total=4\n"
      "function[0].value_stack_push_count=3\n"
      "function[0].value_stack_pop_count=3\n"
      "function[0].spill_count=0\n"
      "function[0].reload_count=0\n"
      "function[1].index=3\n"
      "function[1].name=/zeta\n"
      "function[1].instruction_total=2\n"
      "function[1].value_stack_push_count=1\n"
      "function[1].value_stack_pop_count=1\n"
      "function[1].spill_count=0\n"
      "function[1].reload_count=0\n"
      "[optimization]\n"
      "applied=0\n"
      "instruction_total_before=0\n"
      "instruction_total_after=0\n"
      "value_stack_push_count_before=0\n"
      "value_stack_push_count_after=0\n"
      "value_stack_pop_count_before=0\n"
      "value_stack_pop_count_after=0\n"
      "spill_count_before=0\n"
      "spill_count_after=0\n"
      "reload_count_before=0\n"
      "reload_count_after=0\n";

  CHECK(primec::formatNativeEmitterDebugDump(instrumentation) == expected);
}

TEST_CASE("native emitter debug dump docs applied example snapshot stays stable") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 4;
  instrumentation.totalValueStackPushCount = 3;
  instrumentation.totalValueStackPopCount = 2;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;
  primec::NativeEmitterFunctionInstrumentation mainStats;
  mainStats.functionIndex = 0;
  mainStats.functionName = "/main";
  mainStats.instructionTotal = 4;
  mainStats.valueStackPushCount = 3;
  mainStats.valueStackPopCount = 2;
  mainStats.spillCount = 1;
  mainStats.reloadCount = 1;
  instrumentation.perFunction.push_back(mainStats);

  primec::NativeEmitterOptimizationInstrumentation optimization;
  optimization.applied = true;
  optimization.instructionTotalBefore = 4;
  optimization.instructionTotalAfter = 3;
  optimization.valueStackPushCountBefore = 3;
  optimization.valueStackPushCountAfter = 2;
  optimization.valueStackPopCountBefore = 2;
  optimization.valueStackPopCountAfter = 2;
  optimization.spillCountBefore = 1;
  optimization.spillCountAfter = 0;
  optimization.reloadCountBefore = 1;
  optimization.reloadCountAfter = 0;

  // Keep in sync with docs/PrimeStruct.md "Native Allocator & Scheduler (IR Optimization Path)".
  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=4\n"
      "total_value_stack_push_count=3\n"
      "total_value_stack_pop_count=2\n"
      "total_spill_count=1\n"
      "total_reload_count=1\n"
      "function_count=1\n"
      "function[0].index=0\n"
      "function[0].name=/main\n"
      "function[0].instruction_total=4\n"
      "function[0].value_stack_push_count=3\n"
      "function[0].value_stack_pop_count=2\n"
      "function[0].spill_count=1\n"
      "function[0].reload_count=1\n"
      "[optimization]\n"
      "applied=1\n"
      "instruction_total_before=4\n"
      "instruction_total_after=3\n"
      "value_stack_push_count_before=3\n"
      "value_stack_push_count_after=2\n"
      "value_stack_pop_count_before=2\n"
      "value_stack_pop_count_after=2\n"
      "spill_count_before=1\n"
      "spill_count_after=0\n"
      "reload_count_before=1\n"
      "reload_count_after=0\n";

  CHECK(primec::formatNativeEmitterDebugDump(instrumentation, optimization) == expected);
}

TEST_CASE("native emitter debug dump docs default example snapshot stays stable") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 2;
  instrumentation.totalValueStackPushCount = 1;
  instrumentation.totalValueStackPopCount = 1;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;
  primec::NativeEmitterFunctionInstrumentation mainStats;
  mainStats.functionIndex = 0;
  mainStats.functionName = "/main";
  mainStats.instructionTotal = 2;
  mainStats.valueStackPushCount = 1;
  mainStats.valueStackPopCount = 1;
  mainStats.spillCount = 1;
  mainStats.reloadCount = 1;
  instrumentation.perFunction.push_back(mainStats);

  // Keep in sync with docs/PrimeStruct.md "Native Allocator & Scheduler (IR Optimization Path)".
  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=2\n"
      "total_value_stack_push_count=1\n"
      "total_value_stack_pop_count=1\n"
      "total_spill_count=1\n"
      "total_reload_count=1\n"
      "function_count=1\n"
      "function[0].index=0\n"
      "function[0].name=/main\n"
      "function[0].instruction_total=2\n"
      "function[0].value_stack_push_count=1\n"
      "function[0].value_stack_pop_count=1\n"
      "function[0].spill_count=1\n"
      "function[0].reload_count=1\n"
      "[optimization]\n"
      "applied=0\n"
      "instruction_total_before=0\n"
      "instruction_total_after=0\n"
      "value_stack_push_count_before=0\n"
      "value_stack_push_count_after=0\n"
      "value_stack_pop_count_before=0\n"
      "value_stack_pop_count_after=0\n"
      "spill_count_before=0\n"
      "spill_count_after=0\n"
      "reload_count_before=0\n"
      "reload_count_after=0\n";

  CHECK(primec::formatNativeEmitterDebugDump(instrumentation) == expected);
}

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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program secondProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, firstProgram, error));
  CHECK(error.empty());
  REQUIRE(parseAndValidate(source, secondProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule firstModule;
  primec::IrModule secondModule;
  REQUIRE(lowerer.lower(firstProgram, "/main", {}, {}, firstModule, error));
  CHECK(error.empty());
  REQUIRE(lowerer.lower(secondProgram, "/main", {}, {}, secondModule, error));
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
  primec::Program secondProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, firstProgram, error));
  CHECK(error.empty());
  REQUIRE(parseAndValidate(source, secondProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule firstModule;
  primec::IrModule secondModule;
  REQUIRE(lowerer.lower(firstProgram, "/main", {}, {}, firstModule, error));
  CHECK(error.empty());
  REQUIRE(lowerer.lower(secondProgram, "/main", {}, {}, secondModule, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error, {"io_out"}));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {"io_out"}, {"io_out"}, module, error));
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

TEST_CASE("ir leaves tail metadata unset for builtin return") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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

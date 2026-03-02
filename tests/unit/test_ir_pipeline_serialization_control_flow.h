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

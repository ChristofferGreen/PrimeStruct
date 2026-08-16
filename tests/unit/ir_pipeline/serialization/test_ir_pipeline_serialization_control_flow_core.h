#pragma once

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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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


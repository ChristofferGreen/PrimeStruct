
TEST_CASE("ir serialize roundtrip and vm execution") {
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
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].instructions.size() == 4);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(module.functions[0].instructions[2].op == primec::IrOpcode::AddI32);
  CHECK(module.functions[0].instructions[3].op == primec::IrOpcode::ReturnI32);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  CHECK(decoded.functions[0].instructions.size() == 4);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir serialize roundtrip with implicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value(2i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions[0].instructions.size() == 3);
  CHECK(module.functions[0].instructions[2].op == primec::IrOpcode::ReturnVoid);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  REQUIRE(decoded.functions[0].instructions.size() == 3);
  CHECK(decoded.functions[0].instructions[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 7;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers literal statement with pop") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 4);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::Pop);
  CHECK(inst[2].op == primec::IrOpcode::PushI32);
  CHECK(inst[3].op == primec::IrOpcode::ReturnI32);
}

TEST_CASE("ir lowers i64 arithmetic") {
  const std::string source = R"(
[return<i64>]
main() {
  return(plus(1i64, 2i64))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 4);
  CHECK(inst[0].op == primec::IrOpcode::PushI64);
  CHECK(inst[1].op == primec::IrOpcode::PushI64);
  CHECK(inst[2].op == primec::IrOpcode::AddI64);
  CHECK(inst[3].op == primec::IrOpcode::ReturnI64);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers u64 comparison") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(0xFFFFFFFFFFFFFFFFu64, 1u64))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());

  bool sawCmp = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtU64) {
      sawCmp = true;
      break;
    }
  }
  CHECK(sawCmp);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers locals and assign") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  assign(value, plus(value, 3i32))
  return(value)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 10);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::AddI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::Pop);
  CHECK(inst[8].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[9].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers assign in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  return(assign(value, 4i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 6);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::PushI32);
  CHECK(inst[3].op == primec::IrOpcode::Dup);
  CHECK(inst[4].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[5].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers definition call by inlining") {
  const std::string source = R"(
[return<int>]
addOne([i32] x) {
  return(plus(x, 1i32))
}

[return<int>]
main() {
  return(addOne(2i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers definition call with defaults and named args") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b(2i32), [i32] c(3i32)) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3(1i32, c = 10i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 13);
}

TEST_CASE("ir lowers void definition call as statement") {
  const std::string source = R"(
touch() {
  return()
}

[return<int>]
main() {
  touch()
  return(7i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("native backend rejects recursive definition calls") {
  const std::string source = R"(
[return<int>]
loop([i32] x) {
  return(loop(x))
}

[return<int>]
main() {
  return(loop(1i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", module, error));
  CHECK(error.find("recursive") != std::string::npos);
}

TEST_CASE("ir lowers implicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value(4i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 3);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 1;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers implicit void return without transform") {
  const std::string source = R"(
main() {
  [i32] value(4i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 3);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 1;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 1);
  CHECK(inst[0].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 1;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir allows expression statements") {
  const std::string source = R"(
[return<int>]
main() {
  plus(1i32, 2i32)
  return(7i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 6);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::PushI32);
  CHECK(inst[2].op == primec::IrOpcode::AddI32);
  CHECK(inst[3].op == primec::IrOpcode::Pop);
  CHECK(inst[4].op == primec::IrOpcode::PushI32);
  CHECK(inst[5].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowers comparisons and boolean ops") {
  const std::string source = R"(
[return<int>]
main() {
  return(and(greater_than(5i32, 2i32), not_equal(3i32, 0i32)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());

  bool sawCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtI32 || inst.op == primec::IrOpcode::CmpNeI32) {
      sawCompare = true;
      break;
    }
  }
  CHECK(sawCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers boolean not/or") {
  const std::string source = R"(
[return<int>]
main() {
  return(or(not(false), false))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers numeric boolean ops") {
  const std::string source = R"(
[return<int>]
main() {
  return(and(1i32, not(0i32)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [i32 mut] witness(0i32)
  assign(value, and(equal(value, 0i32), assign(witness, 9i32)))
  return(witness)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());

  bool sawJumpIfZero = false;
  bool sawJump = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    } else if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    }
  }
  CHECK(sawJumpIfZero);
  CHECK(sawJump);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [i32 mut] witness(0i32)
  assign(value, or(equal(value, 1i32), assign(witness, 9i32)))
  return(witness)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
  CHECK(error.empty());

  bool sawJumpIfZero = false;
  bool sawJump = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    } else if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    }
  }
  CHECK(sawJumpIfZero);
  CHECK(sawJump);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers if/else to jumps") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(1i32)
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
  REQUIRE(lowerer.lower(program, "/main", module, error));
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
  return(if(false, 4i32, 9i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
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
  [i32 mut] value(0i32)
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
  REQUIRE(lowerer.lower(program, "/main", module, error));
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
  [i32 mut] value(0i32)
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
  REQUIRE(lowerer.lower(program, "/main", module, error));
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
  [i32 mut] value(1i32)
  [Pointer<i32> mut] ptr(location(value))
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
  REQUIRE(lowerer.lower(program, "/main", module, error));
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

TEST_CASE("ir lowers pointer helper calls") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  [Pointer<i32> mut] ptr(location(value))
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
  REQUIRE(lowerer.lower(program, "/main", module, error));
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


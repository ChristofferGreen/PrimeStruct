TEST_CASE("ir lowers location of reference binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  [Reference<i32> mut] ref(location(value))
  [Pointer<i32>] ptr(location(ref))
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

  int addressCount = 0;
  bool sawRefLoad = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      addressCount += 1;
    }
    if (inst.op == primec::IrOpcode::LoadLocal && inst.imm == 1) {
      sawRefLoad = true;
    }
  }
  CHECK(addressCount == 1);
  CHECK(sawRefLoad);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

TEST_CASE("ir lowers pointer plus") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(5i32)
  return(dereference(plus(location(value), 0i32)))
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

  bool sawAdd = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI64) {
      sawAdd = true;
      break;
    }
  }
  CHECK(sawAdd);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers pointer plus on reference location") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  [Reference<i32>] ref(location(first))
  return(dereference(plus(location(ref), 16i32)))
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

  bool sawAdd = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI64) {
      sawAdd = true;
      break;
    }
  }
  CHECK(sawAdd);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowers pointer minus") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(second), 16i32)))
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

  bool sawSub = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::SubI64) {
      sawSub = true;
      break;
    }
  }
  CHECK(sawSub);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers pointer minus u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(second), 16u64)))
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

  bool sawSub = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::SubI64) {
      sawSub = true;
      break;
    }
  }
  CHECK(sawSub);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers pointer minus negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(minus(location(first), -16i64)))
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

  bool sawSub = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::SubI64) {
      sawSub = true;
      break;
    }
  }
  CHECK(sawSub);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer plus uses byte offsets in VM") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(first), 16i32)))
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
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer plus accepts u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(first), 16u64)))
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
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer plus accepts negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(second), -16i64)))
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
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("pointer plus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(first), 16i64)))
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

  bool sawPushI64 = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PushI64) {
      sawPushI64 = true;
      break;
    }
  }
  CHECK(sawPushI64);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer plus accepts u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first(4i32)
  [i32] second(9i32)
  return(dereference(plus(location(first), 16u64)))
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

  bool sawPushI64 = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PushI64) {
      sawPushI64 = true;
      break;
    }
  }
  CHECK(sawPushI64);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowers reference assignments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(3i32)
  [Reference<i32> mut] ref(location(value))
  assign(ref, 8i32)
  return(ref)
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

  bool sawStoreIndirect = false;
  bool sawLoadIndirect = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::StoreIndirect) {
      sawStoreIndirect = true;
    }
    if (inst.op == primec::IrOpcode::LoadIndirect) {
      sawLoadIndirect = true;
    }
  }
  CHECK(sawStoreIndirect);
  CHECK(sawLoadIndirect);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowers location on reference without extra address-of") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [Reference<i32> mut] ref(location(value))
  [Pointer<i32> mut] ptr(location(ref))
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

  int addressOfCount = 0;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      addressOfCount += 1;
    }
  }
  CHECK(addressOfCount == 1);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowers clamp") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(5i32, 2i32, 4i32))
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

  bool sawClampCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtI32 || inst.op == primec::IrOpcode::CmpGtI32) {
      sawClampCompare = true;
      break;
    }
  }
  CHECK(sawClampCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers min") {
  const std::string source = R"(
[return<int>]
main() {
  return(min(5i32, 2i32))
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

  bool sawMinCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtI32) {
      sawMinCompare = true;
      break;
    }
  }
  CHECK(sawMinCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

TEST_CASE("ir lowers max u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(max(3u64, 7u64))
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

  bool sawMaxCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtU64) {
      sawMaxCompare = true;
      break;
    }
  }
  CHECK(sawMaxCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowers max mixed i32/i64") {
  const std::string source = R"(
[return<i64>]
main() {
  return(max(3i32, 7i64))
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

  bool sawMaxCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtI64) {
      sawMaxCompare = true;
      break;
    }
  }
  CHECK(sawMaxCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowers abs") {
  const std::string source = R"(
[return<int>]
main() {
  return(abs(negate(5i32)))
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

  bool sawAbsCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtI32) {
      sawAbsCompare = true;
      break;
    }
  }
  CHECK(sawAbsCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers sign") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(sign(5i32), sign(negate(4i32))))
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

  bool sawSignCompare = false;
  bool sawSignCompareGt = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtI32) {
      sawSignCompare = true;
    }
    if (inst.op == primec::IrOpcode::CmpGtI32) {
      sawSignCompareGt = true;
    }
  }
  CHECK(sawSignCompare);
  CHECK(sawSignCompareGt);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers saturate i32") {
  const std::string source = R"(
[return<int>]
main() {
  return(saturate(negate(3i32)))
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

  bool sawSaturateCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtI32 || inst.op == primec::IrOpcode::CmpGtI32) {
      sawSaturateCompare = true;
      break;
    }
  }
  CHECK(sawSaturateCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers saturate u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(saturate(9u64))
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

  bool sawSaturateCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtU64) {
      sawSaturateCompare = true;
      break;
    }
  }
  CHECK(sawSaturateCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers clamp u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(clamp(9u64, 2u64, 6u64))
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

  bool sawClampCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtU64 || inst.op == primec::IrOpcode::CmpGtU64) {
      sawClampCompare = true;
      break;
    }
  }
  CHECK(sawClampCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 6);
}

TEST_CASE("ir lowers clamp mixed i32/i64") {
  const std::string source = R"(
[return<i64>]
main() {
  return(clamp(9i32, 2i64, 6i32))
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

  bool sawClampCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpLtI64 || inst.op == primec::IrOpcode::CmpGtI64) {
      sawClampCompare = true;
      break;
    }
  }
  CHECK(sawClampCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 6);
}

TEST_CASE("ir lowerer rejects clamp mixed signed/unsigned") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(1i64, 2u64, 3i64))
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("ir lowers convert to int") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1i32))
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

TEST_CASE("ir lowers convert to i64") {
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(9i64))
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
  REQUIRE(inst.size() == 2);
  CHECK(inst[0].op == primec::IrOpcode::PushI64);
  CHECK(inst[1].op == primec::IrOpcode::ReturnI64);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowers convert to u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(10u64))
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
  REQUIRE(inst.size() == 2);
  CHECK(inst[0].op == primec::IrOpcode::PushI64);
  CHECK(inst[1].op == primec::IrOpcode::ReturnI64);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 10);
}

TEST_CASE("ir lowers convert to bool") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<bool>(0i32))
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
    if (inst.op == primec::IrOpcode::CmpNeI32) {
      sawCompare = true;
      break;
    }
  }
  CHECK(sawCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}


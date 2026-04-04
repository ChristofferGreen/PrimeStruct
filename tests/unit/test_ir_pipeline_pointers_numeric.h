TEST_CASE("ir lowers clamp") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(clamp(5i32, 2i32, 4i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<int>]
main() {
  return(min(5i32, 2i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<u64>]
main() {
  return(max(3u64, 7u64))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<i64>]
main() {
  return(max(3i32, 7i64))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<int>]
main() {
  return(abs(negate(5i32)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<int>]
main() {
  return(plus(sign(5i32), sign(negate(4i32))))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<int>]
main() {
  return(saturate(negate(3i32)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<u64>]
main() {
  return(saturate(9u64))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<u64>]
main() {
  return(clamp(9u64, 2u64, 6u64))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
[return<i64>]
main() {
  return(clamp(9i32, 2i64, 6i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
import /std/math/*
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
[return<bool>]
main() {
  return(convert<bool>(0i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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

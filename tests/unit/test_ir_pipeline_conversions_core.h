TEST_SUITE_BEGIN("primestruct.ir.pipeline.conversions");

TEST_CASE("ir lowers convert<bool> from u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1u64))
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

  bool sawCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpNeI64) {
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

TEST_CASE("ir lowers convert<bool> from negative i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i64))
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

  bool sawCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpNeI64) {
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

TEST_CASE("ir treats integer width convert as no-op") {
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(-1i32))
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

  auto isConvertOp = [](primec::IrOpcode op) -> bool {
    switch (op) {
      case primec::IrOpcode::ConvertI32ToF32:
      case primec::IrOpcode::ConvertI32ToF64:
      case primec::IrOpcode::ConvertI64ToF32:
      case primec::IrOpcode::ConvertI64ToF64:
      case primec::IrOpcode::ConvertU64ToF32:
      case primec::IrOpcode::ConvertU64ToF64:
      case primec::IrOpcode::ConvertF32ToI32:
      case primec::IrOpcode::ConvertF32ToI64:
      case primec::IrOpcode::ConvertF32ToU64:
      case primec::IrOpcode::ConvertF64ToI32:
      case primec::IrOpcode::ConvertF64ToI64:
      case primec::IrOpcode::ConvertF64ToU64:
      case primec::IrOpcode::ConvertF32ToF64:
      case primec::IrOpcode::ConvertF64ToF32:
        return true;
      default:
        return false;
    }
  };

  bool sawConvert = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (isConvertOp(inst.op)) {
      sawConvert = true;
      break;
    }
  }
  CHECK_FALSE(sawConvert);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == UINT64_MAX);
}

TEST_CASE("ir lowers location of reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32> mut] ref{location(value)}
  return(dereference(location(ref)))
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
  bool sawLoadLocal = false;
  bool sawLoadIndirect = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      sawAddressOf = true;
    } else if (inst.op == primec::IrOpcode::LoadLocal) {
      sawLoadLocal = true;
    } else if (inst.op == primec::IrOpcode::LoadIndirect) {
      sawLoadIndirect = true;
    }
  }
  CHECK(sawAddressOf);
  CHECK(sawLoadLocal);
  CHECK(sawLoadIndirect);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers convert<f32> from i32") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<f32>(1i32), 1.0f32))
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

  bool sawConvert = false;
  bool sawCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::ConvertI32ToF32) {
      sawConvert = true;
    } else if (inst.op == primec::IrOpcode::CmpEqF32) {
      sawCompare = true;
    }
  }
  CHECK(sawConvert);
  CHECK(sawCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers convert<int> from float literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
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

  bool sawConvert = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::ConvertF32ToI32) {
      sawConvert = true;
      break;
    }
  }
  CHECK(sawConvert);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowerer rejects string comparisons") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("a"utf8, "b"utf8))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.find("native backend does not support string comparisons") != std::string::npos);
}

TEST_CASE("ir lowers value block initializers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{
    [i32] temp{1i32}
    temp
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
  CHECK(result == 1);
}

TEST_CASE("ir lowers f32 returns") {
  const std::string source = R"(
[return<f32>]
main() {
  return(1.25f32)
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

  bool sawReturn = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::ReturnF32) {
      sawReturn = true;
      break;
    }
  }
  CHECK(sawReturn);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  uint32_t bits = static_cast<uint32_t>(result);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  CHECK(value == doctest::Approx(1.25f));
}

TEST_CASE("ir lowers f64 returns") {
  const std::string source = R"(
[return<f64>]
main() {
  return(2.5f64)
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

  bool sawReturn = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::ReturnF64) {
      sawReturn = true;
      break;
    }
  }
  CHECK(sawReturn);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  double value = 0.0;
  std::memcpy(&value, &result, sizeof(value));
  CHECK(value == doctest::Approx(2.5));
}

TEST_CASE("ir lowers bool comparison with signed integer") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
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

  bool sawCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpEqI32) {
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

TEST_CASE("ir lowerer supports string-keyed map literals") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  return(plus(count(values), at(values, "b"raw_utf8)))
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
  CHECK(result == 4);
}

TEST_CASE("ir lowerer supports math-qualified min/max") {
  const std::string source = R"(
[return<int>]
main() {
  return(/math/max(1i32, /math/min(4i32, 2i32)))
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
  CHECK(result == 2);
}

TEST_CASE("ir lowerer supports math lerp with import") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(lerp(2i32, 5i32, 2i32))
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
  CHECK(result == 8);
}

TEST_CASE("ir lowerer supports math pow with import") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, 5i32))
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
  CHECK(result == 32);
}

TEST_CASE("ir lowerer allows local binding named pi") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] pi{4i32}
  return(pi)
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
  CHECK(result == 4);
}

TEST_CASE("ir lowerer resolves imported definition calls") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
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
  CHECK(result == 5);
}

TEST_CASE("ir lowerer supports hyperbolic math") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sinh(0.0f32)}
  [f32] b{cosh(0.0f32)}
  [f32] c{tanh(0.0f32)}
  [f32] d{asinh(0.0f32)}
  [f32] e{acosh(1.0f32)}
  [f32] f{atanh(0.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, plus(e, f))))}
  return(convert<int>(sum))
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
  CHECK(result == 1);
}

TEST_CASE("ir lowerer supports float pow in native backend") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(convert<int>(plus(pow(2.0f32, 3.0f32), 0.5f32)))
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
  CHECK(result == 8);
}

TEST_CASE("ir lowerer supports math constant conversions") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(/math/pi))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports math-qualified abs/sign/saturate") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(/math/abs(-5i32), plus(/math/sign(-2i32), /math/saturate(2i32))))
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

#include "primec/ir/IrOpcodeTable.h"
#include "primec/ir/IrPureSemantics.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmKernelBoundary.h"

#include "third_party/doctest.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.pure_semantics");

namespace {

using primec::IrOpcode;
using primec::IrPureEval;

constexpr uint64_t Int64Min = 0x8000000000000000ull;
constexpr uint64_t Int64Max = 0x7FFFFFFFFFFFFFFFull;

uint64_t i64(int64_t value) {
  return static_cast<uint64_t>(value);
}
uint64_t f32Bits(float value) {
  return static_cast<uint64_t>(std::bit_cast<uint32_t>(value));
}
uint64_t f64Bits(double value) {
  return std::bit_cast<uint64_t>(value);
}

struct Evaluated {
  IrPureEval status = IrPureEval::Ok;
  uint64_t value = 0;
};

Evaluated eval(IrOpcode op, uint64_t lhs, uint64_t rhs = 0) {
  Evaluated out;
  out.status = primec::evalPureOpcode(op, lhs, rhs, out.value);
  return out;
}

// Runs `lhs rhs op` through the real VM; false on a fault.
bool runInVm(IrOpcode op, uint64_t lhs, uint64_t rhs, uint64_t &result, std::string &error) {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({IrOpcode::PushI64, lhs});
  if (primec::irPureOpcodeArity(op) == 2) {
    function.instructions.push_back({IrOpcode::PushI64, rhs});
  }
  function.instructions.push_back({op, 0});
  function.instructions.push_back({IrOpcode::ReturnI64, 0});
  module.functions.push_back(std::move(function));
  primec::Vm vm;
  return vm.execute(module, result, error);
}

} // namespace

TEST_CASE("integer arithmetic wraps on the full 64-bit slot") {
  CHECK(eval(IrOpcode::AddI64, Int64Max, 1).value == Int64Min);
  CHECK(eval(IrOpcode::SubI64, Int64Min, 1).value == Int64Max);
  CHECK(eval(IrOpcode::MulI64, Int64Max, 2).value == i64(-2));
  // The I32 forms are the same 64-bit operation: no truncation to 32 bits.
  CHECK(eval(IrOpcode::AddI32, i64(2147483647), 1).value == 2147483648ull);
  CHECK(eval(IrOpcode::MulI32, i64(65536), i64(65536)).value == 4294967296ull);
  CHECK(eval(IrOpcode::NegI32, i64(5)).value == i64(-5));
  CHECK(eval(IrOpcode::NegI64, Int64Min).value == Int64Min);
}

TEST_CASE("division is checked and signed overflow wraps instead of trapping") {
  CHECK(eval(IrOpcode::DivI64, i64(-7), 2).value == i64(-3));
  CHECK(eval(IrOpcode::DivI32, 7, i64(-2)).value == i64(-3));
  CHECK(eval(IrOpcode::DivU64, i64(-1), 2).value == 0x7FFFFFFFFFFFFFFFull);
  CHECK(eval(IrOpcode::DivI64, Int64Min, i64(-1)).value == Int64Min);
  CHECK(eval(IrOpcode::DivI32, i64(-2147483648LL), i64(-1)).value == 2147483648ull);
  CHECK(eval(IrOpcode::DivI64, 5, 0).status == IrPureEval::DivisionByZero);
  CHECK(eval(IrOpcode::DivI32, 5, 0).status == IrPureEval::DivisionByZero);
  CHECK(eval(IrOpcode::DivU64, 5, 0).status == IrPureEval::DivisionByZero);
  // Float division by zero is a value, not a fault.
  const Evaluated inf = eval(IrOpcode::DivF64, f64Bits(1.0), f64Bits(0.0));
  CHECK(inf.status == IrPureEval::Ok);
  CHECK(std::isinf(std::bit_cast<double>(inf.value)));
}

TEST_CASE("integer comparisons distinguish signed and unsigned ordering") {
  CHECK(eval(IrOpcode::CmpLtI64, i64(-1), 1).value == 1);
  CHECK(eval(IrOpcode::CmpLtU64, i64(-1), 1).value == 0);
  CHECK(eval(IrOpcode::CmpGeU64, i64(-1), 1).value == 1);
  CHECK(eval(IrOpcode::CmpEqI32, 3, 3).value == 1);
  CHECK(eval(IrOpcode::CmpNeI64, 3, 3).value == 0);
  CHECK(eval(IrOpcode::CmpLeI32, i64(-5), i64(-5)).value == 1);
}

TEST_CASE("float comparisons follow IEEE rules for NaN") {
  const uint64_t nan32 = f32Bits(std::numeric_limits<float>::quiet_NaN());
  const uint64_t one32 = f32Bits(1.0f);
  CHECK(eval(IrOpcode::CmpEqF32, nan32, nan32).value == 0);
  CHECK(eval(IrOpcode::CmpNeF32, nan32, nan32).value == 1);
  CHECK(eval(IrOpcode::CmpLtF32, nan32, one32).value == 0);
  CHECK(eval(IrOpcode::CmpGeF32, one32, nan32).value == 0);
  const uint64_t nan64 = f64Bits(std::numeric_limits<double>::quiet_NaN());
  CHECK(eval(IrOpcode::CmpLeF64, nan64, nan64).value == 0);
  CHECK(eval(IrOpcode::CmpNeF64, nan64, f64Bits(0.0)).value == 1);
  CHECK(eval(IrOpcode::CmpEqF64, f64Bits(0.0), f64Bits(-0.0)).value == 1);
}

TEST_CASE("float arithmetic keeps f32 results in the low half of the slot") {
  const Evaluated sum = eval(IrOpcode::AddF32, f32Bits(1.5f), f32Bits(2.25f));
  CHECK(sum.value == f32Bits(3.75f));
  CHECK((sum.value >> 32) == 0);
  CHECK(eval(IrOpcode::NegF32, f32Bits(2.0f)).value == f32Bits(-2.0f));
  CHECK(eval(IrOpcode::MulF64, f64Bits(1.5), f64Bits(4.0)).value == f64Bits(6.0));
  CHECK(eval(IrOpcode::NegF64, f64Bits(0.0)).value == f64Bits(-0.0));
}

TEST_CASE("conversions read the operand width the opcode names") {
  // I32 sources use the low 32 bits of the slot, I64 sources the whole slot.
  CHECK(eval(IrOpcode::ConvertI32ToF32, 0x00000000FFFFFFFFull).value == f32Bits(-1.0f));
  CHECK(eval(IrOpcode::ConvertI64ToF32, 0x00000000FFFFFFFFull).value == f32Bits(4294967295.0f));
  CHECK(eval(IrOpcode::ConvertI32ToF64, i64(-3)).value == f64Bits(-3.0));
  CHECK(eval(IrOpcode::ConvertU64ToF64, i64(-1)).value == f64Bits(18446744073709551616.0));
  CHECK(eval(IrOpcode::ConvertF32ToI32, f32Bits(-2.5f)).value == i64(-2));
  CHECK(eval(IrOpcode::ConvertF64ToI64, f64Bits(2.9)).value == 2);
  CHECK(eval(IrOpcode::ConvertF64ToU64, f64Bits(3e9)).value == 3000000000ull);
  CHECK(eval(IrOpcode::ConvertF32ToF64, f32Bits(0.5f)).value == f64Bits(0.5));
  CHECK(eval(IrOpcode::ConvertF64ToF32, f64Bits(0.25)).value == f32Bits(0.25f));
}

TEST_CASE("portability check rejects host-defined float conversions and division by zero") {
  CHECK(primec::irPureEvalIsPortable(IrOpcode::ConvertF64ToI32, f64Bits(2.5), 0));
  CHECK_FALSE(primec::irPureEvalIsPortable(IrOpcode::ConvertF64ToI32, f64Bits(3e10), 0));
  CHECK_FALSE(primec::irPureEvalIsPortable(
      IrOpcode::ConvertF64ToI64, f64Bits(std::numeric_limits<double>::quiet_NaN()), 0));
  CHECK_FALSE(primec::irPureEvalIsPortable(IrOpcode::ConvertF32ToU64, f32Bits(-5.0f), 0));
  CHECK(primec::irPureEvalIsPortable(IrOpcode::ConvertF32ToU64, f32Bits(-0.5f), 0));
  CHECK_FALSE(primec::irPureEvalIsPortable(IrOpcode::ConvertF64ToI64, f64Bits(9.3e18), 0));
  CHECK(primec::irPureEvalIsPortable(IrOpcode::ConvertF64ToI64, f64Bits(-9223372036854775808.0), 0));
  CHECK_FALSE(primec::irPureEvalIsPortable(IrOpcode::DivI64, 5, 0));
  CHECK(primec::irPureEvalIsPortable(IrOpcode::DivI64, 5, 2));
  CHECK(primec::irPureEvalIsPortable(IrOpcode::AddF32, f32Bits(1.0f), f32Bits(2.0f)));
}

TEST_CASE("pure opcode classification matches the VM kernel's") {
  for (const primec::IrOpcodeInfo &info : primec::IrOpcodeTable) {
    CAPTURE(info.name);
    CHECK(primec::isIrPureOpcode(info.op) == primec::vm_kernel::isPureNumericOpcode(info.op));
    uint64_t ignored = 0;
    if (primec::isIrPureOpcode(info.op)) {
      CHECK(primec::evalPureOpcode(info.op, 1, 1, ignored) != IrPureEval::NotPure);
      CHECK((primec::irPureOpcodeArity(info.op) == 1 || primec::irPureOpcodeArity(info.op) == 2));
    } else {
      CHECK(primec::evalPureOpcode(info.op, 1, 1, ignored) == IrPureEval::NotPure);
      CHECK(primec::irPureOpcodeArity(info.op) == 0);
    }
  }
}

TEST_CASE("evalPureOpcode agrees with the VM over an operand grid") {
  const std::vector<uint64_t> ints = {0,
                                      1,
                                      i64(-1),
                                      2,
                                      3,
                                      7,
                                      i64(-7),
                                      2147483647ull,
                                      i64(-2147483648LL),
                                      4294967296ull,
                                      Int64Max,
                                      Int64Min,
                                      0xFFFFFFFFFFFFFFFFull};
  const std::vector<float> f32Values = {0.0f,
                                        -0.0f,
                                        1.5f,
                                        -2.5f,
                                        1e30f,
                                        -1e-30f,
                                        std::numeric_limits<float>::infinity(),
                                        -std::numeric_limits<float>::infinity(),
                                        std::numeric_limits<float>::quiet_NaN()};
  const std::vector<double> f64Values = {0.0,
                                         -0.0,
                                         1.5,
                                         -2.5,
                                         1e300,
                                         -1e-300,
                                         3e9,
                                         std::numeric_limits<double>::infinity(),
                                         -std::numeric_limits<double>::infinity(),
                                         std::numeric_limits<double>::quiet_NaN()};

  // Operand kind: conversions are named after their source (ConvertF32To...),
  // everything else after its type suffix (AddF32, CmpLtF64, NegI64).
  const auto operandsFor = [&](IrOpcode op) {
    const std::string name = primec::irOpcodeInfo(op)->name;
    const auto endsWith = [&](const char *suffix) {
      const std::string s = suffix;
      return name.size() >= s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0;
    };
    bool useF32 = false;
    bool useF64 = false;
    if (name.rfind("ConvertF32", 0) == 0) {
      useF32 = true;
    } else if (name.rfind("ConvertF64", 0) == 0) {
      useF64 = true;
    } else if (name.rfind("Convert", 0) != 0) {
      useF32 = endsWith("F32");
      useF64 = endsWith("F64");
    }
    std::vector<uint64_t> values;
    if (useF32) {
      for (const float v : f32Values) {
        values.push_back(f32Bits(v));
      }
    } else if (useF64) {
      for (const double v : f64Values) {
        values.push_back(f64Bits(v));
      }
    } else {
      values = ints;
    }
    return values;
  };

  size_t compared = 0;
  for (const primec::IrOpcodeInfo &info : primec::IrOpcodeTable) {
    if (!primec::isIrPureOpcode(info.op)) {
      continue;
    }
    CAPTURE(info.name);
    const std::vector<uint64_t> operands = operandsFor(info.op);
    const bool unary = primec::irPureOpcodeArity(info.op) == 1;
    for (const uint64_t lhs : operands) {
      for (const uint64_t rhs : unary ? std::vector<uint64_t>{0} : operands) {
        // Signed overflow in the divide is undefined in the VM's own code path
        // (it traps on x86); the shared semantics define it, pinned above.
        const bool signedDivide = info.op == IrOpcode::DivI32 || info.op == IrOpcode::DivI64;
        if (signedDivide && rhs == i64(-1) && lhs == Int64Min) {
          continue;
        }
        if ((info.op == IrOpcode::NegI32 || info.op == IrOpcode::NegI64) && lhs == Int64Min) {
          continue;
        }
        CAPTURE(lhs);
        CAPTURE(rhs);
        uint64_t vmResult = 0;
        std::string vmError;
        const bool vmOk = runInVm(info.op, lhs, rhs, vmResult, vmError);
        uint64_t sharedResult = 0;
        const IrPureEval status = primec::evalPureOpcode(info.op, lhs, rhs, sharedResult);
        if (status == IrPureEval::DivisionByZero) {
          CHECK_FALSE(vmOk);
          CHECK(vmError == "division by zero in IR");
        } else {
          REQUIRE(vmOk);
          CHECK(vmResult == sharedResult);
        }
        ++compared;
      }
    }
  }
  CHECK(compared > 2000);
}

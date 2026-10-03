#pragma once

#include <array>
#include <bit>
#include <cstdint>

#include "primec/ir/Ir.h"

namespace primec {

// Value-level semantics of the pure numeric IR opcodes: arithmetic, negation,
// comparisons and conversions. The VM kernel executes these through
// evalPureOpcode, and any pass that folds or re-emits them (constant folding,
// the C++ emitters) must call it too, so every execution form agrees bit for
// bit.
//
// Operands are raw 64-bit stack slots. Integer slots hold sign-extended values;
// the I32 and I64 forms of an opcode compute identically on the full slot (an
// I32 add of two sign-extended values is a 64-bit add and is not truncated).
// Floats are IEEE bit patterns (f32 in the low 32 bits).

enum class IrPureEval : uint8_t {
  Ok,
  DivisionByZero,
  // The opcode is not a pure numeric opcode.
  NotPure,
};

// Number of stack operands the opcode consumes (1 or 2), or 0 when the opcode
// is not a pure numeric opcode. Every pure opcode produces exactly one value.
constexpr int irPureOpcodeArity(IrOpcode op) {
  switch (op) {
    case IrOpcode::NegI32:
    case IrOpcode::NegI64:
    case IrOpcode::NegF32:
    case IrOpcode::NegF64:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
      return 1;
    case IrOpcode::AddI32:
    case IrOpcode::AddI64:
    case IrOpcode::SubI32:
    case IrOpcode::SubI64:
    case IrOpcode::MulI32:
    case IrOpcode::MulI64:
    case IrOpcode::DivI32:
    case IrOpcode::DivI64:
    case IrOpcode::DivU64:
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::CmpEqI32:
    case IrOpcode::CmpNeI32:
    case IrOpcode::CmpLtI32:
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGeI32:
    case IrOpcode::CmpEqI64:
    case IrOpcode::CmpNeI64:
    case IrOpcode::CmpLtI64:
    case IrOpcode::CmpLeI64:
    case IrOpcode::CmpGtI64:
    case IrOpcode::CmpGeI64:
    case IrOpcode::CmpLtU64:
    case IrOpcode::CmpLeU64:
    case IrOpcode::CmpGtU64:
    case IrOpcode::CmpGeU64:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
      return 2;
    default:
      return 0;
  }
}

// irPureOpcodeArity as a 256-entry table, for hot paths that dispatch on the
// opcode byte (the VM kernel) and want a single load instead of a switch.
inline constexpr std::array<uint8_t, 256> IrPureOpcodeArityTable = [] {
  std::array<uint8_t, 256> table{};
  for (size_t value = 0; value < table.size(); ++value) {
    table[value] = static_cast<uint8_t>(irPureOpcodeArity(static_cast<IrOpcode>(value)));
  }
  return table;
}();

constexpr bool isIrPureOpcode(IrOpcode op) {
  return irPureOpcodeArity(op) != 0;
}

namespace ir_pure_detail {

inline float f32FromSlot(uint64_t raw) {
  return std::bit_cast<float>(static_cast<uint32_t>(raw));
}
inline uint64_t slotFromF32(float value) {
  return static_cast<uint64_t>(std::bit_cast<uint32_t>(value));
}
inline double f64FromSlot(uint64_t raw) {
  return std::bit_cast<double>(raw);
}
inline uint64_t slotFromF64(double value) {
  return std::bit_cast<uint64_t>(value);
}
inline int64_t asSigned(uint64_t raw) {
  return static_cast<int64_t>(raw);
}

} // namespace ir_pure_detail

// Evaluates a pure numeric opcode. `lhs` is the operand pushed first and `rhs`
// the top of stack for binary opcodes; unary opcodes read `lhs` only and ignore
// `rhs`. Integer overflow wraps (including INT64_MIN / -1 and negating
// INT64_MIN). Division by zero is reported, never evaluated. Float to integer
// conversions of NaN or out-of-range values are host-defined: see
// irPureEvalIsPortable.
inline IrPureEval evalPureOpcode(IrOpcode op, uint64_t lhs, uint64_t rhs, uint64_t &result) {
  using namespace ir_pure_detail;
  switch (op) {
    case IrOpcode::AddI32:
    case IrOpcode::AddI64:
      result = lhs + rhs;
      return IrPureEval::Ok;
    case IrOpcode::SubI32:
    case IrOpcode::SubI64:
      result = lhs - rhs;
      return IrPureEval::Ok;
    case IrOpcode::MulI32:
    case IrOpcode::MulI64:
      result = lhs * rhs;
      return IrPureEval::Ok;
    case IrOpcode::DivI32:
    case IrOpcode::DivI64: {
      const int64_t divisor = asSigned(rhs);
      if (divisor == 0) {
        return IrPureEval::DivisionByZero;
      }
      if (divisor == -1) {
        // INT64_MIN / -1 overflows; wrap like every other integer operation.
        result = uint64_t{0} - lhs;
        return IrPureEval::Ok;
      }
      result = static_cast<uint64_t>(asSigned(lhs) / divisor);
      return IrPureEval::Ok;
    }
    case IrOpcode::DivU64:
      if (rhs == 0) {
        return IrPureEval::DivisionByZero;
      }
      result = lhs / rhs;
      return IrPureEval::Ok;
    case IrOpcode::NegI32:
    case IrOpcode::NegI64:
      result = uint64_t{0} - lhs;
      return IrPureEval::Ok;

    case IrOpcode::AddF32:
      result = slotFromF32(f32FromSlot(lhs) + f32FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::SubF32:
      result = slotFromF32(f32FromSlot(lhs) - f32FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::MulF32:
      result = slotFromF32(f32FromSlot(lhs) * f32FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::DivF32:
      result = slotFromF32(f32FromSlot(lhs) / f32FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::NegF32:
      result = slotFromF32(-f32FromSlot(lhs));
      return IrPureEval::Ok;
    case IrOpcode::AddF64:
      result = slotFromF64(f64FromSlot(lhs) + f64FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::SubF64:
      result = slotFromF64(f64FromSlot(lhs) - f64FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::MulF64:
      result = slotFromF64(f64FromSlot(lhs) * f64FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::DivF64:
      result = slotFromF64(f64FromSlot(lhs) / f64FromSlot(rhs));
      return IrPureEval::Ok;
    case IrOpcode::NegF64:
      result = slotFromF64(-f64FromSlot(lhs));
      return IrPureEval::Ok;

    case IrOpcode::CmpEqI32:
    case IrOpcode::CmpEqI64:
      result = lhs == rhs ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpNeI32:
    case IrOpcode::CmpNeI64:
      result = lhs != rhs ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLtI32:
    case IrOpcode::CmpLtI64:
      result = asSigned(lhs) < asSigned(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpLeI64:
      result = asSigned(lhs) <= asSigned(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGtI64:
      result = asSigned(lhs) > asSigned(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGeI32:
    case IrOpcode::CmpGeI64:
      result = asSigned(lhs) >= asSigned(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLtU64:
      result = lhs < rhs ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLeU64:
      result = lhs <= rhs ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGtU64:
      result = lhs > rhs ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGeU64:
      result = lhs >= rhs ? 1 : 0;
      return IrPureEval::Ok;

    case IrOpcode::CmpEqF32:
      result = f32FromSlot(lhs) == f32FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpNeF32:
      result = f32FromSlot(lhs) != f32FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLtF32:
      result = f32FromSlot(lhs) < f32FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLeF32:
      result = f32FromSlot(lhs) <= f32FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGtF32:
      result = f32FromSlot(lhs) > f32FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGeF32:
      result = f32FromSlot(lhs) >= f32FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpEqF64:
      result = f64FromSlot(lhs) == f64FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpNeF64:
      result = f64FromSlot(lhs) != f64FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLtF64:
      result = f64FromSlot(lhs) < f64FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpLeF64:
      result = f64FromSlot(lhs) <= f64FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGtF64:
      result = f64FromSlot(lhs) > f64FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;
    case IrOpcode::CmpGeF64:
      result = f64FromSlot(lhs) >= f64FromSlot(rhs) ? 1 : 0;
      return IrPureEval::Ok;

    case IrOpcode::ConvertI32ToF32:
      result = slotFromF32(static_cast<float>(static_cast<int32_t>(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertI64ToF32:
      result = slotFromF32(static_cast<float>(asSigned(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertU64ToF32:
      result = slotFromF32(static_cast<float>(lhs));
      return IrPureEval::Ok;
    case IrOpcode::ConvertI32ToF64:
      result = slotFromF64(static_cast<double>(static_cast<int32_t>(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertI64ToF64:
      result = slotFromF64(static_cast<double>(asSigned(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertU64ToF64:
      result = slotFromF64(static_cast<double>(lhs));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF32ToI32:
      result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(f32FromSlot(lhs))));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF32ToI64:
      result = static_cast<uint64_t>(static_cast<int64_t>(f32FromSlot(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF32ToU64:
      result = static_cast<uint64_t>(f32FromSlot(lhs));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF64ToI32:
      result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(f64FromSlot(lhs))));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF64ToI64:
      result = static_cast<uint64_t>(static_cast<int64_t>(f64FromSlot(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF64ToU64:
      result = static_cast<uint64_t>(f64FromSlot(lhs));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF32ToF64:
      result = slotFromF64(static_cast<double>(f32FromSlot(lhs)));
      return IrPureEval::Ok;
    case IrOpcode::ConvertF64ToF32:
      result = slotFromF32(static_cast<float>(f64FromSlot(lhs)));
      return IrPureEval::Ok;
    default:
      return IrPureEval::NotPure;
  }
}

// True when evalPureOpcode yields the same value on every host. The float to
// integer conversions are the exception: casting NaN or an out-of-range float
// is undefined in C++ and differs between x86-64 and arm64, so a constant
// folder must leave those instructions to run time. Division by zero is also
// not portable to fold (it is a fault, not a value).
inline bool irPureEvalIsPortable(IrOpcode op, uint64_t lhs, uint64_t rhs) {
  using namespace ir_pure_detail;
  switch (op) {
    case IrOpcode::DivI32:
    case IrOpcode::DivI64:
    case IrOpcode::DivU64:
      return rhs != 0;
    case IrOpcode::ConvertF32ToI32: {
      const float v = f32FromSlot(lhs);
      return v >= -2147483648.0f && v < 2147483648.0f;
    }
    case IrOpcode::ConvertF32ToI64: {
      const float v = f32FromSlot(lhs);
      return v >= -9223372036854775808.0f && v < 9223372036854775808.0f;
    }
    case IrOpcode::ConvertF32ToU64: {
      const float v = f32FromSlot(lhs);
      return v > -1.0f && v < 18446744073709551616.0f;
    }
    case IrOpcode::ConvertF64ToI32: {
      const double v = f64FromSlot(lhs);
      return v > -2147483649.0 && v < 2147483648.0;
    }
    case IrOpcode::ConvertF64ToI64: {
      const double v = f64FromSlot(lhs);
      return v >= -9223372036854775808.0 && v < 9223372036854775808.0;
    }
    case IrOpcode::ConvertF64ToU64: {
      const double v = f64FromSlot(lhs);
      return v > -1.0 && v < 18446744073709551616.0;
    }
    default:
      (void)rhs;
      return true;
  }
}

} // namespace primec

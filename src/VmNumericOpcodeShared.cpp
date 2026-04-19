#include "VmNumericOpcodeShared.h"

#include "VmNumericHelpers.h"

namespace primec::vm_detail {

VmNumericOpcodeResult handleSharedVmNumericOpcode(const IrInstruction &inst,
                                                  std::vector<uint64_t> &stack,
                                                  std::string &error) {
  switch (inst.op) {
    case IrOpcode::AddI32:
    case IrOpcode::AddI64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on add";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t rhs = stack.back();
      stack.pop_back();
      const uint64_t lhs = stack.back();
      stack.pop_back();
      stack.push_back(lhs + rhs);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::SubI32:
    case IrOpcode::SubI64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on sub";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t rhs = stack.back();
      stack.pop_back();
      const uint64_t lhs = stack.back();
      stack.pop_back();
      stack.push_back(lhs - rhs);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::MulI32:
    case IrOpcode::MulI64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on mul";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t rhs = stack.back();
      stack.pop_back();
      const uint64_t lhs = stack.back();
      stack.pop_back();
      stack.push_back(lhs * rhs);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::DivI32:
    case IrOpcode::DivI64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on div";
        return VmNumericOpcodeResult::Fault;
      }
      const int64_t rhs = static_cast<int64_t>(stack.back());
      stack.pop_back();
      const int64_t lhs = static_cast<int64_t>(stack.back());
      stack.pop_back();
      if (rhs == 0) {
        error = "division by zero in IR";
        return VmNumericOpcodeResult::Fault;
      }
      stack.push_back(static_cast<uint64_t>(lhs / rhs));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::DivU64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on div";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t rhs = stack.back();
      stack.pop_back();
      const uint64_t lhs = stack.back();
      stack.pop_back();
      if (rhs == 0) {
        error = "division by zero in IR";
        return VmNumericOpcodeResult::Fault;
      }
      stack.push_back(lhs / rhs);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::NegI32:
    case IrOpcode::NegI64: {
      if (stack.empty()) {
        error = "IR stack underflow on negate";
        return VmNumericOpcodeResult::Fault;
      }
      const int64_t value = static_cast<int64_t>(stack.back());
      stack.pop_back();
      stack.push_back(static_cast<uint64_t>(-value));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32: {
      if (stack.size() < 2) {
        error = "IR stack underflow on float op";
        return VmNumericOpcodeResult::Fault;
      }
      const float rhs = bitsToF32(stack.back());
      stack.pop_back();
      const float lhs = bitsToF32(stack.back());
      stack.pop_back();
      float out = 0.0f;
      switch (inst.op) {
        case IrOpcode::AddF32:
          out = lhs + rhs;
          break;
        case IrOpcode::SubF32:
          out = lhs - rhs;
          break;
        case IrOpcode::MulF32:
          out = lhs * rhs;
          break;
        case IrOpcode::DivF32:
          out = lhs / rhs;
          break;
        default:
          break;
      }
      stack.push_back(f32ToBits(out));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::NegF32: {
      if (stack.empty()) {
        error = "IR stack underflow on negate";
        return VmNumericOpcodeResult::Fault;
      }
      const float value = bitsToF32(stack.back());
      stack.pop_back();
      stack.push_back(f32ToBits(-value));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on float op";
        return VmNumericOpcodeResult::Fault;
      }
      const double rhs = bitsToF64(stack.back());
      stack.pop_back();
      const double lhs = bitsToF64(stack.back());
      stack.pop_back();
      double out = 0.0;
      switch (inst.op) {
        case IrOpcode::AddF64:
          out = lhs + rhs;
          break;
        case IrOpcode::SubF64:
          out = lhs - rhs;
          break;
        case IrOpcode::MulF64:
          out = lhs * rhs;
          break;
        case IrOpcode::DivF64:
          out = lhs / rhs;
          break;
        default:
          break;
      }
      stack.push_back(f64ToBits(out));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::NegF64: {
      if (stack.empty()) {
        error = "IR stack underflow on negate";
        return VmNumericOpcodeResult::Fault;
      }
      const double value = bitsToF64(stack.back());
      stack.pop_back();
      stack.push_back(f64ToBits(-value));
      return VmNumericOpcodeResult::Continue;
    }
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
    case IrOpcode::CmpGeU64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on compare";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t rhsRaw = stack.back();
      stack.pop_back();
      const uint64_t lhsRaw = stack.back();
      stack.pop_back();
      bool cmp = false;
      switch (inst.op) {
        case IrOpcode::CmpEqI32:
        case IrOpcode::CmpEqI64:
          cmp = lhsRaw == rhsRaw;
          break;
        case IrOpcode::CmpNeI32:
        case IrOpcode::CmpNeI64:
          cmp = lhsRaw != rhsRaw;
          break;
        case IrOpcode::CmpLtI32:
        case IrOpcode::CmpLtI64:
          cmp = static_cast<int64_t>(lhsRaw) < static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpLeI32:
        case IrOpcode::CmpLeI64:
          cmp = static_cast<int64_t>(lhsRaw) <= static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpGtI32:
        case IrOpcode::CmpGtI64:
          cmp = static_cast<int64_t>(lhsRaw) > static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpGeI32:
        case IrOpcode::CmpGeI64:
          cmp = static_cast<int64_t>(lhsRaw) >= static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpLtU64:
          cmp = lhsRaw < rhsRaw;
          break;
        case IrOpcode::CmpLeU64:
          cmp = lhsRaw <= rhsRaw;
          break;
        case IrOpcode::CmpGtU64:
          cmp = lhsRaw > rhsRaw;
          break;
        case IrOpcode::CmpGeU64:
          cmp = lhsRaw >= rhsRaw;
          break;
        default:
          break;
      }
      stack.push_back(cmp ? 1 : 0);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32: {
      if (stack.size() < 2) {
        error = "IR stack underflow on compare";
        return VmNumericOpcodeResult::Fault;
      }
      const float rhs = bitsToF32(stack.back());
      stack.pop_back();
      const float lhs = bitsToF32(stack.back());
      stack.pop_back();
      bool cmp = false;
      switch (inst.op) {
        case IrOpcode::CmpEqF32:
          cmp = lhs == rhs;
          break;
        case IrOpcode::CmpNeF32:
          cmp = lhs != rhs;
          break;
        case IrOpcode::CmpLtF32:
          cmp = lhs < rhs;
          break;
        case IrOpcode::CmpLeF32:
          cmp = lhs <= rhs;
          break;
        case IrOpcode::CmpGtF32:
          cmp = lhs > rhs;
          break;
        case IrOpcode::CmpGeF32:
          cmp = lhs >= rhs;
          break;
        default:
          break;
      }
      stack.push_back(cmp ? 1 : 0);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64: {
      if (stack.size() < 2) {
        error = "IR stack underflow on compare";
        return VmNumericOpcodeResult::Fault;
      }
      const double rhs = bitsToF64(stack.back());
      stack.pop_back();
      const double lhs = bitsToF64(stack.back());
      stack.pop_back();
      bool cmp = false;
      switch (inst.op) {
        case IrOpcode::CmpEqF64:
          cmp = lhs == rhs;
          break;
        case IrOpcode::CmpNeF64:
          cmp = lhs != rhs;
          break;
        case IrOpcode::CmpLtF64:
          cmp = lhs < rhs;
          break;
        case IrOpcode::CmpLeF64:
          cmp = lhs <= rhs;
          break;
        case IrOpcode::CmpGtF64:
          cmp = lhs > rhs;
          break;
        case IrOpcode::CmpGeF64:
          cmp = lhs >= rhs;
          break;
        default:
          break;
      }
      stack.push_back(cmp ? 1 : 0);
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertU64ToF32: {
      if (stack.empty()) {
        error = "IR stack underflow on convert";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t raw = stack.back();
      stack.pop_back();
      float value = 0.0f;
      if (inst.op == IrOpcode::ConvertU64ToF32) {
        value = static_cast<float>(raw);
      } else if (inst.op == IrOpcode::ConvertI64ToF32) {
        value = static_cast<float>(static_cast<int64_t>(raw));
      } else {
        value = static_cast<float>(static_cast<int32_t>(raw));
      }
      stack.push_back(f32ToBits(value));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF64: {
      if (stack.empty()) {
        error = "IR stack underflow on convert";
        return VmNumericOpcodeResult::Fault;
      }
      const uint64_t raw = stack.back();
      stack.pop_back();
      double value = 0.0;
      if (inst.op == IrOpcode::ConvertU64ToF64) {
        value = static_cast<double>(raw);
      } else if (inst.op == IrOpcode::ConvertI64ToF64) {
        value = static_cast<double>(static_cast<int64_t>(raw));
      } else {
        value = static_cast<double>(static_cast<int32_t>(raw));
      }
      stack.push_back(f64ToBits(value));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64: {
      if (stack.empty()) {
        error = "IR stack underflow on convert";
        return VmNumericOpcodeResult::Fault;
      }
      const float value = bitsToF32(stack.back());
      stack.pop_back();
      if (inst.op == IrOpcode::ConvertF32ToU64) {
        const uint64_t out = static_cast<uint64_t>(value);
        stack.push_back(out);
      } else if (inst.op == IrOpcode::ConvertF32ToI64) {
        const int64_t out = static_cast<int64_t>(value);
        stack.push_back(static_cast<uint64_t>(out));
      } else {
        const int32_t out = static_cast<int32_t>(value);
        stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(out)));
      }
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64: {
      if (stack.empty()) {
        error = "IR stack underflow on convert";
        return VmNumericOpcodeResult::Fault;
      }
      const double value = bitsToF64(stack.back());
      stack.pop_back();
      if (inst.op == IrOpcode::ConvertF64ToU64) {
        const uint64_t out = static_cast<uint64_t>(value);
        stack.push_back(out);
      } else if (inst.op == IrOpcode::ConvertF64ToI64) {
        const int64_t out = static_cast<int64_t>(value);
        stack.push_back(static_cast<uint64_t>(out));
      } else {
        const int32_t out = static_cast<int32_t>(value);
        stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(out)));
      }
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::ConvertF32ToF64: {
      if (stack.empty()) {
        error = "IR stack underflow on convert";
        return VmNumericOpcodeResult::Fault;
      }
      const float value = bitsToF32(stack.back());
      stack.pop_back();
      const double out = static_cast<double>(value);
      stack.push_back(f64ToBits(out));
      return VmNumericOpcodeResult::Continue;
    }
    case IrOpcode::ConvertF64ToF32: {
      if (stack.empty()) {
        error = "IR stack underflow on convert";
        return VmNumericOpcodeResult::Fault;
      }
      const double value = bitsToF64(stack.back());
      stack.pop_back();
      const float out = static_cast<float>(value);
      stack.push_back(f32ToBits(out));
      return VmNumericOpcodeResult::Continue;
    }
    default:
      return VmNumericOpcodeResult::NotHandled;
  }
}

} // namespace primec::vm_detail

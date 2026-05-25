#include "primec/VmKernelBoundary.h"

#include "VmNumericHelpers.h"

namespace primec::vm_kernel {

std::string_view pureOpcodeResultName(PureOpcodeResult result) {
  switch (result) {
  case PureOpcodeResult::NotHandled:
    return "not_handled";
  case PureOpcodeResult::Continue:
    return "continue";
  case PureOpcodeResult::Fault:
    return "fault";
  }
  return "unknown";
}

bool isPureNumericOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::AddI32:
  case IrOpcode::AddI64:
  case IrOpcode::SubI32:
  case IrOpcode::SubI64:
  case IrOpcode::MulI32:
  case IrOpcode::MulI64:
  case IrOpcode::DivI32:
  case IrOpcode::DivI64:
  case IrOpcode::DivU64:
  case IrOpcode::NegI32:
  case IrOpcode::NegI64:
  case IrOpcode::AddF32:
  case IrOpcode::SubF32:
  case IrOpcode::MulF32:
  case IrOpcode::DivF32:
  case IrOpcode::NegF32:
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64:
  case IrOpcode::NegF64:
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
  case IrOpcode::ConvertI32ToF32:
  case IrOpcode::ConvertI64ToF32:
  case IrOpcode::ConvertU64ToF32:
  case IrOpcode::ConvertI32ToF64:
  case IrOpcode::ConvertI64ToF64:
  case IrOpcode::ConvertU64ToF64:
  case IrOpcode::ConvertF32ToI32:
  case IrOpcode::ConvertF32ToI64:
  case IrOpcode::ConvertF32ToU64:
  case IrOpcode::ConvertF64ToI32:
  case IrOpcode::ConvertF64ToI64:
  case IrOpcode::ConvertF64ToU64:
  case IrOpcode::ConvertF32ToF64:
  case IrOpcode::ConvertF64ToF32:
    return true;
  default:
    return false;
  }
}

PureOpcodeResult executePureNumericOpcode(const IrInstruction &inst,
                                          std::vector<std::uint64_t> &stack,
                                          std::string &error) {
  switch (inst.op) {
  case IrOpcode::AddI32:
  case IrOpcode::AddI64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on add";
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t rhs = stack.back();
    stack.pop_back();
    const std::uint64_t lhs = stack.back();
    stack.pop_back();
    stack.push_back(lhs + rhs);
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::SubI32:
  case IrOpcode::SubI64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on sub";
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t rhs = stack.back();
    stack.pop_back();
    const std::uint64_t lhs = stack.back();
    stack.pop_back();
    stack.push_back(lhs - rhs);
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::MulI32:
  case IrOpcode::MulI64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on mul";
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t rhs = stack.back();
    stack.pop_back();
    const std::uint64_t lhs = stack.back();
    stack.pop_back();
    stack.push_back(lhs * rhs);
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::DivI32:
  case IrOpcode::DivI64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on div";
      return PureOpcodeResult::Fault;
    }
    const std::int64_t rhs = static_cast<std::int64_t>(stack.back());
    stack.pop_back();
    const std::int64_t lhs = static_cast<std::int64_t>(stack.back());
    stack.pop_back();
    if (rhs == 0) {
      error = "division by zero in IR";
      return PureOpcodeResult::Fault;
    }
    stack.push_back(static_cast<std::uint64_t>(lhs / rhs));
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::DivU64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on div";
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t rhs = stack.back();
    stack.pop_back();
    const std::uint64_t lhs = stack.back();
    stack.pop_back();
    if (rhs == 0) {
      error = "division by zero in IR";
      return PureOpcodeResult::Fault;
    }
    stack.push_back(lhs / rhs);
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::NegI32:
  case IrOpcode::NegI64: {
    if (stack.empty()) {
      error = "IR stack underflow on negate";
      return PureOpcodeResult::Fault;
    }
    const std::int64_t value = static_cast<std::int64_t>(stack.back());
    stack.pop_back();
    stack.push_back(static_cast<std::uint64_t>(-value));
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::AddF32:
  case IrOpcode::SubF32:
  case IrOpcode::MulF32:
  case IrOpcode::DivF32: {
    if (stack.size() < 2) {
      error = "IR stack underflow on float op";
      return PureOpcodeResult::Fault;
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
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::NegF32: {
    if (stack.empty()) {
      error = "IR stack underflow on negate";
      return PureOpcodeResult::Fault;
    }
    const float value = bitsToF32(stack.back());
    stack.pop_back();
    stack.push_back(f32ToBits(-value));
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on float op";
      return PureOpcodeResult::Fault;
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
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::NegF64: {
    if (stack.empty()) {
      error = "IR stack underflow on negate";
      return PureOpcodeResult::Fault;
    }
    const double value = bitsToF64(stack.back());
    stack.pop_back();
    stack.push_back(f64ToBits(-value));
    return PureOpcodeResult::Continue;
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
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t rhsRaw = stack.back();
    stack.pop_back();
    const std::uint64_t lhsRaw = stack.back();
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
      cmp = static_cast<std::int64_t>(lhsRaw) <
            static_cast<std::int64_t>(rhsRaw);
      break;
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpLeI64:
      cmp = static_cast<std::int64_t>(lhsRaw) <=
            static_cast<std::int64_t>(rhsRaw);
      break;
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGtI64:
      cmp = static_cast<std::int64_t>(lhsRaw) >
            static_cast<std::int64_t>(rhsRaw);
      break;
    case IrOpcode::CmpGeI32:
    case IrOpcode::CmpGeI64:
      cmp = static_cast<std::int64_t>(lhsRaw) >=
            static_cast<std::int64_t>(rhsRaw);
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
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::CmpEqF32:
  case IrOpcode::CmpNeF32:
  case IrOpcode::CmpLtF32:
  case IrOpcode::CmpLeF32:
  case IrOpcode::CmpGtF32:
  case IrOpcode::CmpGeF32: {
    if (stack.size() < 2) {
      error = "IR stack underflow on compare";
      return PureOpcodeResult::Fault;
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
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::CmpEqF64:
  case IrOpcode::CmpNeF64:
  case IrOpcode::CmpLtF64:
  case IrOpcode::CmpLeF64:
  case IrOpcode::CmpGtF64:
  case IrOpcode::CmpGeF64: {
    if (stack.size() < 2) {
      error = "IR stack underflow on compare";
      return PureOpcodeResult::Fault;
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
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::ConvertI32ToF32:
  case IrOpcode::ConvertI64ToF32:
  case IrOpcode::ConvertU64ToF32: {
    if (stack.empty()) {
      error = "IR stack underflow on convert";
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t raw = stack.back();
    stack.pop_back();
    float value = 0.0f;
    if (inst.op == IrOpcode::ConvertU64ToF32) {
      value = static_cast<float>(raw);
    } else if (inst.op == IrOpcode::ConvertI64ToF32) {
      value = static_cast<float>(static_cast<std::int64_t>(raw));
    } else {
      value = static_cast<float>(static_cast<std::int32_t>(raw));
    }
    stack.push_back(f32ToBits(value));
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::ConvertI32ToF64:
  case IrOpcode::ConvertI64ToF64:
  case IrOpcode::ConvertU64ToF64: {
    if (stack.empty()) {
      error = "IR stack underflow on convert";
      return PureOpcodeResult::Fault;
    }
    const std::uint64_t raw = stack.back();
    stack.pop_back();
    double value = 0.0;
    if (inst.op == IrOpcode::ConvertU64ToF64) {
      value = static_cast<double>(raw);
    } else if (inst.op == IrOpcode::ConvertI64ToF64) {
      value = static_cast<double>(static_cast<std::int64_t>(raw));
    } else {
      value = static_cast<double>(static_cast<std::int32_t>(raw));
    }
    stack.push_back(f64ToBits(value));
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::ConvertF32ToI32:
  case IrOpcode::ConvertF32ToI64:
  case IrOpcode::ConvertF32ToU64: {
    if (stack.empty()) {
      error = "IR stack underflow on convert";
      return PureOpcodeResult::Fault;
    }
    const float value = bitsToF32(stack.back());
    stack.pop_back();
    if (inst.op == IrOpcode::ConvertF32ToU64) {
      stack.push_back(static_cast<std::uint64_t>(value));
    } else if (inst.op == IrOpcode::ConvertF32ToI64) {
      const std::int64_t out = static_cast<std::int64_t>(value);
      stack.push_back(static_cast<std::uint64_t>(out));
    } else {
      const std::int32_t out = static_cast<std::int32_t>(value);
      stack.push_back(static_cast<std::uint64_t>(static_cast<std::int64_t>(out)));
    }
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::ConvertF64ToI32:
  case IrOpcode::ConvertF64ToI64:
  case IrOpcode::ConvertF64ToU64: {
    if (stack.empty()) {
      error = "IR stack underflow on convert";
      return PureOpcodeResult::Fault;
    }
    const double value = bitsToF64(stack.back());
    stack.pop_back();
    if (inst.op == IrOpcode::ConvertF64ToU64) {
      stack.push_back(static_cast<std::uint64_t>(value));
    } else if (inst.op == IrOpcode::ConvertF64ToI64) {
      const std::int64_t out = static_cast<std::int64_t>(value);
      stack.push_back(static_cast<std::uint64_t>(out));
    } else {
      const std::int32_t out = static_cast<std::int32_t>(value);
      stack.push_back(static_cast<std::uint64_t>(static_cast<std::int64_t>(out)));
    }
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::ConvertF32ToF64: {
    if (stack.empty()) {
      error = "IR stack underflow on convert";
      return PureOpcodeResult::Fault;
    }
    const float value = bitsToF32(stack.back());
    stack.pop_back();
    const double out = static_cast<double>(value);
    stack.push_back(f64ToBits(out));
    return PureOpcodeResult::Continue;
  }
  case IrOpcode::ConvertF64ToF32: {
    if (stack.empty()) {
      error = "IR stack underflow on convert";
      return PureOpcodeResult::Fault;
    }
    const double value = bitsToF64(stack.back());
    stack.pop_back();
    const float out = static_cast<float>(value);
    stack.push_back(f32ToBits(out));
    return PureOpcodeResult::Continue;
  }
  default:
    return PureOpcodeResult::NotHandled;
  }
}

} // namespace primec::vm_kernel

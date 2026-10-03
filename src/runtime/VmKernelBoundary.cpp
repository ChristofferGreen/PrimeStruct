#include "primec/runtime/VmKernelBoundary.h"

#include "primec/ir/IrPureSemantics.h"

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
  return isIrPureOpcode(op);
}

const char *pureOpcodeUnderflowMessage(IrOpcode op) {
  switch (op) {
  case IrOpcode::AddI32:
  case IrOpcode::AddI64:
    return "IR stack underflow on add";
  case IrOpcode::SubI32:
  case IrOpcode::SubI64:
    return "IR stack underflow on sub";
  case IrOpcode::MulI32:
  case IrOpcode::MulI64:
    return "IR stack underflow on mul";
  case IrOpcode::DivI32:
  case IrOpcode::DivI64:
  case IrOpcode::DivU64:
    return "IR stack underflow on div";
  case IrOpcode::NegI32:
  case IrOpcode::NegI64:
  case IrOpcode::NegF32:
  case IrOpcode::NegF64:
    return "IR stack underflow on negate";
  case IrOpcode::SextI32:
    return "IR stack underflow on sext";
  case IrOpcode::AddF32:
  case IrOpcode::SubF32:
  case IrOpcode::MulF32:
  case IrOpcode::DivF32:
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64:
    return "IR stack underflow on float op";
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
    return "IR stack underflow on convert";
  default:
    return "IR stack underflow on compare";
  }
}

// The value semantics live in primec/ir/IrPureSemantics.h so the VM, constant
// folding and the C++ emitters cannot disagree; this function only moves
// operands between the operand stack and evalPureOpcode.
PureOpcodeResult executePureNumericOpcode(const IrInstruction &inst,
                                          std::vector<std::uint64_t> &stack,
                                          std::string &error) {
  const int arity = irPureOpcodeArity(inst.op);
  if (arity == 0) {
    return PureOpcodeResult::NotHandled;
  }
  if (stack.size() < static_cast<size_t>(arity)) {
    error = pureOpcodeUnderflowMessage(inst.op);
    return PureOpcodeResult::Fault;
  }
  std::uint64_t rhs = 0;
  if (arity == 2) {
    rhs = stack.back();
    stack.pop_back();
  }
  const std::uint64_t lhs = stack.back();
  stack.pop_back();
  std::uint64_t result = 0;
  switch (evalPureOpcode(inst.op, lhs, rhs, result)) {
  case IrPureEval::Ok:
    stack.push_back(result);
    return PureOpcodeResult::Continue;
  case IrPureEval::DivisionByZero:
    error = "division by zero in IR";
    return PureOpcodeResult::Fault;
  case IrPureEval::NotPure:
    break;
  }
  return PureOpcodeResult::NotHandled;
}

} // namespace primec::vm_kernel

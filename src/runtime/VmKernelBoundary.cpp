#include "primec/runtime/VmKernelBoundary.h"
#include "primec/ir/IrOpcodeTable.h"

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
  const IrOpArith arith = irOpcodeArith(op);
  switch (irOpcodeCategory(op)) {
  case IrOpCategory::IntArith:
  case IrOpCategory::FloatArith:
    if (arith == IrOpArith::Neg) {
      return "IR stack underflow on negate";
    }
    if (irOpcodeCategory(op) == IrOpCategory::FloatArith) {
      return "IR stack underflow on float op";
    }
    switch (arith) {
    case IrOpArith::Add:
      return "IR stack underflow on add";
    case IrOpArith::Sub:
      return "IR stack underflow on sub";
    case IrOpArith::Mul:
      return "IR stack underflow on mul";
    default:
      return "IR stack underflow on div";
    }
  case IrOpCategory::SignExtend:
    return "IR stack underflow on sext";
  case IrOpCategory::ZeroExtend:
    return "IR stack underflow on zext";
  case IrOpCategory::Convert:
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

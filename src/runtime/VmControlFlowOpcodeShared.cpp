#include "VmControlFlowOpcodeShared.h"

namespace primec::vm_detail {

VmControlFlowOpcodeOutcome handleSharedVmControlFlowOpcode(const IrInstruction &inst,
                                                           std::vector<uint64_t> &stack,
                                                           size_t instructionCount,
                                                           size_t functionCount,
                                                           size_t frameCount,
                                                           size_t maxCallDepth,
                                                           bool currentFrameReturnsValueToCaller,
                                                           size_t &ip,
                                                           std::string &error) {
  VmControlFlowOpcodeOutcome outcome;

  switch (inst.op) {
    case IrOpcode::JumpIfZero: {
      if (stack.empty()) {
        error = "IR stack underflow on jump";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      const uint64_t value = stack.back();
      stack.pop_back();
      if (inst.imm > instructionCount) {
        error = "invalid jump target in IR";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      if (value == 0) {
        ip = static_cast<size_t>(inst.imm);
      } else {
        ip += 1;
      }
      outcome.result = VmControlFlowOpcodeResult::Continue;
      return outcome;
    }
    case IrOpcode::Jump: {
      if (inst.imm > instructionCount) {
        error = "invalid jump target in IR";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      ip = static_cast<size_t>(inst.imm);
      outcome.result = VmControlFlowOpcodeResult::Continue;
      return outcome;
    }
    case IrOpcode::Call:
    case IrOpcode::CallVoid: {
      if (inst.imm >= functionCount) {
        error = "invalid call target in IR";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      if (frameCount >= maxCallDepth) {
        error = "VM call stack overflow";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      outcome.targetFunctionIndex = static_cast<size_t>(inst.imm);
      outcome.returnValueToCaller = inst.op == IrOpcode::Call;
      ip += 1;
      outcome.result = VmControlFlowOpcodeResult::Call;
      return outcome;
    }
    case IrOpcode::ReturnVoid: {
      outcome.returnValueToCaller = currentFrameReturnsValueToCaller;
      outcome.returnValue = 0;
      outcome.result = frameCount == 1 ? VmControlFlowOpcodeResult::Exit
                                       : VmControlFlowOpcodeResult::Return;
      return outcome;
    }
    case IrOpcode::ReturnI32: {
      if (stack.empty()) {
        error = "IR stack underflow on return";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      outcome.returnValue =
          static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(stack.back())));
      stack.pop_back();
      outcome.returnValueToCaller = currentFrameReturnsValueToCaller;
      outcome.result = frameCount == 1 ? VmControlFlowOpcodeResult::Exit
                                       : VmControlFlowOpcodeResult::Return;
      return outcome;
    }
    case IrOpcode::ReturnI64:
    case IrOpcode::ReturnF64: {
      if (stack.empty()) {
        error = "IR stack underflow on return";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      outcome.returnValue = stack.back();
      stack.pop_back();
      outcome.returnValueToCaller = currentFrameReturnsValueToCaller;
      outcome.result = frameCount == 1 ? VmControlFlowOpcodeResult::Exit
                                       : VmControlFlowOpcodeResult::Return;
      return outcome;
    }
    case IrOpcode::ReturnF32: {
      if (stack.empty()) {
        error = "IR stack underflow on return";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      outcome.returnValue = static_cast<uint64_t>(static_cast<uint32_t>(stack.back()));
      stack.pop_back();
      outcome.returnValueToCaller = currentFrameReturnsValueToCaller;
      outcome.result = frameCount == 1 ? VmControlFlowOpcodeResult::Exit
                                       : VmControlFlowOpcodeResult::Return;
      return outcome;
    }
    default:
      outcome.result = VmControlFlowOpcodeResult::NotHandled;
      return outcome;
  }
}

} // namespace primec::vm_detail

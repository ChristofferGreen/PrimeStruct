#include "VmControlFlowOpcodeShared.h"

namespace primec::vm_detail {

VmControlFlowOpcodeOutcome handleSharedVmControlFlowOpcode(const IrInstruction &inst,
                                                           std::vector<uint64_t> &stack,
                                                           size_t instructionCount,
                                                           const std::vector<IrFunction> &functions,
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
      if (inst.imm >= functions.size()) {
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
    case IrOpcode::CallIndirect:
    case IrOpcode::CallIndirectVoid: {
      // The address is the deepest of the call's `imm + 1` slots; the callee finds its parameters above it.
      const size_t parameterCount = static_cast<size_t>(inst.imm);
      if (stack.size() < parameterCount + 1) {
        error = "IR stack underflow on indirect call";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      const size_t addressIndex = stack.size() - parameterCount - 1;
      const uint64_t address = stack[addressIndex];
      if (address == 0) {
        error = "indirect call through a null function address";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      if (address > functions.size()) {
        error = "indirect call through an invalid function address";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      const size_t target = static_cast<size_t>(address - 1);
      if (functions[target].parameterCount != parameterCount) {
        error = "indirect call passes " + std::to_string(parameterCount) + " parameters to " +
                functions[target].name + ", which takes " +
                std::to_string(functions[target].parameterCount);
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      if (frameCount >= maxCallDepth) {
        error = "VM call stack overflow";
        outcome.result = VmControlFlowOpcodeResult::Fault;
        return outcome;
      }
      stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(addressIndex));
      outcome.targetFunctionIndex = target;
      outcome.returnValueToCaller = inst.op == IrOpcode::CallIndirect;
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
    case IrOpcode::Trap: {
      // The program ends here with the trap's exit code, whatever frame runs it.
      outcome.returnValue = inst.imm;
      outcome.returnValueToCaller = false;
      outcome.result = VmControlFlowOpcodeResult::Exit;
      return outcome;
    }
    default:
      outcome.result = VmControlFlowOpcodeResult::NotHandled;
      return outcome;
  }
}

} // namespace primec::vm_detail

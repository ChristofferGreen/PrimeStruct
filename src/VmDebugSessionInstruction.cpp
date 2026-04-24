#include "primec/Vm.h"

#include "VmControlFlowOpcodeShared.h"
#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"
#include "VmDebugSessionInstructionNumeric.h"

namespace primec {

VmDebugSession::StepOutcome VmDebugSession::stepInstruction(std::string &error) {
  constexpr uint64_t kSlotBytes = IrSlotBytes;
  constexpr size_t MaxCallDepth = 4096;
  if (!module_) {
    error = "debug session has no active module";
    return StepOutcome::Fault;
  }
  if (frames_.empty()) {
    error = "debug session has no active frame";
    return StepOutcome::Fault;
  }
  Frame &frame = frames_.back();
  const IrFunction &fn = *frame.function;
  std::vector<uint64_t> &locals = frame.locals;
  size_t &ip = frame.ip;
  if (ip >= fn.instructions.size()) {
    if (frames_.size() == 1) {
      error = "missing return in IR";
    } else {
      error = "missing return in IR function " + fn.name;
    }
    appendMappedStackTrace(error);
    return StepOutcome::Fault;
  }
  const IrInstruction &inst = fn.instructions[ip];
  auto emitInstructionHook = [&](VmDebugInstructionHook hook) {
    if (!hook) {
      return;
    }
    VmDebugInstructionHookEvent event;
    event.sequence = nextHookSequence_++;
    event.snapshot = snapshot();
    event.opcode = inst.op;
    event.immediate = inst.imm;
    hook(event, hooks_.userData);
  };
  auto emitCallHook = [&](VmDebugCallHook hook, size_t functionIndex, bool returnsValueToCaller) {
    if (!hook) {
      return;
    }
    VmDebugCallHookEvent event;
    event.sequence = nextHookSequence_++;
    event.snapshot = snapshot();
    event.functionIndex = functionIndex;
    event.returnsValueToCaller = returnsValueToCaller;
    hook(event, hooks_.userData);
  };
  auto finishStep = [&](StepOutcome outcome) {
    emitInstructionHook(hooks_.afterInstruction);
    return outcome;
  };
  auto finishFault = [&]() {
    appendMappedStackTrace(error);
    if (hooks_.fault) {
      VmDebugFaultHookEvent event;
      event.sequence = nextHookSequence_++;
      event.snapshot = snapshot();
      event.opcode = inst.op;
      event.immediate = inst.imm;
      event.message = error;
      hooks_.fault(event, hooks_.userData);
    }
    return StepOutcome::Fault;
  };
  emitInstructionHook(hooks_.beforeInstruction);
  const auto numericResult = vm_debug_detail::handleVmDebugNumericOpcode(inst, stack_, error);
  if (numericResult != vm_debug_detail::OpcodeBlockResult::NotHandled) {
    if (numericResult == vm_debug_detail::OpcodeBlockResult::Fault) {
      return finishFault();
    }
    ip += 1;
    return finishStep(StepOutcome::Continue);
  }
  const auto controlFlowOutcome = vm_detail::handleSharedVmControlFlowOpcode(inst,
                                                                             stack_,
                                                                             fn.instructions.size(),
                                                                             module_ ? module_->functions.size() : 0,
                                                                             frames_.size(),
                                                                             MaxCallDepth,
                                                                             frame.returnValueToCaller,
                                                                             ip,
                                                                             error);
  if (controlFlowOutcome.result == vm_detail::VmControlFlowOpcodeResult::Fault) {
    return finishFault();
  }
  if (controlFlowOutcome.result == vm_detail::VmControlFlowOpcodeResult::Continue) {
    return finishStep(StepOutcome::Continue);
  }
  if (controlFlowOutcome.result == vm_detail::VmControlFlowOpcodeResult::Call) {
    Frame calleeFrame;
    calleeFrame.functionIndex = controlFlowOutcome.targetFunctionIndex;
    calleeFrame.function = &module_->functions[controlFlowOutcome.targetFunctionIndex];
    calleeFrame.locals.assign(localCounts_[controlFlowOutcome.targetFunctionIndex], 0);
    calleeFrame.returnValueToCaller = controlFlowOutcome.returnValueToCaller;
    frames_.push_back(std::move(calleeFrame));
    emitCallHook(hooks_.callPush,
                 controlFlowOutcome.targetFunctionIndex,
                 controlFlowOutcome.returnValueToCaller);
    return finishStep(StepOutcome::Continue);
  }
  if (controlFlowOutcome.result == vm_detail::VmControlFlowOpcodeResult::Exit) {
    const size_t poppedFunctionIndex = frame.functionIndex;
    result_ = controlFlowOutcome.returnValue;
    frames_.clear();
    emitCallHook(hooks_.callPop,
                 poppedFunctionIndex,
                 controlFlowOutcome.returnValueToCaller);
    return finishStep(StepOutcome::Exit);
  }
  if (controlFlowOutcome.result == vm_detail::VmControlFlowOpcodeResult::Return) {
    const size_t poppedFunctionIndex = frame.functionIndex;
    frames_.pop_back();
    if (controlFlowOutcome.returnValueToCaller) {
      stack_.push_back(controlFlowOutcome.returnValue);
    }
    emitCallHook(hooks_.callPop,
                 poppedFunctionIndex,
                 controlFlowOutcome.returnValueToCaller);
    return finishStep(StepOutcome::Continue);
  }
  switch (inst.op) {
    case IrOpcode::PushI32:
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(inst.imm))));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    case IrOpcode::PushI64:
      stack_.push_back(inst.imm);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
      stack_.push_back(inst.imm);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    case IrOpcode::PushArgc: {
      const int32_t count32 = static_cast<int32_t>(argCount_);
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(count32)));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return finishFault();
      }
      stack_.push_back(locals[static_cast<size_t>(inst.imm)]);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::StoreLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return finishFault();
      }
      if (stack_.empty()) {
        error = "IR stack underflow on store";
        return finishFault();
      }
      locals[static_cast<size_t>(inst.imm)] = stack_.back();
      stack_.pop_back();
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::AddressOfLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return finishFault();
      }
      stack_.push_back(static_cast<uint64_t>(inst.imm) * kSlotBytes);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadIndirect: {
      if (stack_.empty()) {
        error = "IR stack underflow on load indirect";
        return finishFault();
      }
      const uint64_t address = stack_.back();
      stack_.pop_back();
      uint64_t *slot = nullptr;
      if (!vm_detail::resolveIndirectAddress(address, kSlotBytes, locals, heapSlots_, heapAllocations_, slot, error)) {
        return finishFault();
      }
      stack_.push_back(*slot);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::StoreIndirect: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on store indirect";
        return finishFault();
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      const uint64_t address = stack_.back();
      stack_.pop_back();
      uint64_t *slot = nullptr;
      if (!vm_detail::resolveIndirectAddress(address, kSlotBytes, locals, heapSlots_, heapAllocations_, slot, error)) {
        return finishFault();
      }
      *slot = value;
      stack_.push_back(value);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::HeapAlloc: {
      if (stack_.empty()) {
        error = "IR stack underflow on heap alloc";
        return finishFault();
      }
      const uint64_t slotCount = stack_.back();
      stack_.pop_back();
      uint64_t address = 0;
      if (!vm_detail::allocateVmHeapSlots(slotCount, kSlotBytes, heapSlots_, heapAllocations_, address, error)) {
        return finishFault();
      }
      stack_.push_back(address);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::HeapFree: {
      if (stack_.empty()) {
        error = "IR stack underflow on heap free";
        return finishFault();
      }
      const uint64_t address = stack_.back();
      stack_.pop_back();
      if (!vm_detail::freeVmHeapSlots(address, kSlotBytes, heapSlots_, heapAllocations_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::HeapRealloc: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on heap realloc";
        return finishFault();
      }
      const uint64_t slotCount = stack_.back();
      stack_.pop_back();
      const uint64_t address = stack_.back();
      stack_.pop_back();
      uint64_t newAddress = 0;
      if (!vm_detail::reallocVmHeapSlots(address,
                                         slotCount,
                                         kSlotBytes,
                                         heapSlots_,
                                         heapAllocations_,
                                         newAddress,
                                         error)) {
        return finishFault();
      }
      stack_.push_back(newAddress);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::Dup: {
      if (stack_.empty()) {
        error = "IR stack underflow on dup";
        return finishFault();
      }
      stack_.push_back(stack_.back());
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::Pop: {
      if (stack_.empty()) {
        error = "IR stack underflow on pop";
        return finishFault();
      }
      stack_.pop_back();
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintI32: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintI64: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintU64: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintString: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintStringDynamic: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintArgv: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintArgvUnsafe: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, argvViews_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileClose: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileReadByte: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileFlush: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteI32: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteI64: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteU64: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteString: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteStringDynamic: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteByte: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteNewline: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadStringByte: {
      if (stack_.empty()) {
        error = "IR stack underflow on string index";
        return finishFault();
      }
      const uint64_t indexRaw = stack_.back();
      stack_.pop_back();
      const uint64_t stringIndex = inst.imm;
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return finishFault();
      }
      const std::string &text = module_->stringTable[static_cast<size_t>(stringIndex)];
      const size_t index = static_cast<size_t>(indexRaw);
      if (index >= text.size()) {
        error = "string index out of bounds in IR";
        return finishFault();
      }
      const uint8_t byte = static_cast<uint8_t>(text[index]);
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte))));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadStringLength: {
      if (stack_.empty()) {
        error = "IR stack underflow on string index";
        return finishFault();
      }
      const uint64_t stringIndex = stack_.back();
      stack_.pop_back();
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return finishFault();
      }
      stack_.push_back(
          static_cast<uint64_t>(module_->stringTable[static_cast<size_t>(stringIndex)].size()));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    default:
      error = "unknown IR opcode";
      return finishFault();
  }
}

} // namespace primec

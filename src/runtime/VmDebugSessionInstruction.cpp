#include "primec/runtime/Vm.h"

#include "VmDebugTasks.h"

namespace primec {

const std::vector<VmDebugSession::Frame> &VmDebugSession::currentFrames() const {
  return tasks_ ? *tasks_->scheduler.currentView().frames : frames_;
}

const std::vector<uint64_t> &VmDebugSession::currentStack() const {
  return tasks_ ? *tasks_->scheduler.currentView().stack : stack_;
}

VmDebugSession::StepOutcome VmDebugSession::stepInstruction(std::string &error) {
  if (!module_) {
    error = "debug session has no active module";
    return StepOutcome::Fault;
  }
  if (usesTasks_ && !tasks_) {
    tasks_ = std::make_shared<VmDebugTasks>(*module_,
                                            argCount_,
                                            argvViews_,
                                            hostFunctions_ ? &*hostFunctions_ : nullptr,
                                            heap_,
                                            *textHeap_,
                                            localCounts_,
                                            scheduleSeed_);
  }
  if (currentFrames().empty()) {
    error = "debug session has no active frame";
    return StepOutcome::Fault;
  }
  const Frame &frame = currentFrames().back();
  const IrFunction &fn = *frame.function;
  if (frame.ip >= fn.instructions.size()) {
    if (currentFrames().size() == 1) {
      error = "missing return in IR";
    } else {
      error = "missing return in IR function " + fn.name;
    }
    appendMappedStackTrace(error);
    return StepOutcome::Fault;
  }
  const IrInstruction &inst = fn.instructions[frame.ip];
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

  vm_detail::VmKernelStepEvent event;
  vm_detail::VmKernelStepOutcome stepped;
  if (tasks_) {
    stepped = tasks_->scheduler.step(event, result_, error);
  } else {
    VmDebugKernelHost host(
        argCount_, argvViews_, hostFunctions_ ? &*hostFunctions_ : nullptr, heap_, *textHeap_);
    stepped = vm_detail::stepVmKernel(
        *module_, host, stack_, frames_, localCounts_, result_, event, error);
  }
  switch (stepped) {
  case vm_detail::VmKernelStepOutcome::Fault:
    return finishFault();
  case vm_detail::VmKernelStepOutcome::Exit:
    frames_.clear();
    emitCallHook(hooks_.callPop, event.functionIndex, event.returnsValueToCaller);
    return finishStep(StepOutcome::Exit);
  case vm_detail::VmKernelStepOutcome::Continue:
    break;
  }
  if (event.kind == vm_detail::VmKernelStepKind::Call) {
    emitCallHook(hooks_.callPush, event.functionIndex, event.returnsValueToCaller);
  } else if (event.kind == vm_detail::VmKernelStepKind::Return ||
             event.kind == vm_detail::VmKernelStepKind::Exit) {
    // An Exit that continues is the end of a spawned task.
    emitCallHook(hooks_.callPop, event.functionIndex, event.returnsValueToCaller);
  }
  return finishStep(StepOutcome::Continue);
}

} // namespace primec

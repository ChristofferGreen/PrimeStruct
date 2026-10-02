#include "primec/runtime/Vm.h"

#include "VmExecution.h"
#include "VmHeapHelpers.h"
#include "primec/runtime/VmStringHeap.h"
#include "VmIoHelpers.h"

namespace primec {

namespace {

// Serves one kernel step from the session's own heap, argv and host bindings,
// so debug sessions run exactly the dispatch `Vm::execute` runs.
class DebugSessionKernelHost final : public vm_detail::VmKernelHost {
public:
  DebugSessionKernelHost(uint64_t argCount,
                         const std::vector<std::string_view> *args,
                         const VmHostFunctions *hostFunctions,
                         std::vector<uint64_t> &heapSlots,
                         std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                         vm_detail::VmStringHeap &stringHeap)
      : argCount_(argCount),
        args_(args),
        hostFunctions_(hostFunctions),
        heapSlots_(heapSlots),
        heapAllocations_(heapAllocations),
        stringHeap_(stringHeap) {}

  const vm_detail::VmStringHeap *stringHeap() const override { return &stringHeap_; }
  uint64_t argumentCount() const override { return argCount_; }
  uint64_t slotBytes() const override { return IrSlotBytes; }
  size_t maxCallDepth() const override { return 4096; }

  bool resolveIndirectAddress(uint64_t address,
                              std::vector<uint64_t> &locals,
                              uint64_t *&slot,
                              std::string &error) override {
    return vm_detail::resolveIndirectAddress(
        address, slotBytes(), locals, heapSlots_, heapAllocations_, slot, error);
  }
  bool allocateHeapSlots(uint64_t slotCount, uint64_t &address, std::string &error) override {
    return vm_detail::allocateVmHeapSlots(slotCount, slotBytes(), heapSlots_, heapAllocations_, address, error);
  }
  bool freeHeapSlots(uint64_t address, std::string &error) override {
    return vm_detail::freeVmHeapSlots(address, slotBytes(), heapSlots_, heapAllocations_, error);
  }
  bool reallocHeapSlots(uint64_t address,
                        uint64_t slotCount,
                        uint64_t &newAddress,
                        std::string &error) override {
    return vm_detail::reallocVmHeapSlots(
        address, slotCount, slotBytes(), heapSlots_, heapAllocations_, newAddress, error);
  }
  bool handlePrintInstruction(const IrModule &module,
                              const IrInstruction &inst,
                              std::vector<uint64_t> &stack,
                              std::string &error) override {
    return vm_detail::handlePrintOpcode(module, inst, stack, args_, error, &stringHeap_);
  }
  bool handleFileInstruction(const IrModule &module,
                             const IrInstruction &inst,
                             std::vector<uint64_t> &stack,
                             std::vector<uint64_t> &locals,
                             std::string &error) override {
    return vm_detail::handleFileOpcode(module, inst, stack, locals, error, &stringHeap_);
  }
  bool handleHostCall(const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error) override {
    return vm_detail::handleVmHostCall(hostFunctions_, module, inst, stack, error, &stringHeap_);
  }

private:
  uint64_t argCount_ = 0;
  const std::vector<std::string_view> *args_ = nullptr;
  const VmHostFunctions *hostFunctions_ = nullptr;
  std::vector<uint64_t> &heapSlots_;
  std::vector<VmDebugSession::HeapAllocation> &heapAllocations_;
  vm_detail::VmStringHeap &stringHeap_;
};

} // namespace

VmDebugSession::StepOutcome VmDebugSession::stepInstruction(std::string &error) {
  if (!module_) {
    error = "debug session has no active module";
    return StepOutcome::Fault;
  }
  if (frames_.empty()) {
    error = "debug session has no active frame";
    return StepOutcome::Fault;
  }
  const Frame &frame = frames_.back();
  const IrFunction &fn = *frame.function;
  if (frame.ip >= fn.instructions.size()) {
    if (frames_.size() == 1) {
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

  DebugSessionKernelHost host(argCount_,
                              argvViews_,
                              hostFunctions_ ? &*hostFunctions_ : nullptr,
                              heapSlots_,
                              heapAllocations_,
                              *stringHeap_);
  vm_detail::VmKernelStepEvent event;
  switch (vm_detail::stepVmKernel(*module_, host, stack_, frames_, localCounts_, result_, event, error)) {
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
  } else if (event.kind == vm_detail::VmKernelStepKind::Return) {
    emitCallHook(hooks_.callPop, event.functionIndex, event.returnsValueToCaller);
  }
  return finishStep(StepOutcome::Continue);
}

} // namespace primec

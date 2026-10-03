#include "primec/runtime/VmExecutionKernel.h"

#include "VmControlFlowOpcodeShared.h"
#include "primec/ir/IrPureSemantics.h"
#include "primec/runtime/VmStringHeap.h"
#include "primec/runtime/VmKernelBoundary.h"

#include <algorithm>
#include <utility>

namespace primec::vm_detail {

namespace {

} // namespace

bool isVmKernelPrintOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::PrintI32:
  case IrOpcode::PrintI64:
  case IrOpcode::PrintU64:
  case IrOpcode::PrintString:
  case IrOpcode::PrintStringDynamic:
  case IrOpcode::PrintArgv:
  case IrOpcode::PrintArgvUnsafe:
    return true;
  default:
    return false;
  }
}

bool isVmKernelFileOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::FileOpenRead:
  case IrOpcode::FileOpenWrite:
  case IrOpcode::FileOpenAppend:
  case IrOpcode::FileOpenReadDynamic:
  case IrOpcode::FileOpenWriteDynamic:
  case IrOpcode::FileOpenAppendDynamic:
  case IrOpcode::FileClose:
  case IrOpcode::FileReadByte:
  case IrOpcode::FileFlush:
  case IrOpcode::FileWriteI32:
  case IrOpcode::FileWriteI64:
  case IrOpcode::FileWriteU64:
  case IrOpcode::FileWriteString:
  case IrOpcode::FileWriteStringDynamic:
  case IrOpcode::FileWriteByte:
  case IrOpcode::FileWriteNewline:
    return true;
  default:
    return false;
  }
}

size_t computeVmKernelLocalCount(const IrFunction &function) {
  size_t localCount = 0;
  for (const auto &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
        inst.op == IrOpcode::AddressOfLocal) {
      localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
    }
  }
  return localCount;
}

namespace {

// The single dispatch implementation: `executeVmKernel` loops over it and
// debug sessions call it through `stepVmKernel`. Defined in this unit so the
// run loop can inline it.
// `TrackEvents` is false for plain runs, which skip filling the step event.
template <bool TrackEvents>
[[gnu::always_inline]] inline VmKernelStepOutcome stepImpl(const IrModule &module,
                                                           VmKernelHost &host,
                                                           std::vector<uint64_t> &stack,
                                                           std::vector<VmKernelFrame> &frames,
                                                           const std::vector<size_t> &localCounts,
                                                           uint64_t &result,
                                                           VmKernelStepEvent &event,
                                                           std::string &error) {
  if constexpr (TrackEvents) {
    event = {};
  }
  VmKernelFrame &frame = frames.back();
  const IrFunction &fn = *frame.function;
  std::vector<uint64_t> &locals = frame.locals;
  size_t &ip = frame.ip;
  if (ip >= fn.instructions.size()) {
    if (frames.size() == 1) {
      error = "missing return in IR";
    } else {
      error = "missing return in IR function " + fn.name;
    }
    return VmKernelStepOutcome::Fault;
  }
  const auto &inst = fn.instructions[ip];
  const auto controlFlowOutcome =
      handleSharedVmControlFlowOpcode(inst,
                                      stack,
                                      fn.instructions.size(),
                                      module.functions.size(),
                                      frames.size(),
                                      host.maxCallDepth(),
                                      frame.returnValueToCaller,
                                      ip,
                                      error);
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Fault) {
    return VmKernelStepOutcome::Fault;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Continue) {
    return VmKernelStepOutcome::Continue;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Call) {
    VmKernelFrame calleeFrame;
    calleeFrame.functionIndex = controlFlowOutcome.targetFunctionIndex;
    calleeFrame.function = &module.functions[controlFlowOutcome.targetFunctionIndex];
    calleeFrame.locals.assign(localCounts[controlFlowOutcome.targetFunctionIndex], 0);
    calleeFrame.returnValueToCaller = controlFlowOutcome.returnValueToCaller;
    frames.push_back(std::move(calleeFrame));
    if constexpr (TrackEvents) {
      event.kind = VmKernelStepKind::Call;
      event.functionIndex = controlFlowOutcome.targetFunctionIndex;
      event.returnsValueToCaller = controlFlowOutcome.returnValueToCaller;
    }
    return VmKernelStepOutcome::Continue;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Exit) {
    result = controlFlowOutcome.returnValue;
    if constexpr (TrackEvents) {
      event.kind = VmKernelStepKind::Exit;
      event.functionIndex = frame.functionIndex;
      event.returnsValueToCaller = controlFlowOutcome.returnValueToCaller;
    }
    return VmKernelStepOutcome::Exit;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Return) {
    const bool returnToCaller = controlFlowOutcome.returnValueToCaller;
    if constexpr (TrackEvents) {
      event.kind = VmKernelStepKind::Return;
      event.functionIndex = frame.functionIndex;
      event.returnsValueToCaller = returnToCaller;
    }
    frames.pop_back();
    if (returnToCaller) {
      stack.push_back(controlFlowOutcome.returnValue);
    }
    return VmKernelStepOutcome::Continue;
  }
  switch (inst.op) {
  case IrOpcode::PushI32:
    stack.push_back(static_cast<uint64_t>(
        static_cast<int64_t>(static_cast<int32_t>(inst.imm))));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::PushI64:
  case IrOpcode::PushF32:
  case IrOpcode::PushF64:
    stack.push_back(inst.imm);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::PushArgc: {
    const int32_t count32 = static_cast<int32_t>(host.argumentCount());
    stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(count32)));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadLocal: {
    if (static_cast<size_t>(inst.imm) >= locals.size()) {
      error = "invalid local index in IR";
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(locals[static_cast<size_t>(inst.imm)]);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::StoreLocal: {
    if (static_cast<size_t>(inst.imm) >= locals.size()) {
      error = "invalid local index in IR";
      return VmKernelStepOutcome::Fault;
    }
    if (stack.empty()) {
      error = "IR stack underflow on store";
      return VmKernelStepOutcome::Fault;
    }
    locals[static_cast<size_t>(inst.imm)] = stack.back();
    stack.pop_back();
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::AddressOfLocal: {
    if (static_cast<size_t>(inst.imm) >= locals.size()) {
      error = "invalid local index in IR";
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(static_cast<uint64_t>(inst.imm) * host.slotBytes());
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadIndirect: {
    if (stack.empty()) {
      error = "IR stack underflow on load indirect";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t address = stack.back();
    stack.pop_back();
    uint64_t *slot = nullptr;
    if (!host.resolveIndirectAddress(address, locals, slot, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(*slot);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::StoreIndirect: {
    if (stack.size() < 2) {
      error = "IR stack underflow on store indirect";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t value = stack.back();
    stack.pop_back();
    const uint64_t address = stack.back();
    stack.pop_back();
    uint64_t *slot = nullptr;
    if (!host.resolveIndirectAddress(address, locals, slot, error)) {
      return VmKernelStepOutcome::Fault;
    }
    *slot = value;
    stack.push_back(value);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapAlloc: {
    if (stack.empty()) {
      error = "IR stack underflow on heap alloc";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t slotCount = stack.back();
    stack.pop_back();
    uint64_t address = 0;
    if (!host.allocateHeapSlots(slotCount, address, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(address);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapFree: {
    if (stack.empty()) {
      error = "IR stack underflow on heap free";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t address = stack.back();
    stack.pop_back();
    if (!host.freeHeapSlots(address, error)) {
      return VmKernelStepOutcome::Fault;
    }
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapRealloc: {
    if (stack.size() < 2) {
      error = "IR stack underflow on heap realloc";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t slotCount = stack.back();
    stack.pop_back();
    const uint64_t address = stack.back();
    stack.pop_back();
    uint64_t newAddress = 0;
    if (!host.reallocHeapSlots(address, slotCount, newAddress, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(newAddress);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::Dup:
    if (stack.empty()) {
      error = "IR stack underflow on dup";
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(stack.back());
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::Pop:
    if (stack.empty()) {
      error = "IR stack underflow on pop";
      return VmKernelStepOutcome::Fault;
    }
    stack.pop_back();
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::LoadStringByte:
  case IrOpcode::LoadStringByteDynamic: {
    // LoadStringByte carries the string index as an immediate; the dynamic form
    // pops it (below the byte position).
    const bool dynamic = inst.op == IrOpcode::LoadStringByteDynamic;
    if (stack.size() < (dynamic ? 2u : 1u)) {
      error = "IR stack underflow on string index";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t indexRaw = stack.back();
    stack.pop_back();
    uint64_t stringIndex = inst.imm;
    if (dynamic) {
      stringIndex = stack.back();
      stack.pop_back();
    }
    const std::string *text = nullptr;
    if (!resolveVmString(module, host.stringHeap(), stringIndex, text, error)) {
      return VmKernelStepOutcome::Fault;
    }
    const size_t index = static_cast<size_t>(indexRaw);
    if (index >= text->size()) {
      error = "string index out of bounds in IR";
      return VmKernelStepOutcome::Fault;
    }
    const uint8_t byte = static_cast<uint8_t>((*text)[index]);
    stack.push_back(static_cast<uint64_t>(
        static_cast<int64_t>(static_cast<int32_t>(byte))));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadStringLength: {
    if (stack.empty()) {
      error = "IR stack underflow on string index";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t stringIndex = stack.back();
    stack.pop_back();
    const std::string *text = nullptr;
    if (!resolveVmString(module, host.stringHeap(), stringIndex, text, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(static_cast<uint64_t>(text->size()));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  default:
    // Arithmetic, comparisons and conversions: operands are read in place and
    // the result overwrites the lower operand, with the value semantics shared
    // with constant folding and the C++ emitters (IrPureSemantics.h).
    if (const size_t arity = IrPureOpcodeArityTable[static_cast<uint8_t>(inst.op)]; arity != 0) {
      const size_t size = stack.size();
      if (size < arity) {
        error = vm_kernel::pureOpcodeUnderflowMessage(inst.op);
        return VmKernelStepOutcome::Fault;
      }
      const uint64_t rhs = stack[size - 1];
      const uint64_t lhs = arity == 2 ? stack[size - 2] : rhs;
      uint64_t result = 0;
      if (evalPureOpcode(inst.op, lhs, rhs, result) != IrPureEval::Ok) {
        // The only fault a pure opcode has; the operands are consumed.
        stack.resize(size - arity);
        error = "division by zero in IR";
        return VmKernelStepOutcome::Fault;
      }
      if (arity == 2) {
        stack.pop_back();
      }
      stack.back() = result;
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    if (isVmKernelPrintOpcode(inst.op)) {
      if (!host.handlePrintInstruction(module, inst, stack, error)) {
        return VmKernelStepOutcome::Fault;
      }
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    if (isVmKernelFileOpcode(inst.op)) {
      if (!host.handleFileInstruction(module, inst, stack, locals, error)) {
        return VmKernelStepOutcome::Fault;
      }
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    if (inst.op == IrOpcode::CallHost) {
      if (!host.handleHostCall(module, inst, stack, error)) {
        return VmKernelStepOutcome::Fault;
      }
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    error = "unknown IR opcode";
    return VmKernelStepOutcome::Fault;
  }
}

} // namespace

VmKernelStepOutcome stepVmKernel(const IrModule &module,
                                 VmKernelHost &host,
                                 std::vector<uint64_t> &stack,
                                 std::vector<VmKernelFrame> &frames,
                                 const std::vector<size_t> &localCounts,
                                 uint64_t &result,
                                 VmKernelStepEvent &event,
                                 std::string &error) {
  return stepImpl<true>(module, host, stack, frames, localCounts, result, event, error);
}

bool executeVmKernel(const IrModule &module,
                     VmKernelHost &host,
                     uint64_t &result,
                     std::string &error) {
  if (module.entryIndex < 0 ||
      static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }

  std::vector<size_t> localCounts(module.functions.size(), 0);
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts[i] = computeVmKernelLocalCount(module.functions[i]);
  }

  std::vector<uint64_t> stack;
  std::vector<VmKernelFrame> frames;
  frames.reserve(64);
  VmKernelFrame entryFrame;
  entryFrame.functionIndex = static_cast<size_t>(module.entryIndex);
  entryFrame.function = &module.functions[entryFrame.functionIndex];
  entryFrame.locals.assign(localCounts[entryFrame.functionIndex], 0);
  frames.push_back(std::move(entryFrame));

  VmKernelStepEvent event;
  while (!frames.empty()) {
    switch (stepImpl<false>(module, host, stack, frames, localCounts, result, event, error)) {
    case VmKernelStepOutcome::Continue:
      break;
    case VmKernelStepOutcome::Exit:
      return true;
    case VmKernelStepOutcome::Fault:
      return false;
    }
  }

  error = "missing return in IR";
  return false;
}

} // namespace primec::vm_detail

#include "VmNativeJitHost.h"

#include "VmIoHelpers.h"
#include "primec/ir/IrCfg.h"
#include "primec/ir/IrPureSemantics.h"
#include "primec/runtime/VmExecutionKernel.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <limits>

namespace primec::vm_detail {
namespace {

// Data page words (X64Emitter::JitData*): context, bridge, heap values, slot count, slot states.
constexpr size_t DataHostContext = 4;
constexpr size_t DataHostBridge = 5;
constexpr size_t DataHeapBase = 6;
constexpr size_t DataHeapSlots = 7;
constexpr size_t DataHeapStates = 8;

} // namespace

VmNativeJitHost::VmNativeJitHost(const IrModule &module, const std::vector<std::string_view> &args)
    : module_(module), args_(args) {
  localCounts_.reserve(module.functions.size());
  for (const IrFunction &function : module.functions) {
    localCounts_.push_back(computeVmKernelLocalCount(function));
  }
}

void VmNativeJitHost::attach(uint64_t *dataPage) {
  data_ = dataPage;
  data_[DataHostContext] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
  data_[DataHostBridge] =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&VmNativeJitHost::bridge));
  publishHeap();
}

void VmNativeJitHost::publishHeap() {
  if (data_ != nullptr) {
    data_[DataHeapBase] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(heap_.slots.data()));
    data_[DataHeapSlots] = static_cast<uint64_t>(heap_.slots.size());
    data_[DataHeapStates] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(heap_.states.data()));
  }
}

uint64_t
VmNativeJitHost::bridge(void *host, uint64_t site, uint64_t *operandTop, uint64_t *frameLocals) {
  auto &self = *static_cast<VmNativeJitHost *>(host);
  const uint32_t functionIndex = static_cast<uint32_t>(site >> 32);
  const uint32_t instructionIndex = static_cast<uint32_t>(site);
  const IrInstruction &inst = self.module_.functions[functionIndex].instructions[instructionIndex];
  IrStackEffect effect;
  if (!computeIrStackEffect(inst, self.module_, effect)) {
    self.error_ = "unknown IR opcode";
    return 1;
  }
  // Operand i (deepest first) of n sits 16 (n - 1 - i) bytes above the top, value in word 1.
  self.stack_.clear();
  for (uint32_t i = 0; i < effect.pops; ++i) {
    self.stack_.push_back(operandTop[1 + 2 * (effect.pops - 1 - i)]);
  }
  bool ok = false;
  try {
    ok = self.run(functionIndex, inst, frameLocals);
  } catch (const std::exception &exception) {
    self.error_ = exception.what();
  }
  if (!ok) {
    return 1;
  }
  if (self.stack_.size() != effect.pushes) {
    self.error_ = "native JIT bridge stack mismatch";
    return 1;
  }
  uint64_t *resultTop = operandTop + 2 * (static_cast<int64_t>(effect.pops) - effect.pushes);
  for (uint32_t j = 0; j < effect.pushes; ++j) {
    resultTop[1 + 2 * (effect.pushes - 1 - j)] = self.stack_[j];
  }
  return 0;
}

bool VmNativeJitHost::run(uint32_t functionIndex,
                          const IrInstruction &inst,
                          uint64_t *frameLocals) {
  switch (inst.op) {
  case IrOpcode::HeapAlloc: {
    uint64_t address = 0;
    if (!heap_.allocate(stack_.back(), address, error_)) {
      return false;
    }
    stack_.back() = address;
    publishHeap();
    return true;
  }
  case IrOpcode::HeapFree: {
    const uint64_t address = stack_.back();
    stack_.pop_back();
    return heap_.release(address, error_);
  }
  case IrOpcode::HeapRealloc: {
    const uint64_t slotCount = stack_.back();
    stack_.pop_back();
    uint64_t address = 0;
    if (!heap_.reallocate(stack_.back(), slotCount, address, error_)) {
      return false;
    }
    stack_.back() = address;
    publishHeap();
    return true;
  }
  case IrOpcode::LoadStringByteDynamic: {
    const uint64_t position = stack_.back();
    stack_.pop_back();
    const uint64_t stringIndex = stack_.back();
    stack_.pop_back();
    const std::string *text = nullptr;
    if (!resolveVmString(module_, &strings_, stringIndex, text, error_)) {
      return false;
    }
    if (static_cast<size_t>(position) >= text->size()) {
      error_ = "string index out of bounds in IR";
      return false;
    }
    const uint8_t byte = static_cast<uint8_t>((*text)[static_cast<size_t>(position)]);
    stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte))));
    return true;
  }
  default:
    break;
  }
  if (const size_t arity = IrPureOpcodeArityTable[static_cast<uint8_t>(inst.op)]; arity != 0) {
    const uint64_t rhs = stack_.back();
    const uint64_t lhs = arity == 2 ? stack_[stack_.size() - 2] : rhs;
    uint64_t result = 0;
    if (evalPureOpcode(inst.op, lhs, rhs, result) != IrPureEval::Ok) {
      error_ = "division by zero in IR";
      return false;
    }
    stack_.resize(stack_.size() - arity);
    stack_.push_back(result);
    return true;
  }
  if (isVmKernelPrintOpcode(inst.op)) {
    const bool ok = handlePrintOpcode(module_, inst, stack_, &args_, error_, &strings_, nullptr);
    // The code writes to the descriptors directly; keep the order.
    std::fflush(stdout);
    std::fflush(stderr);
    return ok;
  }
  if (isVmKernelFileOpcode(inst.op)) {
    // Only FileReadByte touches a local: the VM's frame has localCounts_ slots.
    std::vector<uint64_t> locals(localCounts_[functionIndex], 0);
    const bool readsLocal = inst.op == IrOpcode::FileReadByte && inst.imm < locals.size();
    const size_t slot = static_cast<size_t>(inst.imm);
    if (readsLocal) {
      locals[slot] = frameLocals[2 * slot];
    }
    const bool ok = handleFileOpcode(module_, inst, stack_, locals, error_, &strings_, nullptr);
    if (readsLocal) {
      frameLocals[2 * slot] = locals[slot];
    }
    std::fflush(stdout);
    std::fflush(stderr);
    return ok;
  }
  error_ = "unknown IR opcode";
  return false;
}

} // namespace primec::vm_detail

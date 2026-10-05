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

constexpr uint64_t HeapAddressTag = uint64_t{1} << 63;

// Data page words (X64Emitter::JitData*): context, bridge, heap values, slot count, live bytes.
constexpr size_t DataHostContext = 4;
constexpr size_t DataHostBridge = 5;
constexpr size_t DataHeapBase = 6;
constexpr size_t DataHeapSlots = 7;
constexpr size_t DataHeapLive = 8;

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
    data_[DataHeapBase] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(heap_.data()));
    data_[DataHeapSlots] = static_cast<uint64_t>(heap_.size());
    data_[DataHeapLive] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(live_.data()));
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
    if (!allocate(stack_.back(), address)) {
      return false;
    }
    stack_.back() = address;
    return true;
  }
  case IrOpcode::HeapFree: {
    const uint64_t address = stack_.back();
    stack_.pop_back();
    return release(address);
  }
  case IrOpcode::HeapRealloc: {
    const uint64_t slotCount = stack_.back();
    stack_.pop_back();
    uint64_t address = 0;
    if (!reallocate(stack_.back(), slotCount, address)) {
      return false;
    }
    stack_.back() = address;
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

VmNativeJitHost::Allocation *VmNativeJitHost::allocationStartingAt(uint64_t baseIndex) {
  // Allocations are appended in address order.
  auto it = std::lower_bound(
      allocations_.begin(),
      allocations_.end(),
      baseIndex,
      [](const Allocation &allocation, uint64_t value) { return allocation.baseIndex < value; });
  if (it == allocations_.end() || it->baseIndex != baseIndex) {
    return nullptr;
  }
  return &*it;
}

void VmNativeJitHost::clearSlots(uint64_t baseIndex, uint64_t slotCount) {
  std::fill_n(heap_.begin() + static_cast<std::ptrdiff_t>(baseIndex), slotCount, 0);
  std::fill_n(live_.begin() + static_cast<std::ptrdiff_t>(baseIndex), slotCount, 0);
}

bool VmNativeJitHost::allocate(uint64_t slotCount, uint64_t &address) {
  if (slotCount == 0) {
    address = 0;
    return true;
  }
  // The VM's limits (VmHeapHelpers.cpp) on its vector of 8-byte slots.
  const uint64_t baseIndex = heap_.size();
  const uint64_t maxSlots = std::vector<uint64_t>().max_size();
  if (slotCount > maxSlots - baseIndex) {
    error_ = "VM heap allocation overflow";
    return false;
  }
  if (baseIndex > (std::numeric_limits<uint64_t>::max() - HeapAddressTag) / IrSlotBytes) {
    error_ = "VM heap allocation overflow";
    return false;
  }
  heap_.resize(static_cast<size_t>(baseIndex + slotCount), 0);
  live_.resize(static_cast<size_t>(baseIndex + slotCount), 1);
  allocations_.push_back({baseIndex, slotCount, true});
  address = HeapAddressTag + baseIndex * IrSlotBytes;
  publishHeap();
  return true;
}

bool VmNativeJitHost::release(uint64_t address) {
  if (address == 0) {
    return true;
  }
  Allocation *allocation = (address & HeapAddressTag) == 0 || address % IrSlotBytes != 0
                               ? nullptr
                               : allocationStartingAt((address & ~HeapAddressTag) / IrSlotBytes);
  if (allocation == nullptr || !allocation->live) {
    error_ = "invalid heap free address in IR: " + std::to_string(address);
    return false;
  }
  clearSlots(allocation->baseIndex, allocation->slotCount);
  allocation->live = false;
  return true;
}

bool VmNativeJitHost::reallocate(uint64_t address, uint64_t slotCount, uint64_t &newAddress) {
  if (address == 0) {
    return allocate(slotCount, newAddress);
  }
  if (slotCount == 0) {
    if (!release(address)) {
      return false;
    }
    newAddress = 0;
    return true;
  }
  const uint64_t oldBase = (address & ~HeapAddressTag) / IrSlotBytes;
  const Allocation *found = (address & HeapAddressTag) == 0 || address % IrSlotBytes != 0
                                ? nullptr
                                : allocationStartingAt(oldBase);
  if (found == nullptr || !found->live) {
    error_ = "invalid heap realloc address in IR: " + std::to_string(address);
    return false;
  }
  const uint64_t oldCount = found->slotCount; // allocating below may move the list
  if (!allocate(slotCount, newAddress)) {
    return false;
  }
  const uint64_t newBase = (newAddress & ~HeapAddressTag) / IrSlotBytes;
  const uint64_t copied = std::min(oldCount, slotCount);
  std::copy_n(heap_.begin() + static_cast<std::ptrdiff_t>(oldBase),
              copied,
              heap_.begin() + static_cast<std::ptrdiff_t>(newBase));
  clearSlots(oldBase, oldCount);
  allocationStartingAt(oldBase)->live = false;
  return true;
}

} // namespace primec::vm_detail

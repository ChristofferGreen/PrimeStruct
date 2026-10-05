#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmStringHeap.h"

namespace primec::vm_detail {

// The runtime side of the native JIT (primec/backend/NativeJit.h). The code calls `bridge` for
// the opcodes it has no machine code for (nativeJitBridgesOpcode in
// src/native_emitter/NativeJitOpcodes.h), which runs them with the VM's own handlers and
// messages: the heap, prints of argv and dynamic strings, files, dynamic string bytes, f32
// arithmetic and the float conversions the code leaves out.
//
// The heap is kept in the layout the code reads directly: the VM's slot values (a VM heap
// address without its tag is 16 times the slot index) and one byte per slot that is nonzero
// while the slot's allocation is live. Allocation, freeing and reallocation follow the VM's
// (VmHeapHelpers.cpp): the same addresses, the same zeroing and the same faults.
class VmNativeJitHost {
public:
  VmNativeJitHost(const IrModule &module, const std::vector<std::string_view> &args);

  // Writes this host's context, bridge and heap fields into the code's data page.
  void attach(uint64_t *dataPage);

  // bridge(host, functionIndex << 32 | instructionIndex, operand stack top, frame locals): the
  // instruction's operands are the top entries of the code's operand stack (16 bytes each, the
  // value in the upper word, top first) and its results replace them the same way; frame local
  // k is at frameLocals[2 k]. Returns 0, or nonzero with the VM's message in error().
  static uint64_t bridge(void *host, uint64_t site, uint64_t *operandTop, uint64_t *frameLocals);

  const std::string &error() const {
    return error_;
  }

private:
  struct Allocation {
    uint64_t baseIndex = 0;
    uint64_t slotCount = 0;
    bool live = false;
  };

  bool run(uint32_t functionIndex, const IrInstruction &inst, uint64_t *frameLocals);
  bool allocate(uint64_t slotCount, uint64_t &address);
  bool release(uint64_t address);
  bool reallocate(uint64_t address, uint64_t slotCount, uint64_t &newAddress);
  Allocation *allocationStartingAt(uint64_t baseIndex);
  void clearSlots(uint64_t baseIndex, uint64_t slotCount);
  void publishHeap();

  const IrModule &module_;
  const std::vector<std::string_view> &args_;
  std::vector<size_t> localCounts_;
  std::vector<uint64_t> heap_;
  std::vector<uint8_t> live_;
  std::vector<Allocation> allocations_;
  VmStringHeap strings_;
  std::vector<uint64_t> stack_;
  uint64_t *data_ = nullptr;
  std::string error_;
};

} // namespace primec::vm_detail

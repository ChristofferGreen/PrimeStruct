#pragma once

#include "primec/ir/Ir.h"
#include "primec/ir/IrLocalEscape.h"

#include <algorithm>
#include <cstdint>

namespace primec::native_emitter {

// Whether the VM could reach this function's locals through memory: it takes an address, or it
// dereferences one, which the VM resolves against the current frame whatever its origin. The
// JIT keeps such a function's locals in their frame slots, zeroed at entry as in the VM.
[[maybe_unused]] inline bool jitFrameAccessible(const IrFunction &function) {
  if (analyzeIrLocalEscape(function).addressTaken) {
    return true;
  }
  // The runtime reads and writes frame slots too: FileReadByte stores into its local.
  for (const IrInstruction &instruction : function.instructions) {
    if (instruction.op == IrOpcode::LoadIndirect || instruction.op == IrOpcode::StoreIndirect ||
        instruction.op == IrOpcode::FileReadByte) {
      return true;
    }
  }
  return false;
}

// The VM's local count: one past the highest local an instruction loads, stores or addresses.
[[maybe_unused]] inline uint32_t vmLocalCount(const IrFunction &function) {
  uint64_t count = 0;
  for (const IrInstruction &instruction : function.instructions) {
    if (instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal ||
        instruction.op == IrOpcode::AddressOfLocal) {
      count = std::max<uint64_t>(count, instruction.imm + 1);
    }
  }
  return static_cast<uint32_t>(count);
}

// Whether the module allocates heap memory, so the emitted program needs the heap runtime.
inline bool allocatesHeap(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (instruction.op == IrOpcode::HeapAlloc || instruction.op == IrOpcode::HeapRealloc ||
          instruction.op == IrOpcode::HeapAllocBytes) {
        return true;
      }
    }
  }
  return false;
}

} // namespace primec::native_emitter

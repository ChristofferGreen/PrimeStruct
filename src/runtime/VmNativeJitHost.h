#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmHeapCore.h"
#include "primec/runtime/VmStringHeap.h"

namespace primec::vm_detail {

// The runtime side of the native JIT (primec/backend/NativeJit.h). The code calls `bridge` for
// the opcodes it has no machine code for (nativeJitBridgesOpcode in
// src/native_emitter/NativeJitOpcodes.h), which runs them with the VM's own handlers and
// messages: the heap, prints of argv and dynamic strings, files, dynamic string bytes and the
// float conversions the code leaves out (to i32 and u64, and u64 to f32).
//
// The heap is the VM's (VmHeapCore); the code reads its slot values and states directly, and the
// host republishes where they are after every allocation change.
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
  bool run(uint32_t functionIndex, const IrInstruction &inst, uint64_t *frameLocals);
  void publishHeap();

  const IrModule &module_;
  const std::vector<std::string_view> &args_;
  std::vector<size_t> localCounts_;
  VmHeapCore heap_;
  VmStringHeap strings_;
  std::vector<uint64_t> stack_;
  uint64_t *data_ = nullptr;
  std::string error_;
};

} // namespace primec::vm_detail

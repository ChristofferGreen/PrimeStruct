#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

namespace primec::vm_detail {

enum class VmControlFlowOpcodeResult { NotHandled, Continue, Call, Return, Exit, Fault };

struct VmControlFlowOpcodeOutcome {
  VmControlFlowOpcodeResult result = VmControlFlowOpcodeResult::NotHandled;
  size_t targetFunctionIndex = 0;
  bool returnValueToCaller = false;
  uint64_t returnValue = 0;
};

VmControlFlowOpcodeOutcome handleSharedVmControlFlowOpcode(const IrInstruction &inst,
                                                           std::vector<uint64_t> &stack,
                                                           size_t instructionCount,
                                                           size_t functionCount,
                                                           size_t frameCount,
                                                           size_t maxCallDepth,
                                                           bool currentFrameReturnsValueToCaller,
                                                           size_t &ip,
                                                           std::string &error);

} // namespace primec::vm_detail

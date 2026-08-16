#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::vm_debug_detail {

enum class OpcodeBlockResult { NotHandled, Continue, Fault };

OpcodeBlockResult handleVmDebugNumericOpcode(const IrInstruction &inst,
                                             std::vector<uint64_t> &stack,
                                             std::string &error);

} // namespace primec::vm_debug_detail

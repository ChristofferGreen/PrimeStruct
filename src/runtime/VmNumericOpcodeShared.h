#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

namespace primec::vm_detail {

enum class VmNumericOpcodeResult { NotHandled, Continue, Fault };

VmNumericOpcodeResult handleSharedVmNumericOpcode(const IrInstruction &inst,
                                                  std::vector<uint64_t> &stack,
                                                  std::string &error);

} // namespace primec::vm_detail

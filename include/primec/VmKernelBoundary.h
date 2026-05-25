#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/Ir.h"

namespace primec::vm_kernel {

enum class PureOpcodeResult {
  NotHandled,
  Continue,
  Fault,
};

std::string_view pureOpcodeResultName(PureOpcodeResult result);
bool isPureNumericOpcode(IrOpcode op);
PureOpcodeResult executePureNumericOpcode(const IrInstruction &inst,
                                          std::vector<std::uint64_t> &stack,
                                          std::string &error);

} // namespace primec::vm_kernel

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/IrVirtualRegisterLowering.h"

namespace primec {

struct IrVirtualRegisterLiveRange {
  uint32_t startPosition = 0;
  uint32_t endPosition = 0;
};

struct IrVirtualRegisterInterval {
  uint32_t virtualRegister = 0;
  std::vector<IrVirtualRegisterLiveRange> ranges;
};

struct IrVirtualRegisterBlockLiveness {
  std::vector<uint32_t> liveInRegisters;
  std::vector<uint32_t> liveOutRegisters;
};

struct IrVirtualRegisterFunctionLiveness {
  std::string functionName;
  std::vector<IrVirtualRegisterBlockLiveness> blocks;
  std::vector<IrVirtualRegisterInterval> intervals;
};

struct IrVirtualRegisterModuleLiveness {
  std::vector<IrVirtualRegisterFunctionLiveness> functions;
};

bool buildIrVirtualRegisterLiveness(const IrVirtualRegisterModule &module,
                                    IrVirtualRegisterModuleLiveness &out,
                                    std::string &error);

} // namespace primec

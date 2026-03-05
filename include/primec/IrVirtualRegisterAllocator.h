#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/IrVirtualRegisterLiveness.h"

namespace primec {

enum class IrLinearScanSpillPolicy : uint8_t {
  SpillFarthestEnd = 0,
};

struct IrLinearScanAllocatorOptions {
  uint32_t physicalRegisterCount = 0;
  IrLinearScanSpillPolicy spillPolicy = IrLinearScanSpillPolicy::SpillFarthestEnd;
};

struct IrLinearScanRegisterAssignment {
  uint32_t virtualRegister = 0;
  uint32_t startPosition = 0;
  uint32_t endPosition = 0;
  bool spilled = false;
  uint32_t physicalRegister = 0;
  uint32_t spillSlot = 0;
};

struct IrLinearScanFunctionAllocation {
  std::string functionName;
  std::vector<IrLinearScanRegisterAssignment> assignments;
  uint32_t spillSlotCount = 0;
};

struct IrLinearScanModuleAllocation {
  std::vector<IrLinearScanFunctionAllocation> functions;
};

bool allocateIrVirtualRegistersLinearScan(const IrVirtualRegisterModuleLiveness &liveness,
                                          const IrLinearScanAllocatorOptions &options,
                                          IrLinearScanModuleAllocation &out,
                                          std::string &error);

} // namespace primec

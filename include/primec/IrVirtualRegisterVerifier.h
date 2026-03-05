#pragma once

#include <string>

#include "primec/IrVirtualRegisterAllocator.h"
#include "primec/IrVirtualRegisterLowering.h"
#include "primec/IrVirtualRegisterScheduler.h"

namespace primec {

bool verifyIrVirtualRegisterScheduleAndAllocation(const IrVirtualRegisterModule &module,
                                                  const IrLinearScanModuleAllocation &allocation,
                                                  const IrVirtualRegisterScheduledModule &scheduled,
                                                  std::string &error);

} // namespace primec

#pragma once

#include <string>

#include "primec/ir/IrVirtualRegisterAllocator.h"
#include "primec/ir/IrVirtualRegisterLowering.h"
#include "primec/ir/IrVirtualRegisterScheduler.h"

namespace primec {

bool verifyIrVirtualRegisterScheduleAndAllocation(const IrVirtualRegisterModule &module,
                                                  const IrLinearScanModuleAllocation &allocation,
                                                  const IrVirtualRegisterScheduledModule &scheduled,
                                                  std::string &error);

} // namespace primec

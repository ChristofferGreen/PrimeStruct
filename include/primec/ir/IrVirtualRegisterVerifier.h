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

// Checks the promoted-locals form (IrVirtualRegisterLoweringOptions::promoteLocals): local uses and
// defs only on LoadLocal/StoreLocal of promoted slots, every use reads a register defined in its
// block or at its entry, every register is defined once, edge moves match the successor's entry
// locals and the predecessor's exit locals, and no promoted local is read before it is written on
// some path (the entry block has no entry locals). Functions without promoted locals pass.
bool verifyIrVirtualRegisterLocalForm(const IrVirtualRegisterModule &module, std::string &error);

} // namespace primec

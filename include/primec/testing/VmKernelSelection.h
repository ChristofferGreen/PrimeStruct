#pragma once

#include "primec/ir/Ir.h"

namespace primec::testing {

// Turns the VM's flat execution loop on or off for the whole process (default
// on). With it off, plain runs use the step kernel that debug sessions share, so
// a test can run one module both ways and compare results and fault messages.
void setVmFastKernelEnabled(bool enabled);

// Whether the flat loop would run `module` (every function passes the shared CFG
// analysis and returns leave the caller's stack balanced); otherwise plain runs
// fall back to the step kernel.
bool vmFastKernelAccepts(const IrModule &module);

} // namespace primec::testing

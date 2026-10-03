#pragma once

#include <cstdint>
#include <string>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

// Flat execution loop for plain runs (docs/OptimizingBackendsPlan.md, Phase 4).
// It never replaces the step kernel that debug sessions share: it only runs a
// module whose every function passes the shared CFG analysis (consistent stack
// depths, valid jump targets, returns that leave the caller's stack balanced,
// an entry function without parameters), because those facts let the loop
// drop the per-instruction underflow and target checks. `executed` is false
// when the module is not eligible; nothing has run in that case and the caller
// falls back to the step kernel. Results, faults and fault messages are
// identical to the step kernel.
bool executeVmFastKernel(const IrModule &module,
                         VmKernelHost &host,
                         uint64_t &result,
                         std::string &error,
                         bool &executed);

// False when tests turned the fast kernel off (primec/testing/VmKernelSelection.h).
bool vmFastKernelEnabled();

} // namespace primec::vm_detail

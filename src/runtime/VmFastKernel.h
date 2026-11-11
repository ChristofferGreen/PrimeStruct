#pragma once

#include <cstdint>
#include <string>

#include "VmFastKernelProgram.h"
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

// The loop itself. It runs `task` (VmFastKernelProgram.h) until its root function returns, a trap ends the program
// or, with `forTasks`, it reaches an opcode of the task scheduler, where it hands the call stack back with
// `task.ip` at that opcode. Never inlined or cloned: its dispatch table holds the addresses of one body.
#if defined(__clang__)
#define VM_FAST_LOOP_ATTRIBUTES [[gnu::noinline]]
#elif defined(__GNUC__)
#define VM_FAST_LOOP_ATTRIBUTES [[gnu::noinline, gnu::noclone]]
#else
#define VM_FAST_LOOP_ATTRIBUTES
#endif

VM_FAST_LOOP_ATTRIBUTES bool runVmFastTask(const IrModule &module,
                                           VmKernelHost &host,
                                           const std::vector<FastFunction> &functions,
                                           VmFastTask &task,
                                           bool forTasks,
                                           uint64_t &result,
                                           VmFastRunOutcome &outcome,
                                           std::string &error);

// Counts a run with tasks that the scheduler gave to the fast loop (testing::vmFastTaskRunCount).
void noteVmFastTaskRun();

// False when tests turned the fast kernel off (primec/testing/VmKernelSelection.h).
bool vmFastKernelEnabled();

} // namespace primec::vm_detail

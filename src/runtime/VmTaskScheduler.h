#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

// Whether the module holds a TaskSpawn or a TaskWait (docs/spec/vm-design.md, Tasks).
bool moduleUsesTasks(const IrModule &module);

// Runs a module with tasks under the VM's task scheduler (src/runtime/VmTaskScheduler.cpp): every task on the flat
// loop between the scheduler's opcodes when the loop takes the module, else on the step kernel.
bool runVmTaskScheduler(const IrModule &module,
                        VmKernelHost &host,
                        uint64_t &result,
                        std::string &error);

// One step of the step kernel without the event bookkeeping debug sessions need.
VmKernelStepOutcome stepVmKernelUntracked(const IrModule &module,
                                          VmKernelHost &host,
                                          std::vector<uint64_t> &stack,
                                          std::vector<VmKernelFrame> &frames,
                                          const std::vector<size_t> &localCounts,
                                          uint64_t &result,
                                          VmKernelStepEvent &event,
                                          std::string &error);

} // namespace primec::vm_detail

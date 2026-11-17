#pragma once

#include <cstdint>
#include <string>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

// Whether the module holds an atomic, mutex or condition opcode (docs/spec/vm-design.md, Atomics, Mutexes,
// Conditions). The threads mode of the VM does not run them yet.
bool moduleUsesSyncPrimitives(const IrModule &module);

// Runs a module with tasks with each task on an OS thread of its own (--vm-threads; docs/spec/vm-design.md, Tasks on
// threads). The host must answer runsTasksOnThreads(). The calling thread runs the entry function. A fault or a trap
// of any task stops every thread; the run returns once they all ended, with the first fault in `error` (false) or
// the first trap's or the entry function's value in `result` (true).
bool runVmThreadedTasks(const IrModule &module,
                        VmKernelHost &host,
                        uint64_t &result,
                        std::string &error);

} // namespace primec::vm_detail

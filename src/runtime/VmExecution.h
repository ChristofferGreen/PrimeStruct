#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmStringHeap.h"
#include "primec/runtime/VmHost.h"

namespace primec::vm_detail {

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args,
                     const VmHostFunctions *hostFunctions = nullptr);

// Executes IrOpcode::CallHost against `hostFunctions` (null = nothing bound):
// pops the import's parameters, invokes the binding and pushes its result.
bool handleVmHostCall(const VmHostFunctions *hostFunctions,
                      const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error,
                      VmStringHeap *heap = nullptr);

} // namespace primec::vm_detail

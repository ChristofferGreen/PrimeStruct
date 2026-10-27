#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmStringLookup.h"
#include "primec/runtime/VmTextHeap.h"
#include "primec/runtime/VmHost.h"

namespace primec::vm_detail {

// The VM heap and string heap a host call needs to answer the `__pstext_*` functions itself.
struct TextHeapContext {
  VmHeapCore *core = nullptr;
  VmTextHeap *heap = nullptr;
  // The entry arguments (null when the run has none), for `__pstext_argument`.
  const std::vector<std::string_view> *args = nullptr;
};

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args,
                     const VmHostFunctions *hostFunctions = nullptr,
                     const VmOutputSink *outputSink = nullptr,
                     const std::atomic<bool> *cancel = nullptr);

// Executes IrOpcode::CallHost against `hostFunctions` (null = nothing bound):
// pops the import's parameters, invokes the binding and pushes its result.
bool handleVmHostCall(const VmHostFunctions *hostFunctions,
                      const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error,
                      TextHeapContext *text = nullptr);

} // namespace primec::vm_detail

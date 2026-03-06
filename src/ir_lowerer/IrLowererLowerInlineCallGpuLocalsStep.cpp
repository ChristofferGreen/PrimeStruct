#include "IrLowererLowerInlineCallGpuLocalsStep.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

bool runLowerInlineCallGpuLocalsStep(const LowerInlineCallGpuLocalsStepInput &input,
                                     std::string &errorOut) {
  if (input.callerLocals == nullptr) {
    errorOut = "native backend missing inline-call gpu-locals step dependency: callerLocals";
    return false;
  }
  if (input.calleeLocals == nullptr) {
    errorOut = "native backend missing inline-call gpu-locals step dependency: calleeLocals";
    return false;
  }

  auto addGpuLocal = [&](const char *name) {
    auto it = input.callerLocals->find(name);
    if (it != input.callerLocals->end()) {
      input.calleeLocals->emplace(name, it->second);
    }
  };
  addGpuLocal(kGpuGlobalIdXName);
  addGpuLocal(kGpuGlobalIdYName);
  addGpuLocal(kGpuGlobalIdZName);
  return true;
}

} // namespace primec::ir_lowerer

#include "IrLowererLowerInlineCallCleanupStep.h"

namespace primec::ir_lowerer {

bool runLowerInlineCallCleanupStep(const LowerInlineCallCleanupStepInput &input,
                                   std::string &errorOut) {
  if (input.function == nullptr) {
    errorOut = "native backend missing inline-call cleanup step dependency: function";
    return false;
  }
  if (input.returnJumps == nullptr) {
    errorOut = "native backend missing inline-call cleanup step dependency: returnJumps";
    return false;
  }
  if (!input.emitCurrentFileScopeCleanup) {
    errorOut = "native backend missing inline-call cleanup step dependency: emitCurrentFileScopeCleanup";
    return false;
  }
  if (!input.popFileScope) {
    errorOut = "native backend missing inline-call cleanup step dependency: popFileScope";
    return false;
  }

  const size_t cleanupIndex = input.function->instructions.size();
  input.emitCurrentFileScopeCleanup();
  input.popFileScope();
  for (size_t jumpIndex : *input.returnJumps) {
    input.function->instructions[jumpIndex].imm = static_cast<int32_t>(cleanupIndex);
  }
  return true;
}

} // namespace primec::ir_lowerer

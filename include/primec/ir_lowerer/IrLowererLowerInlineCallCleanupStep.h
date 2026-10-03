#pragma once

#include <functional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct LowerInlineCallCleanupStepInput {
  IrFunction *function = nullptr;
  const std::vector<size_t> *returnJumps = nullptr;
  ActionFn emitCurrentFileScopeCleanup;
  ActionFn popFileScope;
};

bool runLowerInlineCallCleanupStep(const LowerInlineCallCleanupStepInput &input,
                                   std::string &errorOut);

} // namespace primec::ir_lowerer

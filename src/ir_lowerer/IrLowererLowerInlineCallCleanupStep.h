#pragma once

#include <functional>
#include <string>
#include <vector>

#include "IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerInlineCallCleanupStepInput {
  IrFunction *function = nullptr;
  const std::vector<size_t> *returnJumps = nullptr;
  std::function<void()> emitCurrentFileScopeCleanup;
  std::function<void()> popFileScope;
};

bool runLowerInlineCallCleanupStep(const LowerInlineCallCleanupStepInput &input,
                                   std::string &errorOut);

} // namespace primec::ir_lowerer

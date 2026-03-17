#pragma once

#include <functional>
#include <string>

#include "IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerInlineCallActiveContextStepInput {
  const Definition *callee = nullptr;
  bool structDefinition = false;
  bool definitionReturnsVoid = false;
  std::function<void()> activateInlineContext;
  std::function<void()> restoreInlineContext;
  std::function<bool(const Expr &)> emitInlineStatement;
  std::function<bool()> runInlineCleanup;
};

bool runLowerInlineCallActiveContextStep(const LowerInlineCallActiveContextStepInput &input,
                                         std::string &errorOut);

} // namespace primec::ir_lowerer

#pragma once

#include <functional>
#include <string>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct LowerInlineCallActiveContextStepInput {
  const Definition *callee = nullptr;
  bool structDefinition = false;
  bool definitionReturnsVoid = false;
  ActionFn activateInlineContext;
  ActionFn restoreInlineContext;
  ExprPredicateFn emitInlineStatement;
  std::function<bool()> runInlineCleanup;
};

bool runLowerInlineCallActiveContextStep(const LowerInlineCallActiveContextStepInput &input,
                                         std::string &errorOut);

} // namespace primec::ir_lowerer

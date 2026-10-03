#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct LowerStatementsEntryStatementStepInput {
  IrFunction *function = nullptr;
  ExprPredicateFn emitStatement;
  std::function<void(const std::string &, const Expr &, size_t, size_t)> appendInstructionSourceRange;
};

bool runLowerStatementsEntryStatementStep(const LowerStatementsEntryStatementStepInput &input,
                                          const Expr &stmt,
                                          std::string &errorOut);

} // namespace primec::ir_lowerer

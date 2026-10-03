#pragma once

#include <functional>
#include <string>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {

using ExprStringFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchBodyStatementStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

EmitterExprControlIfBranchBodyStatementStepResult
runEmitterExprControlIfBranchBodyStatementStep(
    const Expr &stmt,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

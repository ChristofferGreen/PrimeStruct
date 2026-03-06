#pragma once

#include <functional>
#include <string>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBranchBodyStatementEmitExprFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchBodyStatementStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

EmitterExprControlIfBranchBodyStatementStepResult
runEmitterExprControlIfBranchBodyStatementStep(
    const Expr &stmt,
    const EmitterExprControlIfBranchBodyStatementEmitExprFn &emitExpr);

} // namespace primec::emitter

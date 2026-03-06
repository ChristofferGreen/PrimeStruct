#pragma once

#include <functional>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBranchBodyDispatchReturnFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;
using EmitterExprControlIfBranchBodyDispatchBindingFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &)>;
using EmitterExprControlIfBranchBodyDispatchStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &)>;

struct EmitterExprControlIfBranchBodyDispatchStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

EmitterExprControlIfBranchBodyDispatchStepResult
runEmitterExprControlIfBranchBodyDispatchStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBranchBodyDispatchReturnFn &emitReturn,
    const EmitterExprControlIfBranchBodyDispatchBindingFn &emitBinding,
    const EmitterExprControlIfBranchBodyDispatchStatementFn &emitStatement);

} // namespace primec::emitter

#pragma once

#include <functional>
#include <string>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBranchBodyReturnIsReturnCallFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyReturnEmitExprFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchBodyReturnStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

EmitterExprControlIfBranchBodyReturnStepResult
runEmitterExprControlIfBranchBodyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBranchBodyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchBodyReturnEmitExprFn &emitExpr);

} // namespace primec::emitter

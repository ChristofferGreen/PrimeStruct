#pragma once

#include <functional>
#include <string>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {

using ExprPredicateFn =
    std::function<bool(const Expr &)>;
using ExprStringFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchBodyReturnStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

EmitterExprControlIfBranchBodyReturnStepResult
runEmitterExprControlIfBranchBodyReturnStep(
    const Expr &stmt,
    bool isLast,
    const ExprPredicateFn &isReturnCall,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

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
using EmitterExprControlIfBranchValueEmitStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;

struct EmitterExprControlIfBranchValueStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfBranchValueStepResult runEmitterExprControlIfBranchValueStep(
    const Expr &candidate,
    const ExprPredicateFn &isIfBlockEnvelope,
    const ExprStringFn &emitExpr,
    const EmitterExprControlIfBranchValueEmitStatementFn &emitStatement);

} // namespace primec::emitter

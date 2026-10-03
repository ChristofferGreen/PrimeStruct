#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlIfBranchPreludeStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfBranchPreludeStepResult runEmitterExprControlIfBranchPreludeStep(
    const Expr &candidate,
    const ExprPredicateFn &isBlockEnvelope,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

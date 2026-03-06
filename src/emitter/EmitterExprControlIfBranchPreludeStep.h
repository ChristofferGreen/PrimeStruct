#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBranchPreludeIsBlockEnvelopeFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchPreludeEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchPreludeStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfBranchPreludeStepResult runEmitterExprControlIfBranchPreludeStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchPreludeIsBlockEnvelopeFn &isBlockEnvelope,
    const EmitterExprControlIfBranchPreludeEmitExprFn &emitExpr);

} // namespace primec::emitter

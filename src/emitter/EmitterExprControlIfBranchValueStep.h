#pragma once

#include <functional>
#include <string>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBranchValueIsEnvelopeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchValueEmitExprFn =
    std::function<std::string(const Expr &)>;
using EmitterExprControlIfBranchValueEmitStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;

struct EmitterExprControlIfBranchValueStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfBranchValueStepResult runEmitterExprControlIfBranchValueStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchValueIsEnvelopeFn &isIfBlockEnvelope,
    const EmitterExprControlIfBranchValueEmitExprFn &emitExpr,
    const EmitterExprControlIfBranchValueEmitStatementFn &emitStatement);

} // namespace primec::emitter

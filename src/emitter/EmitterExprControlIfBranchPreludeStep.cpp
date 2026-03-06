#include "EmitterExprControlIfBranchPreludeStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchPreludeStepResult runEmitterExprControlIfBranchPreludeStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchPreludeIsBlockEnvelopeFn &isBlockEnvelope,
    const EmitterExprControlIfBranchPreludeEmitExprFn &emitExpr) {
  if (!isBlockEnvelope || !emitExpr) {
    return {};
  }

  EmitterExprControlIfBranchPreludeStepResult result;
  if (!isBlockEnvelope(candidate)) {
    result.handled = true;
    result.emittedExpr = emitExpr(candidate);
    return result;
  }

  if (candidate.bodyArguments.empty()) {
    result.handled = true;
    result.emittedExpr = "0";
    return result;
  }

  return result;
}

} // namespace primec::emitter

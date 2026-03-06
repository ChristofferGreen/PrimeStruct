#include "EmitterExprControlIfBranchBodyReturnStep.h"

#include "EmitterExprControlIfBlockEarlyReturnStep.h"
#include "EmitterExprControlIfBlockFinalValueStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchBodyReturnStepResult
runEmitterExprControlIfBranchBodyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBranchBodyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchBodyReturnEmitExprFn &emitExpr) {
  if (!isReturnCall || !emitExpr) {
    return {};
  }

  if (const auto earlyReturnStep =
          runEmitterExprControlIfBlockEarlyReturnStep(
              stmt, isLast, isReturnCall, emitExpr);
      earlyReturnStep.handled) {
    EmitterExprControlIfBranchBodyReturnStepResult result;
    result.handled = true;
    result.emitted.handled = true;
    result.emitted.emittedStatement = earlyReturnStep.emittedStatement;
    result.emitted.shouldBreak = true;
    return result;
  }

  if (const auto finalValueStep = runEmitterExprControlIfBlockFinalValueStep(
          stmt, isLast, isReturnCall, emitExpr);
      finalValueStep.handled) {
    EmitterExprControlIfBranchBodyReturnStepResult result;
    result.handled = true;
    result.emitted.handled = true;
    result.emitted.emittedStatement = finalValueStep.emittedStatement;
    result.emitted.shouldBreak = true;
    return result;
  }

  return {};
}

} // namespace primec::emitter

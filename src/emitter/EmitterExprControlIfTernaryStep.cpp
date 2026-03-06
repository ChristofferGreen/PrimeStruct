#include "EmitterExprControlIfTernaryStep.h"

namespace primec::emitter {

EmitterExprControlIfTernaryStepResult runEmitterExprControlIfTernaryStep(
    const EmitterExprControlIfTernaryEmitConditionFn &emitCondition,
    const EmitterExprControlIfTernaryEmitThenFn &emitThen,
    const EmitterExprControlIfTernaryEmitElseFn &emitElse) {
  if (!emitCondition || !emitThen || !emitElse) {
    return {};
  }

  EmitterExprControlIfTernaryStepResult result;
  result.handled = true;
  result.emittedExpr = "(" + emitCondition() + " ? " + emitThen() + " : " + emitElse() + ")";
  return result;
}

} // namespace primec::emitter

#include "EmitterExprControlIfTernaryFallbackStep.h"

#include <sstream>

namespace primec::emitter {

EmitterExprControlIfTernaryFallbackStepResult runEmitterExprControlIfTernaryFallbackStep(
    const EmitterExprControlIfTernaryFallbackEmitConditionFn &emitCondition,
    const EmitterExprControlIfTernaryFallbackEmitThenFn &emitThen,
    const EmitterExprControlIfTernaryFallbackEmitElseFn &emitElse) {
  if (!emitCondition || !emitThen || !emitElse) {
    return {};
  }

  EmitterExprControlIfTernaryFallbackStepResult result;
  result.handled = true;

  std::ostringstream out;
  out << "(" << emitCondition() << " ? " << emitThen() << " : " << emitElse() << ")";
  result.emittedExpr = out.str();
  return result;
}

} // namespace primec::emitter

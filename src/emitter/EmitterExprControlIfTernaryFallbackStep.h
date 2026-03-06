#pragma once

#include <functional>
#include <string>

namespace primec::emitter {

using EmitterExprControlIfTernaryFallbackEmitConditionFn = std::function<std::string()>;
using EmitterExprControlIfTernaryFallbackEmitThenFn = std::function<std::string()>;
using EmitterExprControlIfTernaryFallbackEmitElseFn = std::function<std::string()>;

struct EmitterExprControlIfTernaryFallbackStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfTernaryFallbackStepResult runEmitterExprControlIfTernaryFallbackStep(
    const EmitterExprControlIfTernaryFallbackEmitConditionFn &emitCondition,
    const EmitterExprControlIfTernaryFallbackEmitThenFn &emitThen,
    const EmitterExprControlIfTernaryFallbackEmitElseFn &emitElse);

} // namespace primec::emitter

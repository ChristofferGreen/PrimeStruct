#pragma once

#include <functional>
#include <string>

namespace primec::emitter {

using EmitterExprControlIfTernaryEmitConditionFn = std::function<std::string()>;
using EmitterExprControlIfTernaryEmitThenFn = std::function<std::string()>;
using EmitterExprControlIfTernaryEmitElseFn = std::function<std::string()>;

struct EmitterExprControlIfTernaryStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfTernaryStepResult runEmitterExprControlIfTernaryStep(
    const EmitterExprControlIfTernaryEmitConditionFn &emitCondition,
    const EmitterExprControlIfTernaryEmitThenFn &emitThen,
    const EmitterExprControlIfTernaryEmitElseFn &emitElse);

} // namespace primec::emitter

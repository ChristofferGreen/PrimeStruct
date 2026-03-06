#pragma once

#include <functional>
#include <string>

namespace primec::emitter {

using EmitterExprControlIfBranchWrapperEmitBodyFn = std::function<std::string()>;

struct EmitterExprControlIfBranchWrapperStepResult {
  bool handled = false;
  std::string emittedExpr;
};

EmitterExprControlIfBranchWrapperStepResult runEmitterExprControlIfBranchWrapperStep(
    const EmitterExprControlIfBranchWrapperEmitBodyFn &emitBody);

} // namespace primec::emitter

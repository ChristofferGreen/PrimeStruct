#include "EmitterExprControlIfBranchWrapperStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchWrapperStepResult runEmitterExprControlIfBranchWrapperStep(
    const EmitterExprControlIfBranchWrapperEmitBodyFn &emitBody) {
  if (!emitBody) {
    return {};
  }

  EmitterExprControlIfBranchWrapperStepResult result;
  result.handled = true;
  result.emittedExpr = "([&]() { " + emitBody() + "}())";
  return result;
}

} // namespace primec::emitter

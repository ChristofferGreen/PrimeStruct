#include "EmitterExprControlIfBranchValueStep.h"

#include "EmitterExprControlIfBranchPreludeStep.h"
#include "EmitterExprControlIfBranchWrapperStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchValueStepResult runEmitterExprControlIfBranchValueStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchValueIsEnvelopeFn &isIfBlockEnvelope,
    const EmitterExprControlIfBranchValueEmitExprFn &emitExpr,
    const EmitterExprControlIfBranchValueEmitStatementFn &emitStatement) {
  if (!isIfBlockEnvelope || !emitExpr || !emitStatement) {
    return {};
  }

  if (const auto branchPreludeStep = runEmitterExprControlIfBranchPreludeStep(
          candidate,
          isIfBlockEnvelope,
          emitExpr);
      branchPreludeStep.handled) {
    EmitterExprControlIfBranchValueStepResult result;
    result.handled = true;
    result.emittedExpr = branchPreludeStep.emittedExpr;
    return result;
  }

  const auto branchBodyStep = runEmitterExprControlIfBranchBodyStep(
      candidate, emitStatement);
  const std::string emittedBranchBody =
      branchBodyStep.handled ? branchBodyStep.emittedBody : std::string{};
  if (const auto branchWrapperStep = runEmitterExprControlIfBranchWrapperStep(
          [&]() { return emittedBranchBody; });
      branchWrapperStep.handled) {
    EmitterExprControlIfBranchValueStepResult result;
    result.handled = true;
    result.emittedExpr = branchWrapperStep.emittedExpr;
    return result;
  }

  EmitterExprControlIfBranchValueStepResult result;
  result.handled = true;
  result.emittedExpr = "([&]() { " + emittedBranchBody + "}())";
  return result;
}

} // namespace primec::emitter

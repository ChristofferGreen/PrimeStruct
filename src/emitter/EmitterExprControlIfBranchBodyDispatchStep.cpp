#include "EmitterExprControlIfBranchBodyDispatchStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchBodyDispatchStepResult
runEmitterExprControlIfBranchBodyDispatchStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBranchBodyDispatchReturnFn &emitReturn,
    const EmitterExprControlIfBranchBodyDispatchBindingFn &emitBinding,
    const EmitterExprControlIfBranchBodyDispatchStatementFn &emitStatement) {
  if (!emitReturn && !emitBinding && !emitStatement) {
    return {};
  }

  if (emitReturn) {
    const auto emitted = emitReturn(stmt, isLast);
    if (emitted.handled) {
      EmitterExprControlIfBranchBodyDispatchStepResult result;
      result.handled = true;
      result.emitted = emitted;
      return result;
    }
  }

  if (emitBinding) {
    const auto emitted = emitBinding(stmt);
    if (emitted.handled) {
      EmitterExprControlIfBranchBodyDispatchStepResult result;
      result.handled = true;
      result.emitted = emitted;
      return result;
    }
  }

  if (emitStatement) {
    const auto emitted = emitStatement(stmt);
    if (emitted.handled) {
      EmitterExprControlIfBranchBodyDispatchStepResult result;
      result.handled = true;
      result.emitted = emitted;
      return result;
    }
  }

  return {};
}

} // namespace primec::emitter

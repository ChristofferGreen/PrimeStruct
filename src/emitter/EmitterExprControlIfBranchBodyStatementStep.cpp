#include "EmitterExprControlIfBranchBodyStatementStep.h"

#include "EmitterExprControlIfBlockStatementStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchBodyStatementStepResult
runEmitterExprControlIfBranchBodyStatementStep(
    const Expr &stmt,
    const EmitterExprControlIfBranchBodyStatementEmitExprFn &emitExpr) {
  if (const auto statementStep = runEmitterExprControlIfBlockStatementStep(
          stmt, emitExpr);
      statementStep.handled) {
    EmitterExprControlIfBranchBodyStatementStepResult result;
    result.handled = true;
    result.emitted.handled = true;
    result.emitted.emittedStatement = statementStep.emittedStatement;
    result.emitted.shouldBreak = false;
    return result;
  }

  return {};
}

} // namespace primec::emitter

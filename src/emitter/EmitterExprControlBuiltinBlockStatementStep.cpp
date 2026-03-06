#include "EmitterExprControlBuiltinBlockStatementStep.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockStatementStepResult runEmitterExprControlBuiltinBlockStatementStep(
    const Expr &stmt,
    const EmitterExprControlBuiltinBlockStatementEmitExprFn &emitExpr) {
  if (!emitExpr) {
    return {};
  }

  EmitterExprControlBuiltinBlockStatementStepResult result;
  result.handled = true;
  result.emittedStatement = "(void)" + emitExpr(stmt) + "; ";
  return result;
}

} // namespace primec::emitter

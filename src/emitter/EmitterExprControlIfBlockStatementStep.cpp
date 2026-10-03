#include "EmitterExprControlIfBlockStatementStep.h"

namespace primec::emitter {

EmitterExprControlIfBlockStatementStepResult runEmitterExprControlIfBlockStatementStep(
    const Expr &stmt,
    const ExprStringFn &emitExpr) {
  if (!emitExpr) {
    return {};
  }

  EmitterExprControlIfBlockStatementStepResult result;
  result.handled = true;
  result.emittedStatement = "(void)" + emitExpr(stmt) + "; ";
  return result;
}

} // namespace primec::emitter

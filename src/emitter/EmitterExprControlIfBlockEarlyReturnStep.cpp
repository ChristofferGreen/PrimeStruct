#include "EmitterExprControlIfBlockEarlyReturnStep.h"

namespace primec::emitter {

EmitterExprControlIfBlockEarlyReturnStepResult runEmitterExprControlIfBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBlockEarlyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBlockEarlyReturnEmitExprFn &emitExpr) {
  if (isLast || !isReturnCall || !isReturnCall(stmt)) {
    return {};
  }

  EmitterExprControlIfBlockEarlyReturnStepResult result;
  result.handled = true;
  if (stmt.args.size() == 1 && emitExpr) {
    result.emittedStatement = "return " + emitExpr(stmt.args.front()) + "; ";
  } else {
    result.emittedStatement = "return 0; ";
  }
  return result;
}

} // namespace primec::emitter

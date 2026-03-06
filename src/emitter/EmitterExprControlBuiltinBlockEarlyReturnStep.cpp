#include "EmitterExprControlBuiltinBlockEarlyReturnStep.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockEarlyReturnStepResult runEmitterExprControlBuiltinBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlBuiltinBlockEarlyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlBuiltinBlockEarlyReturnEmitExprFn &emitExpr) {
  if (isLast || !isReturnCall || !isReturnCall(stmt)) {
    return {};
  }

  EmitterExprControlBuiltinBlockEarlyReturnStepResult result;
  result.handled = true;
  if (stmt.args.size() == 1 && emitExpr) {
    result.emittedStatement = "return " + emitExpr(stmt.args.front()) + "; ";
  } else {
    result.emittedStatement = "return 0; ";
  }
  return result;
}

} // namespace primec::emitter

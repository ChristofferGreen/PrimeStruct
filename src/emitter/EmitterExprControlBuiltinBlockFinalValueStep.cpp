#include "EmitterExprControlBuiltinBlockFinalValueStep.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockFinalValueStepResult runEmitterExprControlBuiltinBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlBuiltinBlockFinalValueIsReturnCallFn &isReturnCall,
    const EmitterExprControlBuiltinBlockFinalValueEmitExprFn &emitExpr) {
  if (!isLast) {
    return {};
  }

  EmitterExprControlBuiltinBlockFinalValueStepResult result;
  result.handled = true;
  if (stmt.isBinding) {
    result.emittedStatement = "return 0; ";
    return result;
  }

  const Expr *valueExpr = &stmt;
  if (isReturnCall && isReturnCall(stmt)) {
    if (stmt.args.size() == 1) {
      valueExpr = &stmt.args.front();
    } else {
      result.emittedStatement = "return 0; ";
      return result;
    }
  }
  if (!emitExpr) {
    result.emittedStatement = "return 0; ";
    return result;
  }
  result.emittedStatement = "return " + emitExpr(*valueExpr) + "; ";
  return result;
}

} // namespace primec::emitter

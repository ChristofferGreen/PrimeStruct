#include "IrLowererLowerExprEmitSetup.h"

namespace primec::ir_lowerer {

bool runLowerExprEmitSetup(const LowerExprEmitSetupInput &,
                           LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCallOut,
                           std::string &errorOut) {
  emitMovePassthroughCallOut = [](const Expr &expr,
                                  const LocalMap &localsIn,
                                  const EmitExprWithLocalsFn &emitExpr,
                                  std::string &emitErrorOut) -> UnaryPassthroughCallResult {
    if (!emitExpr) {
      emitErrorOut = "native backend missing expr emit setup dependency: emitExpr";
      return UnaryPassthroughCallResult::Error;
    }
    if (expr.isMethodCall) {
      return UnaryPassthroughCallResult::NotMatched;
    }
    return ir_lowerer::tryEmitUnaryPassthroughCall(
        expr,
        "move",
        [&](const Expr &argExpr) { return emitExpr(argExpr, localsIn); },
        emitErrorOut);
  };
  errorOut.clear();
  return true;
}

} // namespace primec::ir_lowerer

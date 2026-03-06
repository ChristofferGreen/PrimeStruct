#include "IrLowererLowerExprEmitSetup.h"

namespace primec::ir_lowerer {

bool runLowerExprEmitSetup(const LowerExprEmitSetupInput &,
                           LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCallOut,
                           LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCallOut,
                           LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCallOut,
                           std::string &errorOut) {
  const auto makeUnaryPassthroughCall = [](const char *callName,
                                           bool rejectMethodCalls) -> LowerExprEmitUnaryPassthroughCallFn {
    return [callName, rejectMethodCalls](const Expr &expr,
                                         const LocalMap &localsIn,
                                         const EmitExprWithLocalsFn &emitExpr,
                                         std::string &emitErrorOut) -> UnaryPassthroughCallResult {
      if (!emitExpr) {
        emitErrorOut = "native backend missing expr emit setup dependency: emitExpr";
        return UnaryPassthroughCallResult::Error;
      }
      if (rejectMethodCalls && expr.isMethodCall) {
        return UnaryPassthroughCallResult::NotMatched;
      }
      return ir_lowerer::tryEmitUnaryPassthroughCall(
          expr,
          callName,
          [&](const Expr &argExpr) { return emitExpr(argExpr, localsIn); },
          emitErrorOut);
    };
  };
  emitMovePassthroughCallOut = makeUnaryPassthroughCall("move", true);
  emitUploadPassthroughCallOut = makeUnaryPassthroughCall("upload", false);
  emitReadbackPassthroughCallOut = makeUnaryPassthroughCall("readback", false);
  errorOut.clear();
  return true;
}

} // namespace primec::ir_lowerer

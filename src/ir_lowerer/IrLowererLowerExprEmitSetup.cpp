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

UnaryPassthroughCallResult runLowerExprEmitUploadReadbackPassthroughStep(
    const Expr &expr,
    const LocalMap &localsIn,
    const LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCall,
    const LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCall,
    const EmitExprWithLocalsFn &emitExpr,
    std::string &errorOut) {
  if (!emitUploadPassthroughCall) {
    errorOut = "native backend missing expr emit setup dependency: emitUploadPassthroughCall";
    return UnaryPassthroughCallResult::Error;
  }
  if (!emitReadbackPassthroughCall) {
    errorOut = "native backend missing expr emit setup dependency: emitReadbackPassthroughCall";
    return UnaryPassthroughCallResult::Error;
  }
  if (!emitExpr) {
    errorOut = "native backend missing expr emit setup dependency: emitExpr";
    return UnaryPassthroughCallResult::Error;
  }
  const auto uploadResult = emitUploadPassthroughCall(expr, localsIn, emitExpr, errorOut);
  if (uploadResult != UnaryPassthroughCallResult::NotMatched) {
    return uploadResult;
  }
  return emitReadbackPassthroughCall(expr, localsIn, emitExpr, errorOut);
}

} // namespace primec::ir_lowerer

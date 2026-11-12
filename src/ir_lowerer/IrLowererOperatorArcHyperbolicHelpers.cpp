#include "primec/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.h"
#include "IrLowererOperatorExpLogHyperbolicHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

OperatorArcHyperbolicEmitResult
emitArcHyperbolicOperatorExpr(const Expr &expr,
                              const LocalMap &localsIn,
                              bool hasMathImport,
                              const ExprLocalsPredicateFn &emitExpr,
                              const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
                              const Int32ProviderFn &allocTempLocal,
                              std::vector<IrInstruction> &instructions,
                              std::string &error) {
  MathEmitter fe(instructions);
  std::string arcName;
  if (getBuiltinArcTrigName(expr, arcName, hasMathImport)) {
    LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
    if (!checkUnaryFloatArgument(expr, arcName, inferExprKind, localsIn, argKind, error)) {
      return OperatorArcHyperbolicEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
      fe.load(leftLocal).load(rightLocal).mul().store(outLocal);
    };

    int32_t tempX = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorArcHyperbolicEmitResult::Error;
    }
    fe.store(tempX);

    if (arcName == "atan") {
      auto emitAtanSeriesForLocal = [&](int32_t inputLocal, int32_t outLocal) {
        int32_t tempX2 = allocTempLocal();
        int32_t tempX3 = allocTempLocal();
        int32_t tempX5 = allocTempLocal();
        int32_t tempX7 = allocTempLocal();
        emitMulLocal(inputLocal, inputLocal, tempX2);
        emitMulLocal(tempX2, inputLocal, tempX3);
        emitMulLocal(tempX3, tempX2, tempX5);
        emitMulLocal(tempX5, tempX2, tempX7);

        fe.load(inputLocal)
            .load(tempX3)
            .push(0.3333333333333333)
            .mul()
            .sub()
            .load(tempX5)
            .push(0.2)
            .mul()
            .add()
            .load(tempX7)
            .push(0.14285714285714285)
            .mul()
            .sub()
            .store(outLocal);
      };

      int32_t tempAbs = allocTempLocal();
      int32_t tempSign = allocTempLocal();
      int32_t tempInv = allocTempLocal();
      int32_t tempOut = allocTempLocal();

      fe.load(tempX).push(0.0).cmpLt();
      size_t jumpIfNotNeg = fe.jumpIfZero();

      fe.load(tempX).neg().store(tempAbs).push(-1.0).store(tempSign);
      size_t jumpAfterSign = fe.jump();

      fe.patch(jumpIfNotNeg);
      fe.load(tempX).store(tempAbs).push(1.0).store(tempSign);
      fe.patch(jumpAfterSign);

      fe.load(tempAbs).push(1.0).cmpLt();
      size_t jumpIfNotSmall = fe.jumpIfZero();

      emitAtanSeriesForLocal(tempAbs, tempOut);
      size_t jumpAfterAtan = fe.jump();

      fe.patch(jumpIfNotSmall);
      fe.push(1.0).load(tempAbs).div().store(tempInv);
      emitAtanSeriesForLocal(tempInv, tempOut);
      fe.push(1.5707963267948966).load(tempOut).sub().store(tempOut);

      fe.patch(jumpAfterAtan);
      fe.load(tempOut).load(tempSign).mul();
      return OperatorArcHyperbolicEmitResult::Handled;
    }

    int32_t tempX2 = allocTempLocal();
    int32_t tempX3 = allocTempLocal();
    int32_t tempX5 = allocTempLocal();
    int32_t tempX7 = allocTempLocal();
    emitMulLocal(tempX, tempX, tempX2);
    emitMulLocal(tempX2, tempX, tempX3);
    emitMulLocal(tempX3, tempX2, tempX5);
    emitMulLocal(tempX5, tempX2, tempX7);

    fe.load(tempX)
        .load(tempX3)
        .push(0.16666666666666666)
        .mul()
        .add()
        .load(tempX5)
        .push(0.075)
        .mul()
        .add()
        .load(tempX7)
        .push(0.044642857142857144)
        .mul()
        .add();

    if (arcName == "asin") {
      return OperatorArcHyperbolicEmitResult::Handled;
    }

    int32_t tempAsin = allocTempLocal();
    fe.store(tempAsin).push(1.5707963267948966).load(tempAsin).sub();
    return OperatorArcHyperbolicEmitResult::Handled;
  }

  if (OperatorArcHyperbolicEmitResult result = emitExpOperatorExpr(expr,
                                                                   localsIn,
                                                                   hasMathImport,
                                                                   emitExpr,
                                                                   inferExprKind,
                                                                   allocTempLocal,
                                                                   instructions,
                                                                   error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  if (OperatorArcHyperbolicEmitResult result = emitLogOperatorExpr(expr,
                                                                   localsIn,
                                                                   hasMathImport,
                                                                   emitExpr,
                                                                   inferExprKind,
                                                                   allocTempLocal,
                                                                   instructions,
                                                                   error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  if (OperatorArcHyperbolicEmitResult result = emitHyperbolicOperatorExpr(expr,
                                                                          localsIn,
                                                                          hasMathImport,
                                                                          emitExpr,
                                                                          inferExprKind,
                                                                          allocTempLocal,
                                                                          instructions,
                                                                          error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  if (OperatorArcHyperbolicEmitResult result = emitArcHyperbolicBuiltinExpr(expr,
                                                                            localsIn,
                                                                            hasMathImport,
                                                                            emitExpr,
                                                                            inferExprKind,
                                                                            allocTempLocal,
                                                                            instructions,
                                                                            error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  return OperatorArcHyperbolicEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

#include "IrLowererOperatorAngleTrigHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

OperatorClampMinMaxTrigEmitResult
emitAngleTrigOperatorExpr(const Expr &expr,
                          const LocalMap &localsIn,
                          bool hasMathImport,
                          const ExprLocalsPredicateFn &emitExpr,
                          const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
                          const Int32ProviderFn &allocTempLocal,
                          std::vector<IrInstruction> &instructions,
                          std::string &error) {
  MathEmitter fe(instructions);
  std::string angleName;
  if (getBuiltinAngleName(expr, angleName, hasMathImport)) {
    LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
    if (!checkUnaryFloatArgument(expr, angleName, inferExprKind, localsIn, argKind, error)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    if (angleName == "radians") {
      fe.push(0.017453292519943295);
    } else {
      fe.push(57.29577951308232);
    }
    fe.mul();
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }

  std::string trigName;
  if (getBuiltinTrigName(expr, trigName, hasMathImport)) {
    LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
    if (!checkUnaryFloatArgument(expr, trigName, inferExprKind, localsIn, argKind, error)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
      fe.load(leftLocal).load(rightLocal).mul().store(outLocal);
    };
    auto emitSquare = [&](int32_t valueLocal, int32_t outLocal) {
      emitMulLocal(valueLocal, valueLocal, outLocal);
    };

    auto emitFloatTruncToLocal = [&](int32_t valueLocal, int32_t outLocal) {
      fe.load(valueLocal);
      if (argKind == LocalInfo::ValueKind::Float32) {
        fe.op(IrOpcode::ConvertF32ToI32).op(IrOpcode::ConvertI32ToF32);
      } else {
        fe.op(IrOpcode::ConvertF64ToI64).op(IrOpcode::ConvertI64ToF64);
      }
      fe.store(outLocal);
    };

    auto emitFloorToLocal = [&](int32_t valueLocal, int32_t outLocal) {
      emitFloatTruncToLocal(valueLocal, outLocal);
      fe.useFloatKind(argKind);
      fe.load(outLocal).load(valueLocal).cmpGt();
      size_t useValue = fe.jumpIfZero();
      fe.load(outLocal).push(1.0).sub().store(outLocal);
      size_t jumpEnd = fe.jump();
      fe.patch(useValue);
      fe.patch(jumpEnd);
    };

    int32_t tempX = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempX);

    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTau = 6.28318530717958647692;
    constexpr double kHalfPi = 1.57079632679489661923;
    int32_t tempScale = allocTempLocal();
    int32_t tempFloor = allocTempLocal();
    int32_t tempMul = allocTempLocal();
    fe.useFloatKind(argKind);
    // Range-reduce x into [-pi, pi] to keep the series stable.
    fe.load(tempX).push(kTau).div().store(tempScale);
    emitFloorToLocal(tempScale, tempFloor);
    fe.load(tempFloor).push(kTau).mul().store(tempMul).load(tempX).load(tempMul).sub().store(tempX);

    fe.load(tempX).push(kPi).cmpGt();
    size_t skipWrap = fe.jumpIfZero();
    fe.load(tempX).push(kTau).sub().store(tempX);
    size_t jumpWrapEnd = fe.jump();
    fe.patch(skipWrap);
    fe.patch(jumpWrapEnd);

    int32_t tempSinSign = allocTempLocal();
    int32_t tempCosSign = allocTempLocal();
    fe.push(1.0).store(tempSinSign).push(1.0).store(tempCosSign);

    fe.load(tempX).push(kHalfPi).cmpGt();
    size_t skipUpper = fe.jumpIfZero();
    fe.push(kPi).load(tempX).sub().store(tempX).push(-1.0).store(tempCosSign);
    fe.patch(skipUpper);

    fe.load(tempX).push(-kHalfPi).cmpLt();
    size_t skipLower = fe.jumpIfZero();
    fe.load(tempX)
        .push(kPi)
        .add()
        .store(tempX)
        .push(-1.0)
        .store(tempCosSign)
        .push(-1.0)
        .store(tempSinSign);
    fe.patch(skipLower);

    int32_t tempX2 = allocTempLocal();
    int32_t tempX3 = allocTempLocal();
    int32_t tempX5 = allocTempLocal();
    int32_t tempX7 = allocTempLocal();
    emitSquare(tempX, tempX2);
    emitMulLocal(tempX2, tempX, tempX3);
    emitMulLocal(tempX3, tempX2, tempX5);
    emitMulLocal(tempX5, tempX2, tempX7);

    int32_t tempSin = allocTempLocal();
    fe.load(tempX)
        .load(tempX3)
        .push(0.16666666666666666)
        .mul()
        .sub()
        .load(tempX5)
        .push(0.008333333333333333)
        .mul()
        .add()
        .load(tempX7)
        .push(0.0001984126984126984)
        .mul()
        .sub()
        .load(tempSinSign)
        .mul()
        .store(tempSin);

    int32_t tempX4 = allocTempLocal();
    int32_t tempX6 = allocTempLocal();
    emitSquare(tempX2, tempX4);
    emitMulLocal(tempX4, tempX2, tempX6);

    int32_t tempCos = allocTempLocal();
    fe.push(1.0)
        .load(tempX2)
        .push(0.5)
        .mul()
        .sub()
        .load(tempX4)
        .push(0.041666666666666664)
        .mul()
        .add()
        .load(tempX6)
        .push(0.001388888888888889)
        .mul()
        .sub()
        .load(tempCosSign)
        .mul()
        .store(tempCos);

    if (trigName == "sin") {
      fe.load(tempSin);
      return OperatorClampMinMaxTrigEmitResult::Handled;
    }
    if (trigName == "cos") {
      fe.load(tempCos);
      return OperatorClampMinMaxTrigEmitResult::Handled;
    }

    fe.load(tempSin).load(tempCos).div();
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }

  std::string trig2Name;
  if (getBuiltinTrig2Name(expr, trig2Name, hasMathImport)) {
    if (expr.args.size() != 2) {
      error = trig2Name + " requires exactly two arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args[0], localsIn);
    LocalInfo::ValueKind argKind2 = inferExprKind(expr.args[1], localsIn);
    if ((argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) ||
        argKind != argKind2) {
      error = trig2Name + " requires float arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    int32_t tempY = allocTempLocal();
    int32_t tempX = allocTempLocal();
    int32_t tempZ = allocTempLocal();
    int32_t tempZ2 = allocTempLocal();
    int32_t tempZ3 = allocTempLocal();
    int32_t tempZ5 = allocTempLocal();
    int32_t tempOut = allocTempLocal();

    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempY);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempX);

    // Handle x == 0 axis cases.
    fe.load(tempX).push(0.0).cmpEq();
    size_t jumpIfNotZeroX = fe.jumpIfZero();

    fe.load(tempY).push(0.0).cmpEq();
    size_t jumpIfNotZeroY = fe.jumpIfZero();

    fe.push(0.0).store(tempOut);
    size_t jumpAxisEndZero = fe.jump();

    fe.patch(jumpIfNotZeroY);
    fe.load(tempY).push(0.0).cmpLt();
    size_t jumpIfPosY = fe.jumpIfZero();

    fe.push(-1.5707963267948966).store(tempOut);
    size_t jumpAxisEndNeg = fe.jump();

    fe.patch(jumpIfPosY);
    fe.push(1.5707963267948966).store(tempOut);
    size_t jumpAxisEndPos = fe.jump();

    fe.patch(jumpIfNotZeroX);

    // atan2(y, x) ≈ atan(y/x) with quadrant adjustment.
    fe.load(tempY).load(tempX).div().store(tempZ);

    auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
      fe.load(leftLocal).load(rightLocal).mul().store(outLocal);
    };

    emitMulLocal(tempZ, tempZ, tempZ2);
    emitMulLocal(tempZ2, tempZ, tempZ3);
    emitMulLocal(tempZ3, tempZ2, tempZ5);

    fe.load(tempZ).load(tempZ3).push(3.0).div().sub().load(tempZ5).push(5.0).div().add().store(
        tempOut);

    fe.load(tempX).push(0.0).cmpLt();
    size_t jumpIfNotNegX = fe.jumpIfZero();

    fe.load(tempY).push(0.0).cmpLt();
    size_t jumpIfPosYAdj = fe.jumpIfZero();

    fe.load(tempOut).push(-3.14159265358979323846).add().store(tempOut);
    size_t jumpAdjustEnd = fe.jump();

    fe.patch(jumpIfPosYAdj);
    fe.load(tempOut).push(3.14159265358979323846).add().store(tempOut);
    fe.patch(jumpAdjustEnd);

    fe.patch(jumpIfNotNegX);

    fe.patch(jumpAxisEndZero);
    fe.patch(jumpAxisEndNeg);
    fe.patch(jumpAxisEndPos);

    fe.load(tempOut);
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }

  return OperatorClampMinMaxTrigEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

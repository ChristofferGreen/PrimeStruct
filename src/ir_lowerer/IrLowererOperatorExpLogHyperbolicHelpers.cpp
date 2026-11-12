#include "IrLowererOperatorExpLogHyperbolicHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

OperatorArcHyperbolicEmitResult
emitExpOperatorExpr(const Expr &expr,
                    const LocalMap &localsIn,
                    bool hasMathImport,
                    const ExprLocalsPredicateFn &emitExpr,
                    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
                    const Int32ProviderFn &allocTempLocal,
                    std::vector<IrInstruction> &instructions,
                    std::string &error) {
  MathEmitter fe(instructions);
  std::string expName;
  if (!getBuiltinExpName(expr, expName, hasMathImport)) {
    return OperatorArcHyperbolicEmitResult::NotHandled;
  }
  LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
  if (!checkUnaryFloatArgument(expr, expName, inferExprKind, localsIn, argKind, error)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.useFloatKind(argKind);
  int32_t tempX = allocTempLocal();
  int32_t tempTerm = allocTempLocal();
  int32_t tempSum = allocTempLocal();
  int32_t tempIter = allocTempLocal();

  if (!emitExpr(expr.args.front(), localsIn)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.store(tempX);

  if (expName == "exp2") {
    fe.load(tempX).push(0.69314718055994530942).mul().store(tempX);
  }

  fe.push(1.0).store(tempTerm).push(1.0).store(tempSum).pushI32(1).store(tempIter);

  const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
  const size_t loopStart = fe.here();
  fe.load(tempIter).pushI32(static_cast<uint64_t>(iterations + 1)).op(IrOpcode::CmpLtI32);
  size_t jumpEndLoop = fe.jumpIfZero();

  fe.load(tempTerm).load(tempX).mul().store(tempTerm);

  fe.load(tempTerm).load(tempIter).fromI32().div().store(tempTerm);

  fe.load(tempSum).load(tempTerm).add().store(tempSum);

  fe.load(tempIter).pushI32(1).op(IrOpcode::AddI32).store(tempIter).jumpTo(loopStart);

  fe.patch(jumpEndLoop);
  fe.load(tempSum);
  return OperatorArcHyperbolicEmitResult::Handled;
}

OperatorArcHyperbolicEmitResult
emitLogOperatorExpr(const Expr &expr,
                    const LocalMap &localsIn,
                    bool hasMathImport,
                    const ExprLocalsPredicateFn &emitExpr,
                    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
                    const Int32ProviderFn &allocTempLocal,
                    std::vector<IrInstruction> &instructions,
                    std::string &error) {
  MathEmitter fe(instructions);
  std::string logName;
  if (!getBuiltinLogName(expr, logName, hasMathImport)) {
    return OperatorArcHyperbolicEmitResult::NotHandled;
  }
  LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
  if (!checkUnaryFloatArgument(expr, logName, inferExprKind, localsIn, argKind, error)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.useFloatKind(argKind);
  int32_t tempX = allocTempLocal();
  int32_t tempZ = allocTempLocal();
  int32_t tempZ2 = allocTempLocal();
  int32_t tempTerm = allocTempLocal();
  int32_t tempSum = allocTempLocal();
  int32_t tempOut = allocTempLocal();
  int32_t tempK = allocTempLocal();

  if (!emitExpr(expr.args.front(), localsIn)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.store(tempX);

  fe.load(tempX).push(0.0).cmpLt();
  size_t jumpIfNotNegative = fe.jumpIfZero();
  fe.push(0.0).push(0.0).div().store(tempOut);
  size_t jumpToEnd = fe.jump();

  fe.patch(jumpIfNotNegative);

  fe.load(tempX).push(0.0).cmpEq();
  size_t jumpIfNotZero = fe.jumpIfZero();
  fe.push(-1.0).push(0.0).div().store(tempOut);
  size_t jumpToEndZero = fe.jump();

  fe.patch(jumpIfNotZero);

  fe.push(0.0).store(tempK);

  const size_t reduceHighStart = fe.here();
  fe.push(2.0).load(tempX).cmpLt();
  size_t jumpAfterHigh = fe.jumpIfZero();
  fe.load(tempX).push(2.0).div().store(tempX).load(tempK).push(1.0).add().store(tempK).jumpTo(
      reduceHighStart);
  fe.patch(jumpAfterHigh);

  const size_t reduceLowStart = fe.here();
  fe.load(tempX).push(0.5).cmpLt();
  size_t jumpAfterLow = fe.jumpIfZero();
  fe.load(tempX).push(2.0).mul().store(tempX).load(tempK).push(1.0).sub().store(tempK).jumpTo(
      reduceLowStart);
  fe.patch(jumpAfterLow);

  fe.load(tempX).push(1.0).sub().load(tempX).push(1.0).add().div().store(tempZ);

  fe.load(tempZ).load(tempZ).mul().store(tempZ2);

  fe.load(tempZ).store(tempTerm).load(tempZ).store(tempSum);

  auto appendSeries = [&](double divisor) {
    fe.load(tempTerm)
        .load(tempZ2)
        .mul()
        .store(tempTerm)
        .load(tempSum)
        .load(tempTerm)
        .push(1.0 / divisor)
        .mul()
        .add()
        .store(tempSum);
  };

  appendSeries(3.0);
  appendSeries(5.0);
  appendSeries(7.0);
  appendSeries(9.0);

  fe.load(tempSum).push(2.0).mul().store(tempOut);

  fe.load(tempOut).load(tempK).push(0.69314718055994530942).mul().add().store(tempOut);

  if (logName == "log2") {
    fe.load(tempOut).push(0.69314718055994530942).div().store(tempOut);
  } else if (logName == "log10") {
    fe.load(tempOut).push(2.30258509299404568402).div().store(tempOut);
  }

  fe.patch(jumpToEnd);
  fe.patch(jumpToEndZero);
  fe.load(tempOut);
  return OperatorArcHyperbolicEmitResult::Handled;
}

OperatorArcHyperbolicEmitResult
emitHyperbolicOperatorExpr(const Expr &expr,
                           const LocalMap &localsIn,
                           bool hasMathImport,
                           const ExprLocalsPredicateFn &emitExpr,
                           const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
                           const Int32ProviderFn &allocTempLocal,
                           std::vector<IrInstruction> &instructions,
                           std::string &error) {
  MathEmitter fe(instructions);
  std::string hyperName;
  if (!getBuiltinHyperbolicName(expr, hyperName, hasMathImport)) {
    return OperatorArcHyperbolicEmitResult::NotHandled;
  }
  LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
  if (!checkUnaryFloatArgument(expr, hyperName, inferExprKind, localsIn, argKind, error)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.useFloatKind(argKind);
  int32_t tempX = allocTempLocal();
  int32_t tempNegX = allocTempLocal();
  int32_t tempExpPos = allocTempLocal();
  int32_t tempExpNeg = allocTempLocal();

  if (!emitExpr(expr.args.front(), localsIn)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.store(tempX);

  fe.load(tempX).neg().store(tempNegX);

  emitExpSeries(fe, allocTempLocal, tempX, tempExpPos);
  emitExpSeries(fe, allocTempLocal, tempNegX, tempExpNeg);

  if (hyperName == "sinh") {
    fe.load(tempExpPos).load(tempExpNeg).sub().push(2.0).div();
    return OperatorArcHyperbolicEmitResult::Handled;
  }

  if (hyperName == "cosh") {
    fe.load(tempExpPos).load(tempExpNeg).add().push(2.0).div();
    return OperatorArcHyperbolicEmitResult::Handled;
  }

  int32_t tempNumerator = allocTempLocal();
  int32_t tempDenominator = allocTempLocal();
  fe.load(tempExpPos)
      .load(tempExpNeg)
      .sub()
      .store(tempNumerator)
      .load(tempExpPos)
      .load(tempExpNeg)
      .add()
      .store(tempDenominator)
      .load(tempNumerator)
      .load(tempDenominator)
      .div();
  return OperatorArcHyperbolicEmitResult::Handled;
}

OperatorArcHyperbolicEmitResult
emitArcHyperbolicBuiltinExpr(const Expr &expr,
                             const LocalMap &localsIn,
                             bool hasMathImport,
                             const ExprLocalsPredicateFn &emitExpr,
                             const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
                             const Int32ProviderFn &allocTempLocal,
                             std::vector<IrInstruction> &instructions,
                             std::string &error) {
  MathEmitter fe(instructions);
  std::string arcHyperName;
  if (!getBuiltinArcHyperbolicName(expr, arcHyperName, hasMathImport)) {
    return OperatorArcHyperbolicEmitResult::NotHandled;
  }
  LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
  if (!checkUnaryFloatArgument(expr, arcHyperName, inferExprKind, localsIn, argKind, error)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.useFloatKind(argKind);
  int32_t tempX = allocTempLocal();
  if (!emitExpr(expr.args.front(), localsIn)) {
    return OperatorArcHyperbolicEmitResult::Error;
  }
  fe.store(tempX);

  int32_t tempOut = allocTempLocal();
  if (arcHyperName == "atanh") {
    int32_t tempOnePlus = allocTempLocal();
    int32_t tempOneMinus = allocTempLocal();
    int32_t tempRatio = allocTempLocal();

    fe.load(tempX).push(1.0).add().store(tempOnePlus);

    fe.push(1.0).load(tempX).sub().store(tempOneMinus);

    fe.load(tempOnePlus).load(tempOneMinus).div().store(tempRatio);

    emitLogSeries(fe, allocTempLocal, tempRatio, tempOut);
    fe.load(tempOut).push(0.5).mul();
    return OperatorArcHyperbolicEmitResult::Handled;
  }

  int32_t tempX2 = allocTempLocal();
  int32_t tempInner = allocTempLocal();
  int32_t tempSqrt = allocTempLocal();
  int32_t tempSum = allocTempLocal();

  fe.load(tempX).load(tempX).mul().store(tempX2);

  fe.load(tempX2).push(1.0);
  instructions.push_back({arcHyperName == "asinh" ? fe.ops.add : fe.ops.sub, 0});
  fe.store(tempInner);

  emitSqrtSeries(fe, allocTempLocal, tempInner, tempSqrt);

  fe.load(tempX).load(tempSqrt).add().store(tempSum);

  emitLogSeries(fe, allocTempLocal, tempSum, tempOut);
  fe.load(tempOut);
  return OperatorArcHyperbolicEmitResult::Handled;
}

} // namespace primec::ir_lowerer

#include "primec/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "IrLowererOperatorAngleTrigHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

OperatorClampMinMaxTrigEmitResult
emitClampMinMaxTrigOperatorExpr(const Expr &expr,
                                const LocalMap &localsIn,
                                bool hasMathImport,
                                const ExprLocalsPredicateFn &emitExpr,
                                const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
                                const CombineClampMinMaxTrigNumericKindsFn &combineNumericKinds,
                                const Int32ProviderFn &allocTempLocal,
                                std::vector<IrInstruction> &instructions,
                                std::string &error) {
  MathEmitter fe(instructions);
  if (getBuiltinClampName(expr, hasMathImport)) {
    if (expr.args.size() != 3) {
      error = "clamp requires exactly three arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (argsMixSignedAndUnsigned(expr.args, inferExprKind, localsIn)) {
      error = "clamp requires numeric arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind clampKind =
        combineNumericKinds(combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                inferExprKind(expr.args[1], localsIn)),
                            inferExprKind(expr.args[2], localsIn));
    if (clampKind == LocalInfo::ValueKind::Unknown) {
      error = "clamp requires numeric arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    IrOpcode cmpLt = IrOpcode::CmpLtI32;
    IrOpcode cmpGt = IrOpcode::CmpGtI32;
    if ((clampKind == LocalInfo::ValueKind::UInt64 || clampKind == LocalInfo::ValueKind::UInt8)) {
      cmpLt = IrOpcode::CmpLtU64;
      cmpGt = IrOpcode::CmpGtU64;
    } else if (clampKind == LocalInfo::ValueKind::Int64) {
      cmpLt = IrOpcode::CmpLtI64;
      cmpGt = IrOpcode::CmpGtI64;
    } else if (clampKind == LocalInfo::ValueKind::Float64) {
      cmpLt = IrOpcode::CmpLtF64;
      cmpGt = IrOpcode::CmpGtF64;
    } else if (clampKind == LocalInfo::ValueKind::Float32) {
      cmpLt = IrOpcode::CmpLtF32;
      cmpGt = IrOpcode::CmpGtF32;
    }
    int32_t tempValue = allocTempLocal();
    int32_t tempMin = allocTempLocal();
    int32_t tempMax = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempValue);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempMin);
    if (!emitExpr(expr.args[2], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempMax);

    fe.load(tempValue).load(tempMin).op(cmpLt);
    size_t skipMin = fe.jumpIfZero();
    fe.load(tempMin).store(tempOut);
    size_t jumpToEnd = fe.jump();
    fe.patch(skipMin);

    fe.load(tempValue).load(tempMax).op(cmpGt);
    size_t skipMax = fe.jumpIfZero();
    fe.load(tempMax).store(tempOut);
    size_t jumpToEnd2 = fe.jump();
    fe.patch(skipMax);
    fe.load(tempValue).store(tempOut);
    fe.patch(jumpToEnd);
    fe.patch(jumpToEnd2);
    fe.load(tempOut);
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }
  std::string minMaxName;
  if (getBuiltinMinMaxName(expr, minMaxName, hasMathImport)) {
    if (expr.args.size() != 2) {
      error = minMaxName + " requires exactly two arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (argsMixSignedAndUnsigned(expr.args, inferExprKind, localsIn)) {
      error = minMaxName + " requires numeric arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind minMaxKind = combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                          inferExprKind(expr.args[1], localsIn));
    if (minMaxKind == LocalInfo::ValueKind::Unknown) {
      error = minMaxName + " requires numeric arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    IrOpcode cmpOp = IrOpcode::CmpLtI32;
    if (minMaxName == "max") {
      cmpOp = IrOpcode::CmpGtI32;
    }
    if ((minMaxKind == LocalInfo::ValueKind::UInt64 || minMaxKind == LocalInfo::ValueKind::UInt8)) {
      cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtU64 : IrOpcode::CmpLtU64;
    } else if (minMaxKind == LocalInfo::ValueKind::Int64) {
      cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtI64 : IrOpcode::CmpLtI64;
    } else if (minMaxKind == LocalInfo::ValueKind::Float64) {
      cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtF64 : IrOpcode::CmpLtF64;
    } else if (minMaxKind == LocalInfo::ValueKind::Float32) {
      cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtF32 : IrOpcode::CmpLtF32;
    }
    int32_t tempLeft = allocTempLocal();
    int32_t tempRight = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempLeft);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempRight);

    fe.load(tempLeft).load(tempRight).op(cmpOp);
    size_t useRight = fe.jumpIfZero();
    fe.load(tempLeft).store(tempOut);
    size_t jumpToEnd = fe.jump();
    fe.patch(useRight);
    fe.load(tempRight).store(tempOut);
    fe.patch(jumpToEnd);
    fe.load(tempOut);
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }
  std::string lerpName;
  if (getBuiltinLerpName(expr, lerpName, hasMathImport)) {
    if (expr.args.size() != 3) {
      error = lerpName + " requires exactly three arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (argsMixSignedAndUnsigned(expr.args, inferExprKind, localsIn)) {
      error = lerpName + " requires numeric arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind lerpKind =
        combineNumericKinds(combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                inferExprKind(expr.args[1], localsIn)),
                            inferExprKind(expr.args[2], localsIn));
    if (lerpKind == LocalInfo::ValueKind::Unknown) {
      error = lerpName + " requires numeric arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (lerpKind == LocalInfo::ValueKind::UInt8) {
      error = lerpName + " does not support u8 operands; convert to u64 first";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    IrOpcode addOp = IrOpcode::AddI32;
    IrOpcode subOp = IrOpcode::SubI32;
    IrOpcode mulOp = IrOpcode::MulI32;
    if (lerpKind == LocalInfo::ValueKind::Int64 ||
        (lerpKind == LocalInfo::ValueKind::UInt64 || lerpKind == LocalInfo::ValueKind::UInt8)) {
      addOp = IrOpcode::AddI64;
      subOp = IrOpcode::SubI64;
      mulOp = IrOpcode::MulI64;
    } else if (lerpKind == LocalInfo::ValueKind::Float64) {
      addOp = IrOpcode::AddF64;
      subOp = IrOpcode::SubF64;
      mulOp = IrOpcode::MulF64;
    } else if (lerpKind == LocalInfo::ValueKind::Float32) {
      addOp = IrOpcode::AddF32;
      subOp = IrOpcode::SubF32;
      mulOp = IrOpcode::MulF32;
    }
    int32_t tempStart = allocTempLocal();
    int32_t tempEnd = allocTempLocal();
    int32_t tempT = allocTempLocal();
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempStart);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempEnd);
    if (!emitExpr(expr.args[2], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempT);

    fe.load(tempEnd).load(tempStart).op(subOp).load(tempT).op(mulOp).load(tempStart).op(addOp);
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }
  std::string fmaName;
  if (getBuiltinFmaName(expr, fmaName, hasMathImport)) {
    if (expr.args.size() != 3) {
      error = fmaName + " requires exactly three arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind aKind = inferExprKind(expr.args[0], localsIn);
    LocalInfo::ValueKind bKind = inferExprKind(expr.args[1], localsIn);
    LocalInfo::ValueKind cKind = inferExprKind(expr.args[2], localsIn);
    auto isFloatKind = [](LocalInfo::ValueKind kind) {
      return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
    };
    if (!isFloatKind(aKind) || !isFloatKind(bKind) || !isFloatKind(cKind)) {
      error = fmaName + " requires float arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (aKind != bKind || aKind != cKind) {
      error = fmaName + " requires float arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.useFloatKind(aKind);
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.mul();
    if (!emitExpr(expr.args[2], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.add();
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }
  std::string hypotName;
  if (getBuiltinHypotName(expr, hypotName, hasMathImport)) {
    if (expr.args.size() != 2) {
      error = hypotName + " requires exactly two arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind aKind = inferExprKind(expr.args[0], localsIn);
    LocalInfo::ValueKind bKind = inferExprKind(expr.args[1], localsIn);
    auto isFloatKind = [](LocalInfo::ValueKind kind) {
      return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
    };
    if (!isFloatKind(aKind) || !isFloatKind(bKind)) {
      error = hypotName + " requires float arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (aKind != bKind) {
      error = hypotName + " requires float arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    int32_t tempA = allocTempLocal();
    int32_t tempB = allocTempLocal();
    int32_t tempValue = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    int32_t tempIter = allocTempLocal();
    int32_t tempX = allocTempLocal();

    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempA);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempB);

    fe.useFloatKind(aKind);
    fe.load(tempA).load(tempA).mul().load(tempB).load(tempB).mul().add().store(tempValue);

    fe.load(tempValue).push(0.0).cmpEq();
    size_t jumpIfNotZero = fe.jumpIfZero();
    fe.push(0.0).store(tempOut);
    size_t jumpToEndZero = fe.jump();

    fe.patch(jumpIfNotZero);

    fe.load(tempValue).store(tempX).op(IrOpcode::PushI32).store(tempIter);

    const int32_t iterations = (aKind == LocalInfo::ValueKind::Float64) ? 8 : 6;
    const size_t loopStart = fe.here();
    fe.load(tempIter).pushI32(static_cast<uint64_t>(iterations)).op(IrOpcode::CmpLtI32);
    size_t jumpEndLoop = fe.jumpIfZero();

    fe.load(tempX).load(tempValue).load(tempX).div().add().push(0.5).mul().store(tempX);

    fe.load(tempIter).pushI32(1).op(IrOpcode::AddI32).store(tempIter).jumpTo(loopStart);

    fe.patch(jumpEndLoop);
    fe.load(tempX).store(tempOut);

    fe.patch(jumpToEndZero);
    fe.load(tempOut);
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }
  std::string copySignName;
  if (getBuiltinCopysignName(expr, copySignName, hasMathImport)) {
    if (expr.args.size() != 2) {
      error = copySignName + " requires exactly two arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind xKind = inferExprKind(expr.args[0], localsIn);
    LocalInfo::ValueKind yKind = inferExprKind(expr.args[1], localsIn);
    auto isFloatKind = [](LocalInfo::ValueKind kind) {
      return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
    };
    if (!isFloatKind(xKind) || !isFloatKind(yKind)) {
      error = copySignName + " requires float arguments";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (xKind != yKind) {
      error = copySignName + " requires float arguments of the same type";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    int32_t tempX = allocTempLocal();
    int32_t tempY = allocTempLocal();
    int32_t tempAbs = allocTempLocal();
    int32_t tempOut = allocTempLocal();

    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempX);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    fe.store(tempY);

    fe.useFloatKind(xKind);
    fe.load(tempX).push(0.0).cmpLt();
    size_t useNeg = fe.jumpIfZero();
    fe.load(tempX).neg().store(tempAbs);
    size_t jumpAbsEnd = fe.jump();

    fe.patch(useNeg);
    fe.load(tempX).store(tempAbs);

    fe.patch(jumpAbsEnd);

    fe.load(tempY).push(0.0).cmpLt();
    size_t usePositive = fe.jumpIfZero();
    fe.load(tempAbs).neg().store(tempOut);
    size_t jumpToEnd = fe.jump();

    fe.patch(usePositive);
    fe.load(tempAbs).store(tempOut);

    fe.patch(jumpToEnd);
    fe.load(tempOut);
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }
  return emitAngleTrigOperatorExpr(
      expr, localsIn, hasMathImport, emitExpr, inferExprKind, allocTempLocal, instructions, error);
}

} // namespace primec::ir_lowerer

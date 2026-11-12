#include "primec/ir_lowerer/IrLowererOperatorSaturateRoundingRootsHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

OperatorSaturateRoundingRootsEmitResult
emitSaturateRoundingRootsOperatorExpr(const Expr &expr,
                                      const LocalMap &localsIn,
                                      bool hasMathImport,
                                      const ExprLocalsPredicateFn &emitExpr,
                                      const ExprLocalsValueKindFn &inferExprKind,
                                      const Int32ProviderFn &allocTempLocal,
                                      std::vector<IrInstruction> &instructions,
                                      std::string &error) {
  MathEmitter fe(instructions);
  std::string saturateName;
  if (getBuiltinSaturateName(expr, saturateName, hasMathImport)) {
    if (expr.args.size() != 1) {
      error = saturateName + " requires exactly one argument";
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
    if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::Bool ||
        argKind == LocalInfo::ValueKind::String) {
      error = saturateName + " requires numeric argument";
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    int32_t tempValue = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    fe.store(tempValue);

    fe.useFloatKind(argKind);
    auto pushConst = [&](double value) {
      if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
        fe.push(value);
        return;
      }
      if (argKind == LocalInfo::ValueKind::Int32) {
        instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
      } else {
        instructions.push_back(
            {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
      }
    };

    if ((argKind == LocalInfo::ValueKind::UInt64 || argKind == LocalInfo::ValueKind::UInt8)) {
      fe.load(tempValue);
      pushConst(1);
      fe.op(IrOpcode::CmpGtU64);
      size_t useValue = fe.jumpIfZero();
      pushConst(1);
      fe.store(tempOut);
      size_t jumpToEnd = fe.jump();
      fe.patch(useValue);
      fe.load(tempValue).store(tempOut);
      fe.patch(jumpToEnd);
      fe.load(tempOut);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }

    IrOpcode cmpLt = IrOpcode::CmpLtI32;
    IrOpcode cmpGt = IrOpcode::CmpGtI32;
    if (argKind == LocalInfo::ValueKind::Int64) {
      cmpLt = IrOpcode::CmpLtI64;
      cmpGt = IrOpcode::CmpGtI64;
    } else if (argKind == LocalInfo::ValueKind::Float64) {
      cmpLt = IrOpcode::CmpLtF64;
      cmpGt = IrOpcode::CmpGtF64;
    } else if (argKind == LocalInfo::ValueKind::Float32) {
      cmpLt = IrOpcode::CmpLtF32;
      cmpGt = IrOpcode::CmpGtF32;
    }
    fe.load(tempValue);
    pushConst(0);
    fe.op(cmpLt);
    size_t checkMax = fe.jumpIfZero();
    pushConst(0);
    fe.store(tempOut);
    size_t jumpToEnd = fe.jump();
    fe.patch(checkMax);
    fe.load(tempValue);
    pushConst(1);
    fe.op(cmpGt);
    size_t useValue = fe.jumpIfZero();
    pushConst(1);
    fe.store(tempOut);
    size_t jumpToEnd2 = fe.jump();
    fe.patch(useValue);
    fe.load(tempValue).store(tempOut);
    fe.patch(jumpToEnd);
    fe.patch(jumpToEnd2);
    fe.load(tempOut);
    return OperatorSaturateRoundingRootsEmitResult::Handled;
  }
  std::string predicateName;
  if (getBuiltinMathPredicateName(expr, predicateName, hasMathImport)) {
    LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
    if (!checkUnaryFloatArgument(expr, predicateName, inferExprKind, localsIn, argKind, error)) {
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    int32_t tempValue = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    fe.store(tempValue);

    fe.useFloatKind(argKind);

    if (predicateName == "is_nan") {
      fe.load(tempValue).load(tempValue);
      fe.cmpNe();
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }

    if (predicateName == "is_finite") {
      fe.load(tempValue).load(tempValue).sub().push(0.0);
      fe.cmpEq();
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }

    fe.load(tempValue).load(tempValue);
    fe.cmpEq();
    fe.load(tempValue).load(tempValue).sub().push(0.0);
    fe.cmpNe();
    fe.op(IrOpcode::MulI32);
    return OperatorSaturateRoundingRootsEmitResult::Handled;
  }
  std::string roundingName;
  if (getBuiltinRoundingName(expr, roundingName, hasMathImport)) {
    if (expr.args.size() != 1) {
      error = roundingName + " requires exactly one argument";
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
    if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::Bool ||
        argKind == LocalInfo::ValueKind::String) {
      error = roundingName + " requires numeric argument";
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    int32_t tempValue = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    fe.store(tempValue);

    fe.useFloatKind(argKind);
    auto emitFloatTrunc = [&](int32_t valueLocal, int32_t outLocal) {
      fe.load(valueLocal);
      if (argKind == LocalInfo::ValueKind::Float32) {
        fe.op(IrOpcode::ConvertF32ToI32).op(IrOpcode::ConvertI32ToF32);
      } else {
        fe.op(IrOpcode::ConvertF64ToI64).op(IrOpcode::ConvertI64ToF64);
      }
      fe.store(outLocal);
    };
    auto emitFloorOrCeil = [&](int32_t valueLocal, int32_t outLocal, bool isCeil) {
      emitFloatTrunc(valueLocal, outLocal);
      IrOpcode cmpOp = isCeil ? fe.ops.cmpLt : fe.ops.cmpGt;
      IrOpcode addSubOp = isCeil ? fe.ops.add : fe.ops.sub;
      fe.load(outLocal).load(valueLocal).op(cmpOp);
      size_t useValue = fe.jumpIfZero();
      fe.load(outLocal).push(1.0).op(addSubOp).store(outLocal);
      size_t jumpEnd = fe.jump();
      fe.patch(useValue);
      fe.patch(jumpEnd);
    };

    if (argKind == LocalInfo::ValueKind::Int32 || argKind == LocalInfo::ValueKind::Int64 ||
        (argKind == LocalInfo::ValueKind::UInt64 || argKind == LocalInfo::ValueKind::UInt8)) {
      if (roundingName == "fract") {
        if (argKind == LocalInfo::ValueKind::Int32) {
          fe.op(IrOpcode::PushI32);
        } else {
          fe.op(IrOpcode::PushI64);
        }
        return OperatorSaturateRoundingRootsEmitResult::Handled;
      }
      fe.load(tempValue);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }

    if (roundingName == "trunc") {
      int32_t tempOut = allocTempLocal();
      emitFloatTrunc(tempValue, tempOut);
      fe.load(tempOut);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }

    int32_t tempOut = allocTempLocal();
    if (roundingName == "floor") {
      emitFloorOrCeil(tempValue, tempOut, false);
      fe.load(tempOut);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }
    if (roundingName == "ceil") {
      emitFloorOrCeil(tempValue, tempOut, true);
      fe.load(tempOut);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }
    if (roundingName == "fract") {
      emitFloorOrCeil(tempValue, tempOut, false);
      fe.load(tempValue).load(tempOut).sub();
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }
    if (roundingName == "round") {
      int32_t tempAdjusted = allocTempLocal();
      fe.load(tempValue).push(0.0).cmpLt();
      size_t jumpNonNegative = fe.jumpIfZero();

      fe.load(tempValue).push(0.5).sub().store(tempAdjusted);
      emitFloorOrCeil(tempAdjusted, tempOut, true);
      size_t jumpEnd = fe.jump();

      fe.patch(jumpNonNegative);
      fe.load(tempValue).push(0.5).add().store(tempAdjusted);
      emitFloorOrCeil(tempAdjusted, tempOut, false);

      fe.patch(jumpEnd);
      fe.load(tempOut);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }
    error = roundingName + " requires numeric argument";
    return OperatorSaturateRoundingRootsEmitResult::Error;
  }
  std::string rootName;
  if (getBuiltinRootName(expr, rootName, hasMathImport)) {
    LocalInfo::ValueKind argKind = LocalInfo::ValueKind::Unknown;
    if (!checkUnaryFloatArgument(expr, rootName, inferExprKind, localsIn, argKind, error)) {
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    fe.useFloatKind(argKind);
    int32_t tempValue = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    int32_t tempIter = allocTempLocal();
    int32_t tempX = allocTempLocal();

    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorSaturateRoundingRootsEmitResult::Error;
    }
    fe.store(tempValue);

    fe.useFloatKind(argKind);

    fe.load(tempValue).push(0.0).cmpEq();
    size_t jumpIfNotZero = fe.jumpIfZero();
    fe.push(0.0).store(tempOut);
    size_t jumpToEndZero = fe.jump();

    fe.patch(jumpIfNotZero);

    if (rootName == "sqrt") {
      fe.load(tempValue).push(0.0).cmpLt();
      size_t jumpIfNonNegative = fe.jumpIfZero();
      fe.push(0.0).push(0.0).div().store(tempOut);
      size_t jumpToEndNegative = fe.jump();

      fe.patch(jumpIfNonNegative);

      fe.load(tempValue).store(tempX).op(IrOpcode::PushI32).store(tempIter);

      const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 8 : 6;
      const size_t loopStart = fe.here();
      fe.load(tempIter).pushI32(static_cast<uint64_t>(iterations)).op(IrOpcode::CmpLtI32);
      size_t jumpEndLoop = fe.jumpIfZero();

      fe.load(tempX).load(tempValue).load(tempX).div().add().push(0.5).mul().store(tempX);

      fe.load(tempIter).pushI32(1).op(IrOpcode::AddI32).store(tempIter).jumpTo(loopStart);

      fe.patch(jumpEndLoop);
      fe.load(tempX).store(tempOut);

      fe.patch(jumpToEndZero);
      fe.patch(jumpToEndNegative);
      fe.load(tempOut);
      return OperatorSaturateRoundingRootsEmitResult::Handled;
    }

    fe.load(tempValue).store(tempX).op(IrOpcode::PushI32).store(tempIter);

    const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 10 : 8;
    const size_t loopStart = fe.here();
    fe.load(tempIter).pushI32(static_cast<uint64_t>(iterations)).op(IrOpcode::CmpLtI32);
    size_t jumpEndLoop = fe.jumpIfZero();

    fe.load(tempX).load(tempX).mul().store(tempOut);

    fe.load(tempX).push(2.0).mul().load(tempValue).load(tempOut).div().add().push(3.0).div().store(
        tempX);

    fe.load(tempIter).pushI32(1).op(IrOpcode::AddI32).store(tempIter).jumpTo(loopStart);

    fe.patch(jumpEndLoop);
    fe.load(tempX).store(tempOut);

    fe.patch(jumpToEndZero);
    fe.load(tempOut);
    return OperatorSaturateRoundingRootsEmitResult::Handled;
  }
  return OperatorSaturateRoundingRootsEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

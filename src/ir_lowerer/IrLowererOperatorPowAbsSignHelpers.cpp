#include "primec/ir_lowerer/IrLowererOperatorPowAbsSignHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

OperatorPowAbsSignEmitResult
emitPowAbsSignOperatorExpr(const Expr &expr,
                           const LocalMap &localsIn,
                           bool hasMathImport,
                           const ExprLocalsPredicateFn &emitExpr,
                           const ExprLocalsValueKindFn &inferExprKind,
                           const CombinePowAbsSignNumericKindsFn &combineNumericKinds,
                           const Int32ProviderFn &allocTempLocal,
                           const ActionFn &emitPowNegativeExponent,
                           std::vector<IrInstruction> &instructions,
                           std::string &error) {
  MathEmitter fe(instructions);
  std::string powName;
  if (getBuiltinPowName(expr, powName, hasMathImport)) {
    if (expr.args.size() != 2) {
      error = powName + " requires exactly two arguments";
      return OperatorPowAbsSignEmitResult::Error;
    }
    if (argsMixSignedAndUnsigned(expr.args, inferExprKind, localsIn)) {
      error = powName + " requires numeric arguments of the same type";
      return OperatorPowAbsSignEmitResult::Error;
    }
    LocalInfo::ValueKind powKind = combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                       inferExprKind(expr.args[1], localsIn));
    if (powKind == LocalInfo::ValueKind::UInt8) {
      error = powName + " does not support u8 operands; convert to u64 first";
      return OperatorPowAbsSignEmitResult::Error;
    }
    if (powKind == LocalInfo::ValueKind::Unknown || powKind == LocalInfo::ValueKind::Bool ||
        powKind == LocalInfo::ValueKind::String) {
      error = powName + " requires numeric arguments of the same type";
      return OperatorPowAbsSignEmitResult::Error;
    }

    if (powKind == LocalInfo::ValueKind::Float32 || powKind == LocalInfo::ValueKind::Float64) {
      fe.useFloatKind(powKind);
      int32_t tempBase = allocTempLocal();
      int32_t tempExponent = allocTempLocal();
      int32_t tempOut = allocTempLocal();
      int32_t tempLog = allocTempLocal();
      int32_t tempMul = allocTempLocal();

      if (!emitExpr(expr.args[0], localsIn)) {
        return OperatorPowAbsSignEmitResult::Error;
      }
      fe.store(tempBase);
      if (!emitExpr(expr.args[1], localsIn)) {
        return OperatorPowAbsSignEmitResult::Error;
      }
      fe.store(tempExponent);

      fe.load(tempBase).push(0.0).cmpLt();
      size_t jumpIfNotNegative = fe.jumpIfZero();
      fe.push(0.0).push(0.0).div().store(tempOut);
      size_t jumpToEndNegative = fe.jump();

      fe.patch(jumpIfNotNegative);

      fe.load(tempBase).push(0.0).cmpEq();
      size_t jumpIfNotZeroBase = fe.jumpIfZero();

      fe.load(tempExponent).push(0.0).cmpEq();
      size_t jumpIfExpNotZero = fe.jumpIfZero();
      fe.push(1.0).store(tempOut);
      size_t jumpToEndZeroExp = fe.jump();

      fe.patch(jumpIfExpNotZero);
      fe.load(tempExponent).push(0.0).cmpLt();
      size_t jumpIfExpPositive = fe.jumpIfZero();
      fe.push(1.0).push(0.0).div().store(tempOut);
      size_t jumpToEndNegExp = fe.jump();

      fe.patch(jumpIfExpPositive);
      fe.push(0.0).store(tempOut);
      size_t jumpToEndZeroBase = fe.jump();

      fe.patch(jumpIfNotZeroBase);

      emitLogSeries(fe, allocTempLocal, tempBase, tempLog);
      fe.load(tempExponent).load(tempLog).mul().store(tempMul);
      emitExpSeries(fe, allocTempLocal, tempMul, tempOut);

      fe.patch(jumpToEndNegative);
      fe.patch(jumpToEndZeroExp);
      fe.patch(jumpToEndNegExp);
      fe.patch(jumpToEndZeroBase);
      fe.load(tempOut);
      return OperatorPowAbsSignEmitResult::Handled;
    }

    IrOpcode mulOp =
        (powKind == LocalInfo::ValueKind::Int64 ||
         (powKind == LocalInfo::ValueKind::UInt64 || powKind == LocalInfo::ValueKind::UInt8))
            ? IrOpcode::MulI64
            : IrOpcode::MulI32;
    IrOpcode subOp =
        (powKind == LocalInfo::ValueKind::Int64 ||
         (powKind == LocalInfo::ValueKind::UInt64 || powKind == LocalInfo::ValueKind::UInt8))
            ? IrOpcode::SubI64
            : IrOpcode::SubI32;
    IrOpcode cmpGtOp = IrOpcode::CmpGtI32;
    if ((powKind == LocalInfo::ValueKind::UInt64 || powKind == LocalInfo::ValueKind::UInt8)) {
      cmpGtOp = IrOpcode::CmpGtU64;
    } else if (powKind == LocalInfo::ValueKind::Int64) {
      cmpGtOp = IrOpcode::CmpGtI64;
    } else {
      cmpGtOp = IrOpcode::CmpGtI32;
    }
    IrOpcode pushOp =
        (powKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;

    int32_t tempBase = allocTempLocal();
    int32_t tempExp = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorPowAbsSignEmitResult::Error;
    }
    fe.store(tempBase);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorPowAbsSignEmitResult::Error;
    }
    fe.store(tempExp);

    if (powKind == LocalInfo::ValueKind::Int32 || powKind == LocalInfo::ValueKind::Int64) {
      IrOpcode cmpLtOp =
          (powKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtI32;
      fe.load(tempExp).op(pushOp).op(cmpLtOp);
      const size_t jumpNonNegative = fe.here();
      fe.op(IrOpcode::JumpIfZero);
      emitPowNegativeExponent();
      fe.patch(jumpNonNegative);
    }

    instructions.push_back({pushOp, 1});
    fe.store(tempOut);

    const size_t loopStart = fe.here();
    fe.load(tempExp).op(pushOp).op(cmpGtOp);
    const size_t jumpToEnd = fe.here();
    fe.op(IrOpcode::JumpIfZero);

    fe.load(tempOut).load(tempBase).op(mulOp);
    if (mulOp == IrOpcode::MulI32) {
      instructions.push_back({IrOpcode::SextI32, 0}); // i32 wraps at 32 bits
    }
    fe.store(tempOut);

    fe.load(tempExp);
    instructions.push_back({pushOp, 1});
    fe.op(subOp).store(tempExp);

    fe.jumpTo(loopStart);

    fe.patch(jumpToEnd);
    fe.load(tempOut);
    return OperatorPowAbsSignEmitResult::Handled;
  }
  std::string absSignName;
  if (getBuiltinAbsSignName(expr, absSignName, hasMathImport)) {
    if (expr.args.size() != 1) {
      error = absSignName + " requires exactly one argument";
      return OperatorPowAbsSignEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
    if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::Bool ||
        argKind == LocalInfo::ValueKind::String) {
      error = absSignName + " requires numeric argument";
      return OperatorPowAbsSignEmitResult::Error;
    }
    int32_t tempValue = allocTempLocal();
    int32_t tempOut = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorPowAbsSignEmitResult::Error;
    }
    fe.store(tempValue);

    fe.useFloatKind(argKind);
    auto pushZero = [&]() {
      if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
        fe.push(0.0);
        return;
      }
      instructions.push_back(
          {argKind == LocalInfo::ValueKind::Int32 ? IrOpcode::PushI32 : IrOpcode::PushI64, 0});
    };
    auto pushOne = [&](double value) {
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

    if (absSignName == "abs") {
      if ((argKind == LocalInfo::ValueKind::UInt64 || argKind == LocalInfo::ValueKind::UInt8)) {
        fe.load(tempValue).store(tempOut).load(tempOut);
        return OperatorPowAbsSignEmitResult::Handled;
      }
      IrOpcode cmpLt = IrOpcode::CmpLtI32;
      IrOpcode negOp = IrOpcode::NegI32;
      if (argKind == LocalInfo::ValueKind::Int64) {
        cmpLt = IrOpcode::CmpLtI64;
        negOp = IrOpcode::NegI64;
      } else if (argKind == LocalInfo::ValueKind::Float64) {
        cmpLt = IrOpcode::CmpLtF64;
        negOp = IrOpcode::NegF64;
      } else if (argKind == LocalInfo::ValueKind::Float32) {
        cmpLt = IrOpcode::CmpLtF32;
        negOp = IrOpcode::NegF32;
      }
      fe.load(tempValue);
      pushZero();
      fe.op(cmpLt);
      size_t useValue = fe.jumpIfZero();
      fe.load(tempValue).op(negOp);
      if (negOp == IrOpcode::NegI32) {
        instructions.push_back({IrOpcode::SextI32, 0}); // abs(i32 min) wraps
      }
      fe.store(tempOut);
      size_t jumpToEnd = fe.jump();
      fe.patch(useValue);
      fe.load(tempValue).store(tempOut);
      fe.patch(jumpToEnd);
      fe.load(tempOut);
      return OperatorPowAbsSignEmitResult::Handled;
    }

    if ((argKind == LocalInfo::ValueKind::UInt64 || argKind == LocalInfo::ValueKind::UInt8)) {
      fe.load(tempValue);
      pushZero();
      fe.op(IrOpcode::CmpEqI64);
      size_t nonZeroIndex = fe.jumpIfZero();
      pushOne(0);
      fe.store(tempOut);
      size_t jumpToEnd = fe.jump();
      fe.patch(nonZeroIndex);
      pushOne(1);
      fe.store(tempOut);
      fe.patch(jumpToEnd);
      fe.load(tempOut);
      return OperatorPowAbsSignEmitResult::Handled;
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
    pushZero();
    fe.op(cmpLt);
    size_t checkPositive = fe.jumpIfZero();
    pushOne(-1);
    fe.store(tempOut);
    size_t jumpToEnd = fe.jump();
    fe.patch(checkPositive);
    fe.load(tempValue);
    pushZero();
    fe.op(cmpGt);
    size_t useZero = fe.jumpIfZero();
    pushOne(1);
    fe.store(tempOut);
    size_t jumpToEnd2 = fe.jump();
    fe.patch(useZero);
    pushOne(0);
    fe.store(tempOut);
    fe.patch(jumpToEnd);
    fe.patch(jumpToEnd2);
    fe.load(tempOut);
    return OperatorPowAbsSignEmitResult::Handled;
  }
  return OperatorPowAbsSignEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

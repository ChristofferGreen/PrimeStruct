#include "IrLowererOperatorAngleTrigHelpers.h"

#include "IrLowererHelpers.h"

#include <cstring>

namespace primec::ir_lowerer {

OperatorClampMinMaxTrigEmitResult emitAngleTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitClampMinMaxTrigExprWithLocalsFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const AllocClampMinMaxTrigTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  std::string angleName;
  if (getBuiltinAngleName(expr, angleName, hasMathImport)) {
    if (expr.args.size() != 1) {
      error = angleName + " requires exactly one argument";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
    if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
      error = angleName + " requires float argument";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    auto pushFloatConst = [&](double value) {
      if (argKind == LocalInfo::ValueKind::Float64) {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        instructions.push_back({IrOpcode::PushF64, bits});
        return;
      }
      float f32 = static_cast<float>(value);
      uint32_t bits = 0;
      std::memcpy(&bits, &f32, sizeof(bits));
      instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
    };
    if (angleName == "radians") {
      pushFloatConst(0.017453292519943295); // pi / 180
    } else {
      pushFloatConst(57.29577951308232); // 180 / pi
    }
    instructions.push_back(
        {argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::MulF64 : IrOpcode::MulF32, 0});
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }

  std::string trigName;
  if (getBuiltinTrigName(expr, trigName, hasMathImport)) {
    if (expr.args.size() != 1) {
      error = trigName + " requires exactly one argument";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
    if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
      error = trigName + " requires float argument";
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
    IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
    IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
    IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;

    auto pushFloatConst = [&](double value) {
      if (argKind == LocalInfo::ValueKind::Float64) {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        instructions.push_back({IrOpcode::PushF64, bits});
        return;
      }
      float f32 = static_cast<float>(value);
      uint32_t bits = 0;
      std::memcpy(&bits, &f32, sizeof(bits));
      instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
    };

    auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightLocal)});
      instructions.push_back({mulOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
    };
    auto emitSquare = [&](int32_t valueLocal, int32_t outLocal) {
      emitMulLocal(valueLocal, valueLocal, outLocal);
    };

    auto emitFloatTruncToLocal = [&](int32_t valueLocal, int32_t outLocal) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      if (argKind == LocalInfo::ValueKind::Float32) {
        instructions.push_back({IrOpcode::ConvertF32ToI32, 0});
        instructions.push_back({IrOpcode::ConvertI32ToF32, 0});
      } else {
        instructions.push_back({IrOpcode::ConvertF64ToI64, 0});
        instructions.push_back({IrOpcode::ConvertI64ToF64, 0});
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
    };

    auto emitFloorToLocal = [&](int32_t valueLocal, int32_t outLocal) {
      emitFloatTruncToLocal(valueLocal, outLocal);
      IrOpcode cmpOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpGtF64 : IrOpcode::CmpGtF32;
      IrOpcode subFloatOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({cmpOp, 0});
      size_t useValue = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
      pushFloatConst(1.0);
      instructions.push_back({subFloatOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
      size_t jumpEnd = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      size_t useValueIndex = instructions.size();
      instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
      size_t endIndex = instructions.size();
      instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
    };

    int32_t tempX = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTau = 6.28318530717958647692;
    constexpr double kHalfPi = 1.57079632679489661923;
    int32_t tempScale = allocTempLocal();
    int32_t tempFloor = allocTempLocal();
    int32_t tempMul = allocTempLocal();
    IrOpcode cmpGtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpGtF64 : IrOpcode::CmpGtF32;
    IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

    // Range-reduce x into [-pi, pi] to keep the series stable.
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(kTau);
    instructions.push_back({divOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempScale)});
    emitFloorToLocal(tempScale, tempFloor);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempFloor)});
    pushFloatConst(kTau);
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMul)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMul)});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(kPi);
    instructions.push_back({cmpGtOp, 0});
    size_t skipWrap = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(kTau);
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
    size_t jumpWrapEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    size_t skipWrapIndex = instructions.size();
    instructions[skipWrap].imm = static_cast<int32_t>(skipWrapIndex);
    size_t wrapEndIndex = instructions.size();
    instructions[jumpWrapEnd].imm = static_cast<int32_t>(wrapEndIndex);

    int32_t tempSinSign = allocTempLocal();
    int32_t tempCosSign = allocTempLocal();
    pushFloatConst(1.0);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSinSign)});
    pushFloatConst(1.0);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(kHalfPi);
    instructions.push_back({cmpGtOp, 0});
    size_t skipUpper = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    pushFloatConst(kPi);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(-1.0);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});
    size_t skipUpperIndex = instructions.size();
    instructions[skipUpper].imm = static_cast<int32_t>(skipUpperIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(-kHalfPi);
    instructions.push_back({cmpLtOp, 0});
    size_t skipLower = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(kPi);
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(-1.0);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});
    pushFloatConst(-1.0);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSinSign)});
    size_t skipLowerIndex = instructions.size();
    instructions[skipLower].imm = static_cast<int32_t>(skipLowerIndex);

    int32_t tempX2 = allocTempLocal();
    int32_t tempX3 = allocTempLocal();
    int32_t tempX5 = allocTempLocal();
    int32_t tempX7 = allocTempLocal();
    emitSquare(tempX, tempX2);
    emitMulLocal(tempX2, tempX, tempX3);
    emitMulLocal(tempX3, tempX2, tempX5);
    emitMulLocal(tempX5, tempX2, tempX7);

    int32_t tempSin = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
    pushFloatConst(0.16666666666666666);
    instructions.push_back({mulOp, 0});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
    pushFloatConst(0.008333333333333333);
    instructions.push_back({mulOp, 0});
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
    pushFloatConst(0.0001984126984126984);
    instructions.push_back({mulOp, 0});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSinSign)});
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSin)});

    int32_t tempX4 = allocTempLocal();
    int32_t tempX6 = allocTempLocal();
    emitSquare(tempX2, tempX4);
    emitMulLocal(tempX4, tempX2, tempX6);

    int32_t tempCos = allocTempLocal();
    pushFloatConst(1.0);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
    pushFloatConst(0.5);
    instructions.push_back({mulOp, 0});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX4)});
    pushFloatConst(0.041666666666666664);
    instructions.push_back({mulOp, 0});
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX6)});
    pushFloatConst(0.001388888888888889);
    instructions.push_back({mulOp, 0});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCosSign)});
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCos)});

    if (trigName == "sin") {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSin)});
      return OperatorClampMinMaxTrigEmitResult::Handled;
    }
    if (trigName == "cos") {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCos)});
      return OperatorClampMinMaxTrigEmitResult::Handled;
    }

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSin)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCos)});
    instructions.push_back({divOp, 0});
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

    IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
    IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
    IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
    IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
    IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
    IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

    auto pushFloatConst = [&](double value) {
      if (argKind == LocalInfo::ValueKind::Float64) {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        instructions.push_back({IrOpcode::PushF64, bits});
        return;
      }
      float f32 = static_cast<float>(value);
      uint32_t bits = 0;
      std::memcpy(&bits, &f32, sizeof(bits));
      instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
    };

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
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempY)});
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorClampMinMaxTrigEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

    // Handle x == 0 axis cases.
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(0.0);
    instructions.push_back({cmpEqOp, 0});
    size_t jumpIfNotZeroX = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
    pushFloatConst(0.0);
    instructions.push_back({cmpEqOp, 0});
    size_t jumpIfNotZeroY = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    pushFloatConst(0.0);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
    size_t jumpAxisEndZero = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    size_t nonZeroYIndex = instructions.size();
    instructions[jumpIfNotZeroY].imm = static_cast<int32_t>(nonZeroYIndex);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
    pushFloatConst(0.0);
    instructions.push_back({cmpLtOp, 0});
    size_t jumpIfPosY = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    pushFloatConst(-1.5707963267948966);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
    size_t jumpAxisEndNeg = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    size_t posYIndex = instructions.size();
    instructions[jumpIfPosY].imm = static_cast<int32_t>(posYIndex);
    pushFloatConst(1.5707963267948966);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
    size_t jumpAxisEndPos = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    size_t notZeroXIndex = instructions.size();
    instructions[jumpIfNotZeroX].imm = static_cast<int32_t>(notZeroXIndex);

    // atan2(y, x) ≈ atan(y/x) with quadrant adjustment.
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    instructions.push_back({divOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ)});

    auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightLocal)});
      instructions.push_back({mulOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
    };

    emitMulLocal(tempZ, tempZ, tempZ2);
    emitMulLocal(tempZ2, tempZ, tempZ3);
    emitMulLocal(tempZ3, tempZ2, tempZ5);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ3)});
    pushFloatConst(3.0);
    instructions.push_back({divOp, 0});
    instructions.push_back({subOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ5)});
    pushFloatConst(5.0);
    instructions.push_back({divOp, 0});
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    pushFloatConst(0.0);
    instructions.push_back({cmpLtOp, 0});
    size_t jumpIfNotNegX = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
    pushFloatConst(0.0);
    instructions.push_back({cmpLtOp, 0});
    size_t jumpIfPosYAdj = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
    pushFloatConst(-3.14159265358979323846);
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
    size_t jumpAdjustEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    size_t posYAdjIndex = instructions.size();
    instructions[jumpIfPosYAdj].imm = static_cast<int32_t>(posYAdjIndex);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
    pushFloatConst(3.14159265358979323846);
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
    size_t adjustEndIndex = instructions.size();
    instructions[jumpAdjustEnd].imm = static_cast<int32_t>(adjustEndIndex);

    size_t notNegXIndex = instructions.size();
    instructions[jumpIfNotNegX].imm = static_cast<int32_t>(notNegXIndex);

    size_t endIndex = instructions.size();
    instructions[jumpAxisEndZero].imm = static_cast<int32_t>(endIndex);
    instructions[jumpAxisEndNeg].imm = static_cast<int32_t>(endIndex);
    instructions[jumpAxisEndPos].imm = static_cast<int32_t>(endIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
    return OperatorClampMinMaxTrigEmitResult::Handled;
  }

  return OperatorClampMinMaxTrigEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

#include "IrLowererOperatorArcHyperbolicHelpers.h"
#include "IrLowererOperatorExpLogHyperbolicHelpers.h"

#include "IrLowererHelpers.h"

#include <cstring>

namespace primec::ir_lowerer {

OperatorArcHyperbolicEmitResult emitArcHyperbolicOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitArcHyperbolicExprWithLocalsFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const AllocArcHyperbolicTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  std::string arcName;
  if (getBuiltinArcTrigName(expr, arcName, hasMathImport)) {
    if (expr.args.size() != 1) {
      error = arcName + " requires exactly one argument";
      return OperatorArcHyperbolicEmitResult::Error;
    }
    LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
    if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
      error = arcName + " requires float argument";
      return OperatorArcHyperbolicEmitResult::Error;
    }
    IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
    IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
    IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
    IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
    IrOpcode negOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::NegF64 : IrOpcode::NegF32;
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

    auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightLocal)});
      instructions.push_back({mulOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
    };

    int32_t tempX = allocTempLocal();
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorArcHyperbolicEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

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

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(inputLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
        pushFloatConst(0.3333333333333333);
        instructions.push_back({mulOp, 0});
        instructions.push_back({subOp, 0});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
        pushFloatConst(0.2);
        instructions.push_back({mulOp, 0});
        instructions.push_back({addOp, 0});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
        pushFloatConst(0.14285714285714285);
        instructions.push_back({mulOp, 0});
        instructions.push_back({subOp, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
      };

      int32_t tempAbs = allocTempLocal();
      int32_t tempSign = allocTempLocal();
      int32_t tempInv = allocTempLocal();
      int32_t tempOut = allocTempLocal();

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
      pushFloatConst(0.0);
      instructions.push_back({cmpLtOp, 0});
      size_t jumpIfNotNeg = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
      instructions.push_back({negOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAbs)});
      pushFloatConst(-1.0);
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSign)});
      size_t jumpAfterSign = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});

      size_t nonNegIndex = instructions.size();
      instructions[jumpIfNotNeg].imm = static_cast<int32_t>(nonNegIndex);
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAbs)});
      pushFloatConst(1.0);
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSign)});
      size_t afterSignIndex = instructions.size();
      instructions[jumpAfterSign].imm = static_cast<int32_t>(afterSignIndex);

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAbs)});
      pushFloatConst(1.0);
      instructions.push_back({cmpLtOp, 0});
      size_t jumpIfNotSmall = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});

      emitAtanSeriesForLocal(tempAbs, tempOut);
      size_t jumpAfterAtan = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});

      size_t notSmallIndex = instructions.size();
      instructions[jumpIfNotSmall].imm = static_cast<int32_t>(notSmallIndex);
      pushFloatConst(1.0);
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAbs)});
      instructions.push_back({divOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempInv)});
      emitAtanSeriesForLocal(tempInv, tempOut);
      pushFloatConst(1.5707963267948966);
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
      instructions.push_back({subOp, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

      size_t afterAtanIndex = instructions.size();
      instructions[jumpAfterAtan].imm = static_cast<int32_t>(afterAtanIndex);
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSign)});
      instructions.push_back({mulOp, 0});
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

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
    pushFloatConst(0.16666666666666666);
    instructions.push_back({mulOp, 0});
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
    pushFloatConst(0.075);
    instructions.push_back({mulOp, 0});
    instructions.push_back({addOp, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
    pushFloatConst(0.044642857142857144);
    instructions.push_back({mulOp, 0});
    instructions.push_back({addOp, 0});

    if (arcName == "asin") {
      return OperatorArcHyperbolicEmitResult::Handled;
    }

    int32_t tempAsin = allocTempLocal();
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAsin)});
    pushFloatConst(1.5707963267948966);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAsin)});
    instructions.push_back({subOp, 0});
    return OperatorArcHyperbolicEmitResult::Handled;
  }

  if (OperatorArcHyperbolicEmitResult result =
          emitExpOperatorExpr(expr, localsIn, hasMathImport, emitExpr, inferExprKind, allocTempLocal, instructions,
                              error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  if (OperatorArcHyperbolicEmitResult result =
          emitLogOperatorExpr(expr, localsIn, hasMathImport, emitExpr, inferExprKind, allocTempLocal, instructions,
                              error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  if (OperatorArcHyperbolicEmitResult result =
          emitHyperbolicOperatorExpr(expr, localsIn, hasMathImport, emitExpr, inferExprKind, allocTempLocal,
                                     instructions, error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  if (OperatorArcHyperbolicEmitResult result =
          emitArcHyperbolicBuiltinExpr(expr, localsIn, hasMathImport, emitExpr, inferExprKind, allocTempLocal,
                                       instructions, error);
      result != OperatorArcHyperbolicEmitResult::NotHandled) {
    return result;
  }
  return OperatorArcHyperbolicEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

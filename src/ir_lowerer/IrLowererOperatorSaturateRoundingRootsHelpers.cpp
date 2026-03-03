#include "IrLowererOperatorSaturateRoundingRootsHelpers.h"

#include "IrLowererHelpers.h"

#include <cstring>

namespace primec::ir_lowerer {

OperatorSaturateRoundingRootsEmitResult emitSaturateRoundingRootsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitSaturateExprWithLocalsFn &emitExpr,
    const InferSaturateExprKindWithLocalsFn &inferExprKind,
    const AllocSaturateTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

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
          auto pushConst = [&](double value) {
            if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
              pushFloatConst(value);
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

          if (argKind == LocalInfo::ValueKind::UInt64) {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushConst(1);
            instructions.push_back({IrOpcode::CmpGtU64, 0});
            size_t useValue = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushConst(1);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEnd = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});
            size_t useValueIndex = instructions.size();
            instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t endIndex = instructions.size();
            instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
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
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushConst(0);
          instructions.push_back({cmpLt, 0});
          size_t checkMax = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushConst(0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t checkMaxIndex = instructions.size();
          instructions[checkMax].imm = static_cast<int32_t>(checkMaxIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushConst(1);
          instructions.push_back({cmpGt, 0});
          size_t useValue = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushConst(1);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t useValueIndex = instructions.size();
          instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorSaturateRoundingRootsEmitResult::Handled;
        }
        std::string predicateName;
        if (getBuiltinMathPredicateName(expr, predicateName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = predicateName + " requires exactly one argument";
            return OperatorSaturateRoundingRootsEmitResult::Error;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = predicateName + " requires float argument";
            return OperatorSaturateRoundingRootsEmitResult::Error;
          }
          int32_t tempValue = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return OperatorSaturateRoundingRootsEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

          auto pushFloatZero = [&]() {
            if (argKind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              double value = 0.0;
              std::memcpy(&bits, &value, sizeof(bits));
              instructions.push_back({IrOpcode::PushF64, bits});
              return;
            }
            uint32_t bits = 0;
            float value = 0.0f;
            std::memcpy(&bits, &value, sizeof(bits));
            instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
          };

          IrOpcode cmpEq = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
          IrOpcode cmpNe = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpNeF64 : IrOpcode::CmpNeF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;

          if (predicateName == "is_nan") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({cmpNe, 0});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }

          if (predicateName == "is_finite") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({subOp, 0});
            pushFloatZero();
            instructions.push_back({cmpEq, 0});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({cmpEq, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({subOp, 0});
          pushFloatZero();
          instructions.push_back({cmpNe, 0});
          instructions.push_back({IrOpcode::MulI32, 0});
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

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
          auto emitFloatTrunc = [&](int32_t valueLocal, int32_t outLocal) {
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
          auto emitFloorOrCeil = [&](int32_t valueLocal, int32_t outLocal, bool isCeil) {
            emitFloatTrunc(valueLocal, outLocal);
            IrOpcode cmpOp = isCeil ? (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::CmpLtF64
                                                                               : IrOpcode::CmpLtF32)
                                   : (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::CmpGtF64
                                                                               : IrOpcode::CmpGtF32);
            IrOpcode addSubOp = isCeil ? (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::AddF64
                                                                                   : IrOpcode::AddF32)
                                       : (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::SubF64
                                                                                   : IrOpcode::SubF32);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({cmpOp, 0});
            size_t useValue = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
            pushFloatConst(1.0);
            instructions.push_back({addSubOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpEnd = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});
            size_t useValueIndex = instructions.size();
            instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
            size_t endIndex = instructions.size();
            instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          };

          if (argKind == LocalInfo::ValueKind::Int32 || argKind == LocalInfo::ValueKind::Int64 ||
              argKind == LocalInfo::ValueKind::UInt64) {
            if (roundingName == "fract") {
              if (argKind == LocalInfo::ValueKind::Int32) {
                instructions.push_back({IrOpcode::PushI32, 0});
              } else {
                instructions.push_back({IrOpcode::PushI64, 0});
              }
              return OperatorSaturateRoundingRootsEmitResult::Handled;
            }
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }

          const bool isF32 = (argKind == LocalInfo::ValueKind::Float32);
          const IrOpcode addOp = isF32 ? IrOpcode::AddF32 : IrOpcode::AddF64;
          const IrOpcode subOp = isF32 ? IrOpcode::SubF32 : IrOpcode::SubF64;
          const IrOpcode cmpLtOp = isF32 ? IrOpcode::CmpLtF32 : IrOpcode::CmpLtF64;

          if (roundingName == "trunc") {
            int32_t tempOut = allocTempLocal();
            emitFloatTrunc(tempValue, tempOut);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }

          int32_t tempOut = allocTempLocal();
          if (roundingName == "floor") {
            emitFloorOrCeil(tempValue, tempOut, false);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }
          if (roundingName == "ceil") {
            emitFloorOrCeil(tempValue, tempOut, true);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }
          if (roundingName == "fract") {
            emitFloorOrCeil(tempValue, tempOut, false);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            instructions.push_back({subOp, 0});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }
          if (roundingName == "round") {
            int32_t tempAdjusted = allocTempLocal();
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.0);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpNonNegative = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.5);
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAdjusted)});
            emitFloorOrCeil(tempAdjusted, tempOut, true);
            size_t jumpEnd = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = instructions.size();
            instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.5);
            instructions.push_back({addOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAdjusted)});
            emitFloorOrCeil(tempAdjusted, tempOut, false);

            size_t endIndex = instructions.size();
            instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }
          error = roundingName + " requires numeric argument";
          return OperatorSaturateRoundingRootsEmitResult::Error;
        }
        std::string rootName;
        if (getBuiltinRootName(expr, rootName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = rootName + " requires exactly one argument";
            return OperatorSaturateRoundingRootsEmitResult::Error;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = rootName + " requires float argument";
            return OperatorSaturateRoundingRootsEmitResult::Error;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          int32_t tempIter = allocTempLocal();
          int32_t tempX = allocTempLocal();

          if (!emitExpr(expr.args.front(), localsIn)) {
            return OperatorSaturateRoundingRootsEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

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

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushFloatConst(0.0);
          instructions.push_back({cmpEqOp, 0});
          size_t jumpIfNotZero = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(0.0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEndZero = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});

          size_t nonZeroIndex = instructions.size();
          instructions[jumpIfNotZero].imm = static_cast<int32_t>(nonZeroIndex);

          if (rootName == "sqrt") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.0);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNonNegative = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndNegative = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = instructions.size();
            instructions[jumpIfNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({IrOpcode::PushI32, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

            const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 8 : 6;
            const size_t loopStart = instructions.size();
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations)});
            instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpEndLoop = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({divOp, 0});
            instructions.push_back({addOp, 0});
            pushFloatConst(0.5);
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({IrOpcode::PushI32, 1});
            instructions.push_back({IrOpcode::AddI32, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

            size_t loopEndIndex = instructions.size();
            instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

            size_t endIndex = instructions.size();
            instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
            instructions[jumpToEndNegative].imm = static_cast<int32_t>(endIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return OperatorSaturateRoundingRootsEmitResult::Handled;
          }

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::PushI32, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

          const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 10 : 8;
          const size_t loopStart = instructions.size();
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations)});
          instructions.push_back({IrOpcode::CmpLtI32, 0});
          size_t jumpEndLoop = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(2.0);
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          instructions.push_back({divOp, 0});
          instructions.push_back({addOp, 0});
          pushFloatConst(3.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({IrOpcode::PushI32, 1});
          instructions.push_back({IrOpcode::AddI32, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

          size_t loopEndIndex = instructions.size();
          instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          size_t endIndex = instructions.size();
          instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorSaturateRoundingRootsEmitResult::Handled;
        }
  return OperatorSaturateRoundingRootsEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

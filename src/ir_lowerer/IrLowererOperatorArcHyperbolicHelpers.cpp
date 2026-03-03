#include "IrLowererOperatorArcHyperbolicHelpers.h"

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
        std::string expName;
        if (getBuiltinExpName(expr, expName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = expName + " requires exactly one argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = expName + " requires float argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }
          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode convertOp =
              (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::ConvertI32ToF64 : IrOpcode::ConvertI32ToF32;

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

          int32_t tempX = allocTempLocal();
          int32_t tempTerm = allocTempLocal();
          int32_t tempSum = allocTempLocal();
          int32_t tempIter = allocTempLocal();

          if (!emitExpr(expr.args.front(), localsIn)) {
            return OperatorArcHyperbolicEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          if (expName == "exp2") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            pushFloatConst(0.69314718055994530942); // ln(2)
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          }

          pushFloatConst(1.0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
          pushFloatConst(1.0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
          instructions.push_back({IrOpcode::PushI32, 1});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

          const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
          const size_t loopStart = instructions.size();
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations + 1)});
          instructions.push_back({IrOpcode::CmpLtI32, 0});
          size_t jumpEndLoop = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({convertOp, 0});
          instructions.push_back({divOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({IrOpcode::PushI32, 1});
          instructions.push_back({IrOpcode::AddI32, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});
          instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

          size_t loopEndIndex = instructions.size();
          instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
          return OperatorArcHyperbolicEmitResult::Handled;
        }
        std::string logName;
        if (getBuiltinLogName(expr, logName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = logName + " requires exactly one argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = logName + " requires float argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }
          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;

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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.0);
          instructions.push_back({cmpLtOp, 0});
          size_t jumpIfNotNegative = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(0.0);
          pushFloatConst(0.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});

          size_t nonNegativeIndex = instructions.size();
          instructions[jumpIfNotNegative].imm = static_cast<int32_t>(nonNegativeIndex);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.0);
          instructions.push_back({cmpEqOp, 0});
          size_t jumpIfNotZero = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(-1.0);
          pushFloatConst(0.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEndZero = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});

          size_t positiveIndex = instructions.size();
          instructions[jumpIfNotZero].imm = static_cast<int32_t>(positiveIndex);

          pushFloatConst(0.0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempK)});

          const size_t reduceHighStart = instructions.size();
          pushFloatConst(2.0);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({cmpLtOp, 0});
          size_t jumpAfterHigh = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(2.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempK)});
          pushFloatConst(1.0);
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempK)});
          instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(reduceHighStart)});
          size_t reduceHighEnd = instructions.size();
          instructions[jumpAfterHigh].imm = static_cast<int32_t>(reduceHighEnd);

          const size_t reduceLowStart = instructions.size();
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.5);
          instructions.push_back({cmpLtOp, 0});
          size_t jumpAfterLow = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(2.0);
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempK)});
          pushFloatConst(1.0);
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempK)});
          instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(reduceLowStart)});
          size_t reduceLowEnd = instructions.size();
          instructions[jumpAfterLow].imm = static_cast<int32_t>(reduceLowEnd);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(1.0);
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(1.0);
          instructions.push_back({addOp, 0});
          instructions.push_back({divOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ2)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

          auto appendSeries = [&](double divisor) {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ2)});
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
            pushFloatConst(1.0 / divisor);
            instructions.push_back({mulOp, 0});
            instructions.push_back({addOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
          };

          appendSeries(3.0);
          appendSeries(5.0);
          appendSeries(7.0);
          appendSeries(9.0);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
          pushFloatConst(2.0);
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempK)});
          pushFloatConst(0.69314718055994530942);
          instructions.push_back({mulOp, 0});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          if (logName == "log2") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            pushFloatConst(0.69314718055994530942);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          } else if (logName == "log10") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            pushFloatConst(2.30258509299404568402);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          }

          size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorArcHyperbolicEmitResult::Handled;
        }
        std::string hyperName;
        if (getBuiltinHyperbolicName(expr, hyperName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = hyperName + " requires exactly one argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = hyperName + " requires float argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode negOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::NegF64 : IrOpcode::NegF32;
          IrOpcode convertOp =
              (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::ConvertI32ToF64 : IrOpcode::ConvertI32ToF32;

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

          auto emitExpForLocal = [&](int32_t valueLocal, int32_t outLocal) {
            int32_t tempTerm = allocTempLocal();
            int32_t tempSum = allocTempLocal();
            int32_t tempIter = allocTempLocal();

            pushFloatConst(1.0);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
            pushFloatConst(1.0);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
            instructions.push_back({IrOpcode::PushI32, 1});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

            const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
            const size_t loopStart = instructions.size();
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations + 1)});
            instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpEndLoop = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({convertOp, 0});
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
            instructions.push_back({addOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({IrOpcode::PushI32, 1});
            instructions.push_back({IrOpcode::AddI32, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});
            instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

            const size_t loopEndIndex = instructions.size();
            instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
          };

          int32_t tempX = allocTempLocal();
          int32_t tempNegX = allocTempLocal();
          int32_t tempExpPos = allocTempLocal();
          int32_t tempExpNeg = allocTempLocal();

          if (!emitExpr(expr.args.front(), localsIn)) {
            return OperatorArcHyperbolicEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({negOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempNegX)});

          emitExpForLocal(tempX, tempExpPos);
          emitExpForLocal(tempNegX, tempExpNeg);

          if (hyperName == "sinh") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
            instructions.push_back({subOp, 0});
            pushFloatConst(2.0);
            instructions.push_back({divOp, 0});
            return OperatorArcHyperbolicEmitResult::Handled;
          }

          if (hyperName == "cosh") {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
            instructions.push_back({addOp, 0});
            pushFloatConst(2.0);
            instructions.push_back({divOp, 0});
            return OperatorArcHyperbolicEmitResult::Handled;
          }

          int32_t tempNumerator = allocTempLocal();
          int32_t tempDenominator = allocTempLocal();
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempNumerator)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempDenominator)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempNumerator)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempDenominator)});
          instructions.push_back({divOp, 0});
          return OperatorArcHyperbolicEmitResult::Handled;
        }
        std::string arcHyperName;
        if (getBuiltinArcHyperbolicName(expr, arcHyperName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = arcHyperName + " requires exactly one argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = arcHyperName + " requires float argument";
            return OperatorArcHyperbolicEmitResult::Error;
          }

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;

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

          auto emitSqrtForLocal = [&](int32_t valueLocal, int32_t outLocal) {
            int32_t tempX = allocTempLocal();
            int32_t tempIter = allocTempLocal();

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(0.0);
            instructions.push_back({cmpEqOp, 0});
            size_t jumpIfNotZero = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpToEndZero = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonZeroIndex = instructions.size();
            instructions[jumpIfNotZero].imm = static_cast<int32_t>(nonZeroIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(0.0);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNonNegative = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpToEndNegative = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = instructions.size();
            instructions[jumpIfNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
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
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
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

            const size_t loopEndIndex = instructions.size();
            instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});

            const size_t endIndex = instructions.size();
            instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
            instructions[jumpToEndNegative].imm = static_cast<int32_t>(endIndex);
          };

          auto emitLogForLocal = [&](int32_t valueLocal, int32_t outLocal) {
            int32_t tempZ = allocTempLocal();
            int32_t tempZ2 = allocTempLocal();
            int32_t tempTerm = allocTempLocal();
            int32_t tempSum = allocTempLocal();

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(0.0);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNotNegative = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpToEnd = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = instructions.size();
            instructions[jumpIfNotNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(0.0);
            instructions.push_back({cmpEqOp, 0});
            size_t jumpIfNotZero = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(-1.0);
            pushFloatConst(0.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpToEndZero = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t positiveIndex = instructions.size();
            instructions[jumpIfNotZero].imm = static_cast<int32_t>(positiveIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(1.0);
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(1.0);
            instructions.push_back({addOp, 0});
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ2)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

            auto appendSeries = [&](double divisor) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ2)});
              instructions.push_back({mulOp, 0});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
              pushFloatConst(1.0 / divisor);
              instructions.push_back({mulOp, 0});
              instructions.push_back({addOp, 0});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
            };

            appendSeries(3.0);
            appendSeries(5.0);
            appendSeries(7.0);
            appendSeries(9.0);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
            pushFloatConst(2.0);
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});

            const size_t endIndex = instructions.size();
            instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
            instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
          };

          int32_t tempX = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return OperatorArcHyperbolicEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          int32_t tempOut = allocTempLocal();
          if (arcHyperName == "atanh") {
            int32_t tempOnePlus = allocTempLocal();
            int32_t tempOneMinus = allocTempLocal();
            int32_t tempRatio = allocTempLocal();

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            pushFloatConst(1.0);
            instructions.push_back({addOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOnePlus)});

            pushFloatConst(1.0);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOneMinus)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOnePlus)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOneMinus)});
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempRatio)});

            emitLogForLocal(tempRatio, tempOut);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            pushFloatConst(0.5);
            instructions.push_back({mulOp, 0});
            return OperatorArcHyperbolicEmitResult::Handled;
          }

          int32_t tempX2 = allocTempLocal();
          int32_t tempInner = allocTempLocal();
          int32_t tempSqrt = allocTempLocal();
          int32_t tempSum = allocTempLocal();

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX2)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
          pushFloatConst(1.0);
          instructions.push_back({arcHyperName == "asinh" ? addOp : subOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempInner)});

          emitSqrtForLocal(tempInner, tempSqrt);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSqrt)});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

          emitLogForLocal(tempSum, tempOut);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorArcHyperbolicEmitResult::Handled;
        }
  return OperatorArcHyperbolicEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

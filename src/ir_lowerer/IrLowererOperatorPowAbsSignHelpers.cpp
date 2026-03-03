#include "IrLowererOperatorPowAbsSignHelpers.h"

#include "IrLowererHelpers.h"

#include <cstring>

namespace primec::ir_lowerer {

OperatorPowAbsSignEmitResult emitPowAbsSignOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        bool hasMathImport,
                                                        const EmitPowAbsSignExprWithLocalsFn &emitExpr,
                                                        const InferPowAbsSignExprKindWithLocalsFn &inferExprKind,
                                                        const CombinePowAbsSignNumericKindsFn &combineNumericKinds,
                                                        const AllocPowAbsSignTempLocalFn &allocTempLocal,
                                                        const EmitPowNegativeExponentFn &emitPowNegativeExponent,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error) {
        std::string powName;
        if (getBuiltinPowName(expr, powName, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = powName + " requires exactly two arguments";
            return OperatorPowAbsSignEmitResult::Error;
          }
          bool sawUnsigned = false;
          bool sawSigned = false;
          for (const auto &arg : expr.args) {
            LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
            if (arg.kind == Expr::Kind::Literal && arg.isUnsigned) {
              sawUnsigned = true;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              sawUnsigned = true;
            } else if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64) {
              sawSigned = true;
            }
          }
          if (sawUnsigned && sawSigned) {
            error = powName + " requires numeric arguments of the same type";
            return OperatorPowAbsSignEmitResult::Error;
          }
          LocalInfo::ValueKind powKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (powKind == LocalInfo::ValueKind::Unknown || powKind == LocalInfo::ValueKind::Bool ||
              powKind == LocalInfo::ValueKind::String) {
            error = powName + " requires numeric arguments of the same type";
            return OperatorPowAbsSignEmitResult::Error;
          }

          if (powKind == LocalInfo::ValueKind::Float32 || powKind == LocalInfo::ValueKind::Float64) {
            IrOpcode addOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
            IrOpcode subOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
            IrOpcode mulOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
            IrOpcode divOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
            IrOpcode cmpLtOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
            IrOpcode cmpEqOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
            IrOpcode convertOp =
                (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::ConvertI32ToF64 : IrOpcode::ConvertI32ToF32;

            auto pushFloatConst = [&](double value) {
              if (powKind == LocalInfo::ValueKind::Float64) {
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

              const int32_t iterations = (powKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
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

            int32_t tempBase = allocTempLocal();
            int32_t tempExponent = allocTempLocal();
            int32_t tempOut = allocTempLocal();
            int32_t tempLog = allocTempLocal();
            int32_t tempMul = allocTempLocal();

            if (!emitExpr(expr.args[0], localsIn)) {
              return OperatorPowAbsSignEmitResult::Error;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempBase)});
            if (!emitExpr(expr.args[1], localsIn)) {
              return OperatorPowAbsSignEmitResult::Error;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempExponent)});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempBase)});
            pushFloatConst(0.0);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNotNegative = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndNegative = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = instructions.size();
            instructions[jumpIfNotNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempBase)});
            pushFloatConst(0.0);
            instructions.push_back({cmpEqOp, 0});
            size_t jumpIfNotZeroBase = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExponent)});
            pushFloatConst(0.0);
            instructions.push_back({cmpEqOp, 0});
            size_t jumpIfExpNotZero = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(1.0);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndZeroExp = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t expNotZeroIndex = instructions.size();
            instructions[jumpIfExpNotZero].imm = static_cast<int32_t>(expNotZeroIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExponent)});
            pushFloatConst(0.0);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfExpPositive = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(1.0);
            pushFloatConst(0.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndNegExp = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t expPositiveIndex = instructions.size();
            instructions[jumpIfExpPositive].imm = static_cast<int32_t>(expPositiveIndex);
            pushFloatConst(0.0);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndZeroBase = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});

            size_t nonZeroBaseIndex = instructions.size();
            instructions[jumpIfNotZeroBase].imm = static_cast<int32_t>(nonZeroBaseIndex);

            emitLogForLocal(tempBase, tempLog);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExponent)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempLog)});
            instructions.push_back({mulOp, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMul)});
            emitExpForLocal(tempMul, tempOut);

            size_t endIndex = instructions.size();
            instructions[jumpToEndNegative].imm = static_cast<int32_t>(endIndex);
            instructions[jumpToEndZeroExp].imm = static_cast<int32_t>(endIndex);
            instructions[jumpToEndNegExp].imm = static_cast<int32_t>(endIndex);
            instructions[jumpToEndZeroBase].imm = static_cast<int32_t>(endIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return OperatorPowAbsSignEmitResult::Handled;
          }

          IrOpcode mulOp =
              (powKind == LocalInfo::ValueKind::Int64 || powKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::MulI64
                  : IrOpcode::MulI32;
          IrOpcode subOp =
              (powKind == LocalInfo::ValueKind::Int64 || powKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::SubI64
                  : IrOpcode::SubI32;
          IrOpcode cmpGtOp = IrOpcode::CmpGtI32;
          if (powKind == LocalInfo::ValueKind::UInt64) {
            cmpGtOp = IrOpcode::CmpGtU64;
          } else if (powKind == LocalInfo::ValueKind::Int64) {
            cmpGtOp = IrOpcode::CmpGtI64;
          } else {
            cmpGtOp = IrOpcode::CmpGtI32;
          }
          IrOpcode pushOp = (powKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;

          int32_t tempBase = allocTempLocal();
          int32_t tempExp = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return OperatorPowAbsSignEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempBase)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorPowAbsSignEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempExp)});

          if (powKind == LocalInfo::ValueKind::Int32 || powKind == LocalInfo::ValueKind::Int64) {
            IrOpcode cmpLtOp = (powKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtI32;
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExp)});
            instructions.push_back({pushOp, 0});
            instructions.push_back({cmpLtOp, 0});
            const size_t jumpNonNegative = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitPowNegativeExponent();
            const size_t nonNegativeIndex = instructions.size();
            instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
          }

          instructions.push_back({pushOp, 1});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          const size_t loopStart = instructions.size();
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExp)});
          instructions.push_back({pushOp, 0});
          instructions.push_back({cmpGtOp, 0});
          const size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempBase)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExp)});
          instructions.push_back({pushOp, 1});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempExp)});

          instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

          const size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
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
          auto pushZero = [&]() {
            if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
              pushFloatConst(0.0);
              return;
            }
            instructions.push_back(
                {argKind == LocalInfo::ValueKind::Int32 ? IrOpcode::PushI32 : IrOpcode::PushI64, 0});
          };
          auto pushOne = [&](double value) {
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

          if (absSignName == "abs") {
            if (argKind == LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
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
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushZero();
            instructions.push_back({cmpLt, 0});
            size_t useValue = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({negOp, 0});
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
            return OperatorPowAbsSignEmitResult::Handled;
          }

          if (argKind == LocalInfo::ValueKind::UInt64) {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushZero();
            instructions.push_back({IrOpcode::CmpEqI64, 0});
            size_t nonZeroIndex = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushOne(0);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEnd = instructions.size();
            instructions.push_back({IrOpcode::Jump, 0});
            size_t notZeroLabel = instructions.size();
            instructions[nonZeroIndex].imm = static_cast<int32_t>(notZeroLabel);
            pushOne(1);
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t endIndex = instructions.size();
            instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
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
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushZero();
          instructions.push_back({cmpLt, 0});
          size_t checkPositive = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushOne(-1);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t positiveIndex = instructions.size();
          instructions[checkPositive].imm = static_cast<int32_t>(positiveIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushZero();
          instructions.push_back({cmpGt, 0});
          size_t useZero = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushOne(1);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t zeroIndex = instructions.size();
          instructions[useZero].imm = static_cast<int32_t>(zeroIndex);
          pushOne(0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorPowAbsSignEmitResult::Handled;
        }
  return OperatorPowAbsSignEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer

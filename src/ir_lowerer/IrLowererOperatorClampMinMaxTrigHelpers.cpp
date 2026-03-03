#include "IrLowererOperatorClampMinMaxTrigHelpers.h"

#include "IrLowererHelpers.h"

#include <cstring>

namespace primec::ir_lowerer {

OperatorClampMinMaxTrigEmitResult emitClampMinMaxTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitClampMinMaxTrigExprWithLocalsFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const CombineClampMinMaxTrigNumericKindsFn &combineNumericKinds,
    const AllocClampMinMaxTrigTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
        if (getBuiltinClampName(expr, hasMathImport)) {
          if (expr.args.size() != 3) {
            error = "clamp requires exactly three arguments";
            return OperatorClampMinMaxTrigEmitResult::Error;
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
          if (clampKind == LocalInfo::ValueKind::UInt64) {
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMin)});
          if (!emitExpr(expr.args[2], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMax)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          instructions.push_back({cmpLt, 0});
          size_t skipMin = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t checkMax = instructions.size();
          instructions[skipMin].imm = static_cast<int32_t>(checkMax);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          instructions.push_back({cmpGt, 0});
          size_t skipMax = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t useValue = instructions.size();
          instructions[skipMax].imm = static_cast<int32_t>(useValue);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorClampMinMaxTrigEmitResult::Handled;
        }
        std::string minMaxName;
        if (getBuiltinMinMaxName(expr, minMaxName, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = minMaxName + " requires exactly two arguments";
            return OperatorClampMinMaxTrigEmitResult::Error;
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
            error = minMaxName + " requires numeric arguments of the same type";
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          LocalInfo::ValueKind minMaxKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (minMaxKind == LocalInfo::ValueKind::Unknown) {
            error = minMaxName + " requires numeric arguments of the same type";
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          IrOpcode cmpOp = IrOpcode::CmpLtI32;
          if (minMaxName == "max") {
            cmpOp = IrOpcode::CmpGtI32;
          }
          if (minMaxKind == LocalInfo::ValueKind::UInt64) {
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempLeft)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempRight)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempLeft)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempRight)});
          instructions.push_back({cmpOp, 0});
          size_t useRight = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempLeft)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});
          size_t useRightIndex = instructions.size();
          instructions[useRight].imm = static_cast<int32_t>(useRightIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempRight)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorClampMinMaxTrigEmitResult::Handled;
        }
        std::string lerpName;
        if (getBuiltinLerpName(expr, lerpName, hasMathImport)) {
          if (expr.args.size() != 3) {
            error = lerpName + " requires exactly three arguments";
            return OperatorClampMinMaxTrigEmitResult::Error;
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
          IrOpcode addOp = IrOpcode::AddI32;
          IrOpcode subOp = IrOpcode::SubI32;
          IrOpcode mulOp = IrOpcode::MulI32;
          if (lerpKind == LocalInfo::ValueKind::Int64 || lerpKind == LocalInfo::ValueKind::UInt64) {
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempStart)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempEnd)});
          if (!emitExpr(expr.args[2], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempT)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempEnd)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempStart)});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempT)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempStart)});
          instructions.push_back({addOp, 0});
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
          IrOpcode mulOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode addOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          if (!emitExpr(expr.args[0], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({mulOp, 0});
          if (!emitExpr(expr.args[2], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({addOp, 0});
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempA)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempB)});

          IrOpcode addOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode mulOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpEqOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;

          auto pushFloatConst = [&](double value) {
            if (aKind == LocalInfo::ValueKind::Float64) {
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

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempA)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempA)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempB)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempB)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

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

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::PushI32, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

          const int32_t iterations = (aKind == LocalInfo::ValueKind::Float64) ? 8 : 6;
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
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return OperatorClampMinMaxTrigEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempY)});

          IrOpcode cmpLtOp = (xKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
          IrOpcode negOp = (xKind == LocalInfo::ValueKind::Float64) ? IrOpcode::NegF64 : IrOpcode::NegF32;

          auto pushFloatZero = [&]() {
            if (xKind == LocalInfo::ValueKind::Float64) {
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

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatZero();
          instructions.push_back({cmpLtOp, 0});
          size_t useNeg = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({negOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAbs)});
          size_t jumpAbsEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});

          size_t usePosIndex = instructions.size();
          instructions[useNeg].imm = static_cast<int32_t>(usePosIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAbs)});

          size_t absEndIndex = instructions.size();
          instructions[jumpAbsEnd].imm = static_cast<int32_t>(absEndIndex);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
          pushFloatZero();
          instructions.push_back({cmpLtOp, 0});
          size_t usePositive = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAbs)});
          instructions.push_back({negOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = instructions.size();
          instructions.push_back({IrOpcode::Jump, 0});

          size_t usePositiveIndex = instructions.size();
          instructions[usePositive].imm = static_cast<int32_t>(usePositiveIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAbs)});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          size_t endIndex = instructions.size();
          instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return OperatorClampMinMaxTrigEmitResult::Handled;
        }
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
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSinSign)});
          pushFloatConst(-1.0);
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});
          size_t skipLowerIndex = instructions.size();
          instructions[skipLower].imm = static_cast<int32_t>(skipLowerIndex);

          int32_t tempX2 = allocTempLocal();
          emitSquare(tempX, tempX2);

          if (trigName == "sin") {
            int32_t tempX3 = allocTempLocal();
            int32_t tempX5 = allocTempLocal();
            int32_t tempX7 = allocTempLocal();
            emitMulLocal(tempX2, tempX, tempX3);
            emitMulLocal(tempX3, tempX2, tempX5);
            emitMulLocal(tempX5, tempX2, tempX7);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
            pushFloatConst(6.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
            pushFloatConst(120.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({addOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
            pushFloatConst(5040.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSinSign)});
            instructions.push_back({mulOp, 0});
            return OperatorClampMinMaxTrigEmitResult::Handled;
          }

          if (trigName == "cos") {
            int32_t tempX4 = allocTempLocal();
            int32_t tempX6 = allocTempLocal();
            emitMulLocal(tempX2, tempX2, tempX4);
            emitMulLocal(tempX4, tempX2, tempX6);

            pushFloatConst(1.0);
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
            pushFloatConst(2.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX4)});
            pushFloatConst(24.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({addOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX6)});
            pushFloatConst(720.0);
            instructions.push_back({divOp, 0});
            instructions.push_back({subOp, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCosSign)});
            instructions.push_back({mulOp, 0});
            return OperatorClampMinMaxTrigEmitResult::Handled;
          }

          // tan(x) ≈ sin(x) / cos(x)
          int32_t tempX3 = allocTempLocal();
          int32_t tempX5 = allocTempLocal();
          int32_t tempX7 = allocTempLocal();
          int32_t tempX4 = allocTempLocal();
          int32_t tempX6 = allocTempLocal();
          int32_t tempSin = allocTempLocal();
          int32_t tempCos = allocTempLocal();

          emitMulLocal(tempX2, tempX, tempX3);
          emitMulLocal(tempX3, tempX2, tempX5);
          emitMulLocal(tempX5, tempX2, tempX7);
          emitMulLocal(tempX2, tempX2, tempX4);
          emitMulLocal(tempX4, tempX2, tempX6);

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
          pushFloatConst(6.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
          pushFloatConst(120.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
          pushFloatConst(5040.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSinSign)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSin)});

          pushFloatConst(1.0);
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
          pushFloatConst(2.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX4)});
          pushFloatConst(24.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({addOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX6)});
          pushFloatConst(720.0);
          instructions.push_back({divOp, 0});
          instructions.push_back({subOp, 0});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCosSign)});
          instructions.push_back({mulOp, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCos)});

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

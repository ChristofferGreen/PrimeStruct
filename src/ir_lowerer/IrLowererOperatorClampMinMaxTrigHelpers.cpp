#include "IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "IrLowererOperatorAngleTrigHelpers.h"

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
  return emitAngleTrigOperatorExpr(
      expr, localsIn, hasMathImport, emitExpr, inferExprKind, allocTempLocal, instructions, error);
}

} // namespace primec::ir_lowerer

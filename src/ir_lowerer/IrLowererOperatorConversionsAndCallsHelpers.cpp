#include "IrLowererOperatorConversionsAndCallsHelpers.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace primec::ir_lowerer {
namespace {

bool isVectorStructPath(const std::string &structPath) {
  return structPath == "/vector" || structPath == "/std/collections/experimental_vector/Vector" ||
         structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool areCompatibleStructPaths(const std::string &lhs, const std::string &rhs) {
  return lhs == rhs || (isVectorStructPath(lhs) && isVectorStructPath(rhs));
}

std::string inferPointerStructTypePath(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return "";
    }
    if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.structTypeName;
    }
    return "";
  }
  if (expr.kind != Expr::Kind::Call) {
    return "";
  }

  std::string memoryBuiltin;
  if (getBuiltinMemoryName(expr, memoryBuiltin)) {
    if (memoryBuiltin == "alloc" && expr.templateArgs.size() == 1) {
      std::string resolvedStruct;
      const std::string targetType = unwrapTopLevelUninitializedTypeText(expr.templateArgs.front());
      if (resolveStructTypeName(targetType, expr.namespacePrefix, resolvedStruct)) {
        return resolvedStruct;
      }
      return "";
    }
    if (memoryBuiltin == "realloc" && expr.args.size() == 2) {
      return inferPointerStructTypePath(expr.args.front(), localsIn, resolveStructTypeName);
    }
    if (memoryBuiltin == "at" && expr.args.size() == 3) {
      return inferPointerStructTypePath(expr.args.front(), localsIn, resolveStructTypeName);
    }
    if (memoryBuiltin == "at_unsafe" && expr.args.size() == 2) {
      return inferPointerStructTypePath(expr.args.front(), localsIn, resolveStructTypeName);
    }
  }

  std::string builtinName;
  if (getBuiltinOperatorName(expr, builtinName) &&
      (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return inferPointerStructTypePath(expr.args.front(), localsIn, resolveStructTypeName);
  }

  if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
    return inferPointerStructTypePath(expr.args.front(), localsIn, resolveStructTypeName);
  }

  return "";
}

bool isAggregatePointerLikeLocal(const LocalInfo &info, bool fromArgsPack) {
  const LocalInfo::Kind kind = fromArgsPack ? info.argsPackElementKind : info.kind;
  if (kind != LocalInfo::Kind::Pointer && kind != LocalInfo::Kind::Reference) {
    return false;
  }
  return !info.structTypeName.empty() || info.referenceToArray || info.pointerToArray || info.referenceToVector ||
         info.pointerToVector || info.referenceToMap || info.pointerToMap || info.referenceToBuffer ||
         info.pointerToBuffer;
}

bool isAggregatePointerLikeExpr(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && isAggregatePointerLikeLocal(it->second, false);
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && isAggregatePointerLikeLocal(it->second, true);
  }

  std::string builtinPointer;
  if (getBuiltinPointerName(expr, builtinPointer) && builtinPointer == "location" && expr.args.size() == 1) {
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(target.name);
    return it != localsIn.end() &&
           (!it->second.structTypeName.empty() || it->second.kind == LocalInfo::Kind::Array ||
            it->second.kind == LocalInfo::Kind::Vector || it->second.kind == LocalInfo::Kind::Map ||
            it->second.kind == LocalInfo::Kind::Buffer);
  }

  return false;
}

} // namespace

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const AllocConversionsAndCallsTempLocalFn &allocTempLocal,
    const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite,
    const EmitConversionsAndCallsPointerIndexOutOfBoundsFn &emitPointerIndexOutOfBounds,
    const EmitConversionsAndCallsArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
    const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName,
    const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount,
    const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo,
    const ResolveConversionsAndCallsStructFieldBindingFn &resolveStructFieldBinding,
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall) {
  handled = true;
  std::string builtin;
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "convert requires exactly one argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs.front();
          LocalInfo::ValueKind targetKind = valueKindFromTypeName(typeName);
          if (targetKind == LocalInfo::ValueKind::Unknown || targetKind == LocalInfo::ValueKind::String) {
            error = "native backend only supports convert<int>, convert<i32>, convert<i64>, convert<u64>, "
                    "convert<bool>, convert<f32>, or convert<f64>";
            return false;
          }
          auto tryEmitMathConstantConvert = [&](const Expr &arg) -> bool {
            if (arg.kind != Expr::Kind::Name) {
              return false;
            }
            std::string mathConst;
            if (!getMathConstantName(arg.name, mathConst)) {
              return false;
            }
            double value = 0.0;
            if (mathConst == "pi") {
              value = 3.14159265358979323846;
            } else if (mathConst == "tau") {
              value = 6.28318530717958647692;
            } else if (mathConst == "e") {
              value = 2.71828182845904523536;
            }
            if (targetKind == LocalInfo::ValueKind::Bool) {
              instructions.push_back({IrOpcode::PushI32, value != 0.0 ? 1u : 0u});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Int32) {
              instructions.push_back(
                  {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Int64) {
              instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              std::memcpy(&bits, &value, sizeof(bits));
              instructions.push_back({IrOpcode::PushF64, bits});
              return true;
            }
            float f32 = static_cast<float>(value);
            uint32_t bits = 0;
            std::memcpy(&bits, &f32, sizeof(bits));
            instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
            return true;
          };
          if (tryEmitMathConstantConvert(expr.args.front())) {
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          if (targetKind == LocalInfo::ValueKind::Bool) {
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (!emitCompareToZero(kind, false)) {
              return false;
            }
            return true;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind == LocalInfo::ValueKind::Bool) {
            argKind = LocalInfo::ValueKind::Int32;
          }
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
            error = "convert requires numeric argument";
            return false;
          }
          bool needsFloatToIntGuard = false;
          LocalInfo::ValueKind floatKind = LocalInfo::ValueKind::Unknown;
          if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
            if (targetKind == LocalInfo::ValueKind::Int32 || targetKind == LocalInfo::ValueKind::Int64 ||
                targetKind == LocalInfo::ValueKind::UInt64) {
              needsFloatToIntGuard = true;
              floatKind = argKind;
            }
          }
          if (argKind == targetKind) {
            return true;
          }
          if (needsFloatToIntGuard) {
            int32_t tempValue = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
            IrOpcode cmpNeOp = (floatKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpNeF64 : IrOpcode::CmpNeF32;
            IrOpcode cmpGtOp = (floatKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpGtF64 : IrOpcode::CmpGtF32;
            IrOpcode cmpLtOp = (floatKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

            auto pushFloatConst = [&](double value) {
              if (floatKind == LocalInfo::ValueKind::Float64) {
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

            const double maxFinite = (floatKind == LocalInfo::ValueKind::Float64) ? 1.7976931348623157e308
                                                                                  : 3.4028234663852886e38;

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({cmpNeOp, 0});
            size_t jumpIfNotNaN = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitFloatToIntNonFinite();
            size_t nanOkIndex = instructions.size();
            instructions[jumpIfNotNaN].imm = static_cast<int32_t>(nanOkIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(maxFinite);
            instructions.push_back({cmpGtOp, 0});
            size_t jumpIfNotPosInf = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitFloatToIntNonFinite();
            size_t posOkIndex = instructions.size();
            instructions[jumpIfNotPosInf].imm = static_cast<int32_t>(posOkIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(-maxFinite);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNotNegInf = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitFloatToIntNonFinite();
            size_t negOkIndex = instructions.size();
            instructions[jumpIfNotNegInf].imm = static_cast<int32_t>(negOkIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          }
          IrOpcode convertOp = IrOpcode::ConvertI32ToF32;
          bool needsConvert = true;
          if (targetKind == LocalInfo::ValueKind::Float32) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              convertOp = IrOpcode::ConvertI32ToF32;
            } else if (argKind == LocalInfo::ValueKind::Int64) {
              convertOp = IrOpcode::ConvertI64ToF32;
            } else if (argKind == LocalInfo::ValueKind::UInt64) {
              convertOp = IrOpcode::ConvertU64ToF32;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToF32;
            } else {
              error = "convert requires numeric argument";
              return false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Float64) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              convertOp = IrOpcode::ConvertI32ToF64;
            } else if (argKind == LocalInfo::ValueKind::Int64) {
              convertOp = IrOpcode::ConvertI64ToF64;
            } else if (argKind == LocalInfo::ValueKind::UInt64) {
              convertOp = IrOpcode::ConvertU64ToF64;
            } else if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToF64;
            } else {
              error = "convert requires numeric argument";
              return false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Int32) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToI32;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToI32;
            } else {
              needsConvert = false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Int64) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToI64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToI64;
            } else {
              needsConvert = false;
            }
          } else if (targetKind == LocalInfo::ValueKind::UInt64) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToU64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToU64;
            } else {
              needsConvert = false;
            }
          }
          if (needsConvert) {
            instructions.push_back({convertOp, 0});
          }
          return true;
        }
        if (getBuiltinMemoryName(expr, builtin)) {
          auto pushIndexConst = [&](LocalInfo::ValueKind kind, int32_t value) {
            instructions.push_back(
                {(kind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64,
                 static_cast<uint64_t>(value)});
          };
          if (builtin == "free") {
            if (!expr.templateArgs.empty()) {
              error = "free does not take template arguments";
              return false;
            }
            if (expr.args.size() != 1) {
              error = "free requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::HeapFree, 0});
            return true;
          }
          if (builtin == "realloc") {
            if (!expr.templateArgs.empty()) {
              error = "realloc does not take template arguments";
              return false;
            }
            if (expr.args.size() != 2) {
              error = "realloc requires exactly two arguments";
              return false;
            }
            const LocalInfo::ValueKind countKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
            if (!isSupportedIndexKind(countKind)) {
              error = "realloc requires integer count argument";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }

            int32_t slotCountMultiplier = 1;
            const std::string structTypeName = inferPointerStructTypePath(expr.args[0], localsIn, resolveStructTypeName);
            if (!structTypeName.empty()) {
              if (!resolveStructSlotCount(structTypeName, slotCountMultiplier)) {
                return false;
              }
            }

            if (slotCountMultiplier != 1) {
              if (countKind == LocalInfo::ValueKind::Int32) {
                instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(slotCountMultiplier)});
                instructions.push_back({IrOpcode::MulI32, 0});
              } else {
                instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotCountMultiplier)});
                instructions.push_back({IrOpcode::MulI64, 0});
              }
            }
            instructions.push_back({IrOpcode::HeapRealloc, 0});
            return true;
          }
          if (builtin == "at") {
            if (!expr.templateArgs.empty()) {
              error = "at does not take template arguments";
              return false;
            }
            if (expr.args.size() != 3) {
              error = "at requires exactly three arguments";
              return false;
            }
            const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              error = "at requires integer index argument";
              return false;
            }
            const LocalInfo::ValueKind countKind = normalizeIndexKind(inferExprKind(expr.args[2], localsIn));
            if (!isSupportedIndexKind(countKind)) {
              error = "at requires integer element count argument";
              return false;
            }
            if (countKind != indexKind) {
              error = "at requires matching integer index and element count kinds";
              return false;
            }

            const int32_t ptrLocal = allocTempLocal();
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

            const int32_t indexLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

            const int32_t countLocal = allocTempLocal();
            if (!emitExpr(expr.args[2], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

            if (indexKind != LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              instructions.push_back({pushZeroForIndex(indexKind), 0});
              instructions.push_back({cmpLtForIndex(indexKind), 0});
              const size_t jumpIndexNonNegative = instructions.size();
              instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitPointerIndexOutOfBounds();
              instructions[jumpIndexNonNegative].imm = static_cast<int32_t>(instructions.size());
            }

            if (countKind != LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
              instructions.push_back({pushZeroForIndex(countKind), 0});
              instructions.push_back({cmpLtForIndex(countKind), 0});
              const size_t jumpCountNonNegative = instructions.size();
              instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitPointerIndexOutOfBounds();
              instructions[jumpCountNonNegative].imm = static_cast<int32_t>(instructions.size());
            }

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
            instructions.push_back({cmpGeForIndex(indexKind), 0});
            const size_t jumpIndexInRange = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitPointerIndexOutOfBounds();
            instructions[jumpIndexInRange].imm = static_cast<int32_t>(instructions.size());

            int32_t slotCountMultiplier = 1;
            const std::string structTypeName = inferPointerStructTypePath(expr.args[0], localsIn, resolveStructTypeName);
            if (!structTypeName.empty()) {
              if (!resolveStructSlotCount(structTypeName, slotCountMultiplier)) {
                return false;
              }
            }

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            pushIndexConst(indexKind, slotCountMultiplier * IrSlotBytesI32);
            instructions.push_back({mulForIndex(indexKind), 0});
            instructions.push_back({IrOpcode::AddI64, 0});
            return true;
          }
          if (builtin == "at_unsafe") {
            if (!expr.templateArgs.empty()) {
              error = "at_unsafe does not take template arguments";
              return false;
            }
            if (expr.args.size() != 2) {
              error = "at_unsafe requires exactly two arguments";
              return false;
            }
            const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              error = "at_unsafe requires integer index argument";
              return false;
            }

            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }

            int32_t slotCountMultiplier = 1;
            const std::string structTypeName = inferPointerStructTypePath(expr.args[0], localsIn, resolveStructTypeName);
            if (!structTypeName.empty()) {
              if (!resolveStructSlotCount(structTypeName, slotCountMultiplier)) {
                return false;
              }
            }

            pushIndexConst(indexKind, slotCountMultiplier * IrSlotBytesI32);
            instructions.push_back({mulForIndex(indexKind), 0});
            instructions.push_back({IrOpcode::AddI64, 0});
            return true;
          }
          if (builtin != "alloc") {
            error = "native backend does not support memory intrinsic: " + builtin;
            return false;
          }
          if (expr.templateArgs.size() != 1) {
            error = "alloc requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "alloc requires exactly one argument";
            return false;
          }
          const LocalInfo::ValueKind countKind = normalizeIndexKind(inferExprKind(expr.args.front(), localsIn));
          if (!isSupportedIndexKind(countKind)) {
            error = "alloc requires integer count argument";
            return false;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }

          int32_t slotCountMultiplier = 1;
          const std::string targetTypeName = unwrapTopLevelUninitializedTypeText(expr.templateArgs.front());
          LocalInfo::ValueKind targetKind = valueKindFromTypeName(targetTypeName);
          if (targetKind == LocalInfo::ValueKind::Unknown) {
            std::string resolvedStructType;
            if (!resolveStructTypeName(targetTypeName, expr.namespacePrefix, resolvedStructType)) {
              error = "native backend does not support alloc target type: " + targetTypeName;
              return false;
            }
            if (!resolveStructSlotCount(resolvedStructType, slotCountMultiplier)) {
              return false;
            }
          }

          if (slotCountMultiplier != 1) {
            if (countKind == LocalInfo::ValueKind::Int32) {
              instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(slotCountMultiplier)});
              instructions.push_back({IrOpcode::MulI32, 0});
            } else {
              instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotCountMultiplier)});
              instructions.push_back({IrOpcode::MulI64, 0});
            }
          }
          instructions.push_back({IrOpcode::HeapAlloc, 0});
          return true;
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (expr.args.size() != 1) {
            error = builtin + " requires exactly one argument";
            return false;
          }
	          if (builtin == "location") {
	            const Expr &target = expr.args.front();
	            auto isBorrowedArgsPackAccessTarget = [&](const Expr &candidate) {
	              const Expr *receiver = nullptr;
	              std::string accessName;
	              if (getBuiltinArrayAccessName(candidate, accessName) &&
	                  candidate.args.size() == 2 &&
	                  (accessName == "at" || accessName == "at_unsafe")) {
	                receiver = &candidate.args.front();
	              } else if (candidate.kind == Expr::Kind::Call &&
	                         candidate.isMethodCall &&
	                         candidate.args.size() == 2 &&
	                         (candidate.name == "at" || candidate.name == "at_unsafe")) {
	                receiver = &candidate.args.front();
	              }
	              if (receiver == nullptr || receiver->kind != Expr::Kind::Name) {
	                return false;
	              }
	              auto it = localsIn.find(receiver->name);
	              return it != localsIn.end() &&
	                     it->second.isArgsPack &&
	                     it->second.argsPackElementKind == LocalInfo::Kind::Reference;
	            };
	            if (target.kind == Expr::Kind::Name) {
	              auto it = localsIn.find(target.name);
	              if (it == localsIn.end()) {
	                error = "location requires a local binding";
	                return false;
	              }
	              if (it->second.kind == LocalInfo::Kind::Reference || !it->second.structTypeName.empty() ||
	                  it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
	                  it->second.isSoaVector ||
	                  it->second.kind == LocalInfo::Kind::Map || it->second.kind == LocalInfo::Kind::Buffer) {
	                instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
	              } else {
	                instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
	              }
	              return true;
	            }
	            if (target.kind == Expr::Kind::Call && isBorrowedArgsPackAccessTarget(target)) {
	              return emitExpr(target, localsIn);
	            }
	            if (target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1) {
	              const Expr &receiver = target.args.front();
	              const std::string receiverStruct = inferStructExprPath(receiver, localsIn);
	              if (receiverStruct.empty()) {
	                error = "field access requires struct receiver";
	                return false;
	              }
	              LayoutFieldBinding fieldBinding;
	              if (resolveStructFieldBinding &&
	                  resolveStructFieldBinding(receiverStruct, target.name, fieldBinding) &&
	                  normalizeBindingTypeName(fieldBinding.typeName) == "Reference") {
	                return emitExpr(target, localsIn);
	              }
	              StructSlotFieldInfo fieldInfo;
	              if (!resolveStructFieldSlot(receiverStruct, target.name, fieldInfo)) {
	                error = "unknown struct field: " + target.name;
	                return false;
	              }
	              if (!emitExpr(receiver, localsIn)) {
	                return false;
	              }
	              const int32_t ptrLocal = allocTempLocal();
	              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
	              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
	              const uint64_t offsetBytes = static_cast<uint64_t>(fieldInfo.slotOffset) * IrSlotBytes;
	              if (offsetBytes != 0) {
	                instructions.push_back({IrOpcode::PushI64, offsetBytes});
	                instructions.push_back({IrOpcode::AddI64, 0});
	              }
	              return true;
	            }
	            if (target.kind == Expr::Kind::Call && !target.isMethodCall && resolveDefinitionCall != nullptr) {
	              const Definition *callee = resolveDefinitionCall(target);
	              if (callee != nullptr) {
	                for (const auto &transform : callee->transforms) {
	                  if (transform.name != "return" || transform.templateArgs.size() != 1) {
	                    continue;
	                  }
	                  std::string base;
	                  std::string arg;
	                  if (splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, arg) &&
	                      normalizeCollectionBindingTypeName(base) == "Reference") {
	                    return emitExpr(target, localsIn);
	                  }
	                }
	              }
	            }
	            error = "location requires a local binding";
	            return false;
	          }
          const Expr &pointerExpr = expr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            auto it = localsIn.find(pointerExpr.name);
            if (it == localsIn.end()) {
              error = "native backend does not know identifier: " + pointerExpr.name;
              return false;
            }
            if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
              error = "dereference requires a pointer or reference";
              return false;
            }
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          } else {
            if (!emitExpr(pointerExpr, localsIn)) {
              return false;
            }
          }
          if (builtin == "dereference" && isAggregatePointerLikeExpr(pointerExpr, localsIn)) {
            return true;
          }
          instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (getBuiltinCollectionName(expr, builtin)) {
          if (builtin == "array" || builtin == "vector" || builtin == "soa_vector") {
            if (expr.templateArgs.size() != 1) {
              error = builtin + " literal requires exactly one template argument";
              return false;
            }
            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = builtin + " literal too large for native backend";
              return false;
            }

            const bool isSoaVector = (builtin == "soa_vector");
            const bool isVectorLike = (builtin == "vector" || isSoaVector);
            LocalInfo::ValueKind elemKind = valueKindFromTypeName(expr.templateArgs.front());
            if (!isSoaVector && elemKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend only supports numeric/bool/string " + builtin + " literals";
              return false;
            }
            if (isSoaVector && !expr.args.empty()) {
              error = "native backend does not support non-empty soa_vector literals";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            const int32_t headerSlots = isVectorLike ? 3 : 1;
            const int32_t literalCount = static_cast<int32_t>(expr.args.size());
            if (isVectorLike && literalCount > kVectorLocalDynamicCapacityLimit) {
              if (isSoaVector) {
                error = "soa_vector literal exceeds local capacity limit (256)";
              } else {
                error = vectorLiteralExceedsLocalCapacityLimitMessage();
              }
              return false;
            }
            const int32_t storageCapacity =
                isVectorLike ? std::max(literalCount, kVectorLocalDynamicCapacityLimit) : literalCount;
            const int32_t dataBaseLocal = baseLocal + headerSlots;
            nextLocal += isVectorLike ? headerSlots : (headerSlots + storageCapacity);

            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(literalCount)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
            if (isVectorLike) {
              instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(literalCount)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
              if (isSoaVector) {
                instructions.push_back({IrOpcode::PushI64, 0});
              } else {
                instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(storageCapacity)});
                instructions.push_back({IrOpcode::HeapAlloc, 0});
              }
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 2)});
            }

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              if (elemKind == LocalInfo::ValueKind::String) {
                int32_t stringIndex = -1;
                size_t length = 0;
                if (!resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
                  error = "native backend requires " + builtin +
                          " literal string elements to be string literals or literal-backed bindings";
                  return false;
                }
                if (isVectorLike) {
                  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(baseLocal + 2)});
                  const uint64_t offsetBytes = static_cast<uint64_t>(i) * IrSlotBytes;
                  if (offsetBytes != 0) {
                    instructions.push_back({IrOpcode::PushI32, offsetBytes});
                    instructions.push_back({IrOpcode::AddI32, 0});
                  }
                  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
                  instructions.push_back({IrOpcode::StoreIndirect, 0});
                } else {
                  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
                  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(dataBaseLocal + static_cast<int32_t>(i))});
                }
                continue;
              }
              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires " + builtin + " literal elements to be numeric/bool values";
                return false;
              }
              if (argKind != elemKind) {
                error = builtin + " literal element type mismatch";
                return false;
              }
              if (isVectorLike) {
                instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(baseLocal + 2)});
                const uint64_t offsetBytes = static_cast<uint64_t>(i) * IrSlotBytes;
                if (offsetBytes != 0) {
                  instructions.push_back({IrOpcode::PushI64, offsetBytes});
                  instructions.push_back({IrOpcode::AddI64, 0});
                }
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                instructions.push_back({IrOpcode::StoreIndirect, 0});
                instructions.push_back({IrOpcode::Pop, 0});
              } else {
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                instructions.push_back(
                    {IrOpcode::StoreLocal, static_cast<uint64_t>(dataBaseLocal + static_cast<int32_t>(i))});
              }
            }

            instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          if (builtin == "map") {
            if (expr.templateArgs.size() != 2) {
              error = "map literal requires exactly two template arguments";
              return false;
            }
            if (expr.args.size() % 2 != 0) {
              error = "map literal requires an even number of arguments";
              return false;
            }

            LocalInfo::ValueKind keyKind = valueKindFromTypeName(expr.templateArgs[0]);
            LocalInfo::ValueKind valueKind = valueKindFromTypeName(expr.templateArgs[1]);
            if (keyKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend only supports numeric/bool map values";
              return false;
            }

            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = "map literal too large for native backend";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += static_cast<int32_t>(1 + expr.args.size());

            const int32_t pairCount = static_cast<int32_t>(expr.args.size() / 2);
            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(pairCount)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              const int32_t slot = baseLocal + 1 + static_cast<int32_t>(i);
              if (i % 2 == 0 && keyKind == LocalInfo::ValueKind::String) {
                int32_t stringIndex = -1;
                size_t length = 0;
                if (resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
                  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
                  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
                  continue;
                }
                const LocalInfo::ValueKind keyArgKind = inferExprKind(arg, localsIn);
                if (keyArgKind != LocalInfo::ValueKind::String) {
                  error = "map literal key type mismatch";
                  return false;
                }
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
                continue;
              }

              if (i % 2 == 1 && valueKind == LocalInfo::ValueKind::String) {
                int32_t stringIndex = -1;
                size_t length = 0;
                if (resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
                  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
                  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
                  continue;
                }
                const LocalInfo::ValueKind valueArgKind = inferExprKind(arg, localsIn);
                if (valueArgKind != LocalInfo::ValueKind::String) {
                  error = "map literal value type mismatch";
                  return false;
                }
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
                continue;
              }

              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires map literal arguments to be numeric/bool values";
                return false;
              }
              LocalInfo::ValueKind expectedKind = (i % 2 == 0) ? keyKind : valueKind;
              if (argKind != expectedKind) {
                error = (i % 2 == 0) ? "map literal key type mismatch" : "map literal value type mismatch";
                return false;
              }
              if (!emitExpr(arg, localsIn)) {
                return false;
              }
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
            }

            instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          error = "native backend does not support " + builtin + " literals";
          return false;
        }
        if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement")) {
          const bool isIncrement = isSimpleCallName(expr, "increment");
          const char *mutateName = isIncrement ? "increment" : "decrement";
          if (expr.args.size() != 1) {
            error = std::string(mutateName) + " requires exactly one argument";
            return false;
          }
          const Expr &target = expr.args.front();
          auto emitDelta = [&](LocalInfo::ValueKind kind) -> bool {
            if (kind == LocalInfo::ValueKind::Int32) {
              instructions.push_back({IrOpcode::PushI32, 1});
              instructions.push_back({isIncrement ? IrOpcode::AddI32 : IrOpcode::SubI32, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::PushI64, 1});
              instructions.push_back({isIncrement ? IrOpcode::AddI64 : IrOpcode::SubI64, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              double one = 1.0;
              std::memcpy(&bits, &one, sizeof(bits));
              instructions.push_back({IrOpcode::PushF64, bits});
              instructions.push_back({isIncrement ? IrOpcode::AddF64 : IrOpcode::SubF64, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Float32) {
              float one = 1.0f;
              uint32_t bits = 0;
              std::memcpy(&bits, &one, sizeof(bits));
              instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
              instructions.push_back({isIncrement ? IrOpcode::AddF32 : IrOpcode::SubF32, 0});
              return true;
            }
            error = std::string(mutateName) + " requires numeric operand";
            return false;
          };
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = std::string(mutateName) + " target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = std::string(mutateName) + " target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              const int32_t ptrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadIndirect, 0});
              if (!emitDelta(it->second.valueKind)) {
                return false;
              }
              const int32_t valueLocal = allocTempLocal();
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::StoreIndirect, 0});
              return true;
            }
            if (it->second.kind != LocalInfo::Kind::Value) {
              error = std::string(mutateName) + " target must be a mutable binding";
              return false;
            }
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            if (!emitDelta(it->second.valueKind)) {
              return false;
            }
            instructions.push_back({IrOpcode::Dup, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            LocalInfo::ValueKind valueKind = inferExprKind(target, localsIn);
            if (valueKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Bool ||
                valueKind == LocalInfo::ValueKind::String) {
              error = std::string(mutateName) + " requires numeric operand";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            const int32_t ptrLocal = allocTempLocal();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = std::string(mutateName) + " target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = std::string(mutateName) + " target must be a mutable pointer binding";
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadIndirect, 0});
            if (!emitDelta(valueKind)) {
              return false;
            }
            const int32_t valueLocal = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::StoreIndirect, 0});
            return true;
          }
          error = std::string(mutateName) + " target must be a mutable binding";
          return false;
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            error = "assign requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "assign target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = "assign target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              const int32_t ptrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              const int32_t valueLocal = allocTempLocal();
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::StoreIndirect, 0});
              return true;
            }
            if (it->second.kind == LocalInfo::Kind::Value && !it->second.structTypeName.empty()) {
              std::string rhsStruct = inferStructExprPath(expr.args[1], localsIn);
              if (rhsStruct.empty() || !areCompatibleStructPaths(rhsStruct, it->second.structTypeName)) {
                error = "assign requires matching struct value";
                return false;
              }
              int32_t structSlotCount = 0;
              if (!resolveStructSlotCount(it->second.structTypeName, structSlotCount)) {
                return false;
              }
              const int32_t destPtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              const int32_t srcPtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
              if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, structSlotCount)) {
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              return true;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::Dup, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            const int32_t ptrLocal = allocTempLocal();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = "assign target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = "assign target must be a mutable pointer binding";
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::StoreIndirect, 0});
            return true;
          }
          std::string accessName;
          if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
            const Expr &collectionTarget = target.args.front();
            const Expr &indexExpr = target.args[1];
            auto targetInfo = resolveArrayVectorAccessTargetInfo(collectionTarget, localsIn);
            if (!targetInfo.isArrayOrVectorTarget) {
              const std::string collectionPath = inferStructExprPath(collectionTarget, localsIn);
              if (collectionPath == "/array" || collectionPath == "/vector") {
                targetInfo.isArrayOrVectorTarget = true;
                targetInfo.isVectorTarget = (collectionPath == "/vector");
              }
            }
            if (!targetInfo.isArrayOrVectorTarget) {
              error = "native backend only supports assign to array/vector elements";
              return false;
            }

            const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              error = "native backend requires integer indices for " + accessName;
              return false;
            }

            const int32_t ptrLocal = allocTempLocal();
            if (!emitExpr(collectionTarget, localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

            const int32_t indexLocal = allocTempLocal();
            if (!emitExpr(indexExpr, localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

            if (accessName == "at") {
              const int32_t countLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadIndirect, 0});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

              if (indexKind != LocalInfo::ValueKind::UInt64) {
                instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
                instructions.push_back({pushZeroForIndex(indexKind), 0});
                instructions.push_back({cmpLtForIndex(indexKind), 0});
                const size_t jumpNonNegative = instructions.size();
                instructions.push_back({IrOpcode::JumpIfZero, 0});
                emitArrayIndexOutOfBounds();
                instructions[jumpNonNegative].imm = static_cast<uint64_t>(instructions.size());
              }

              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
              instructions.push_back({cmpGeForIndex(indexKind), 0});
              const size_t jumpInRange = instructions.size();
              instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              instructions[jumpInRange].imm = static_cast<uint64_t>(instructions.size());
            }

            int32_t basePtrLocal = ptrLocal;
            if (targetInfo.isVectorTarget) {
              basePtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2 * IrSlotBytes)});
              instructions.push_back({IrOpcode::AddI64, 0});
              instructions.push_back({IrOpcode::LoadIndirect, 0});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(basePtrLocal)});
            }

            const int32_t destPtrLocal = allocTempLocal();
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(basePtrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            if (!targetInfo.isVectorTarget) {
              instructions.push_back({pushOneForIndex(indexKind), 1});
              instructions.push_back({addForIndex(indexKind), 0});
            }
            instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
            instructions.push_back({mulForIndex(indexKind), 0});
            instructions.push_back({IrOpcode::AddI64, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::StoreIndirect, 0});
            return true;
          }
          if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
            if (target.args.size() != 1) {
              error = "assign target must be a mutable binding";
              return false;
            }
            const Expr &receiverExpr = target.args.front();
            const std::string receiverStruct = inferStructExprPath(receiverExpr, localsIn);
            if (receiverStruct.empty()) {
              error = "assign target must be a mutable binding";
              return false;
            }
            int32_t fieldSlotOffset = 0;
            int32_t fieldSlotCount = 0;
            std::string fieldStructPath;
            if (!resolveStructFieldInfo(receiverStruct,
                                        target.name,
                                        fieldSlotOffset,
                                        fieldSlotCount,
                                        fieldStructPath)) {
              error = "assign target must be a mutable binding";
              return false;
            }
            auto emitReceiverPointer = [&]() -> bool {
              if (receiverExpr.kind == Expr::Kind::Name) {
                auto it = localsIn.find(receiverExpr.name);
                if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference) {
                  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
                  return true;
                }
              }
              return emitExpr(receiverExpr, localsIn);
            };
            if (!emitReceiverPointer()) {
              return false;
            }
            const int32_t receiverPtrLocal = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(receiverPtrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(receiverPtrLocal)});
            const uint64_t fieldOffsetBytes = static_cast<uint64_t>(fieldSlotOffset) * IrSlotBytes;
            if (fieldOffsetBytes != 0) {
              instructions.push_back({IrOpcode::PushI64, fieldOffsetBytes});
              instructions.push_back({IrOpcode::AddI64, 0});
            }
            const int32_t destPtrLocal = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
            if (!fieldStructPath.empty()) {
              const std::string rhsStruct = inferStructExprPath(expr.args[1], localsIn);
              if (rhsStruct.empty() || !areCompatibleStructPaths(rhsStruct, fieldStructPath)) {
                error = "assign requires matching struct value";
                return false;
              }
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              const int32_t srcPtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
              if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, fieldSlotCount)) {
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
              return true;
            }
            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::StoreIndirect, 0});
            return true;
          }
          error = "native backend only supports assign to local names or dereference";
          return false;
        }
  handled = false;
  return true;
}

} // namespace primec::ir_lowerer

#include "IrLowererOperatorConversionsAndCallsHelpers.h"

#include "IrLowererOperatorConversionsAndCallsInternal.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"

#include <cstring>

namespace primec::ir_lowerer {

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
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const SemanticProductTargetAdapter *semanticProductTargets) {
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
        instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
        return true;
      }
      if (targetKind == LocalInfo::ValueKind::Int64) {
        instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
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

  ConversionsAndCallsOperatorContext context{
      localsIn,
      nextLocal,
      emitExpr,
      inferExprKind,
      emitCompareToZero,
      allocTempLocal,
      emitFloatToIntNonFinite,
      emitPointerIndexOutOfBounds,
      emitArrayIndexOutOfBounds,
      resolveStringTableTarget,
      valueKindFromTypeName,
      getMathConstantName,
      inferStructExprPath,
      resolveStructTypeName,
      resolveStructSlotCount,
      resolveStructFieldInfo,
      resolveStructFieldBinding,
      emitStructCopyFromPtrs,
      instructions,
      error,
      resolveDefinitionCall,
      semanticProductTargets};

  if (!emitConversionsAndCallsMemoryAndPointerExpr(expr, context, handled)) {
    return false;
  }
  if (handled) {
    return true;
  }
  if (!emitConversionsAndCallsCollectionAndMutationExpr(expr, context, handled)) {
    return false;
  }
  if (handled) {
    return true;
  }

  handled = false;
  return true;
}

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
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error) {
  return emitConversionsAndCallsOperatorExpr(
      expr,
      localsIn,
      nextLocal,
      emitExpr,
      inferExprKind,
      emitCompareToZero,
      allocTempLocal,
      emitFloatToIntNonFinite,
      emitPointerIndexOutOfBounds,
      emitArrayIndexOutOfBounds,
      resolveStringTableTarget,
      valueKindFromTypeName,
      getMathConstantName,
      inferStructExprPath,
      resolveStructTypeName,
      resolveStructSlotCount,
      resolveStructFieldInfo,
      [](const std::string &, const std::string &, LayoutFieldBinding &) { return false; },
      emitStructCopyFromPtrs,
      instructions,
      handled,
      error,
      {});
}

} // namespace primec::ir_lowerer

#include "primec/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.h"

#include "IrLowererOperatorConversionsAndCallsInternal.h"

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"

#include <cstring>
#include <utility>

namespace primec::ir_lowerer {

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const ExprLocalsPredicateFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const Int32ProviderFn &allocTempLocal,
    const ActionFn &emitPointerIndexOutOfBounds,
    const ActionFn &emitArrayIndexOutOfBounds,
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
    const SemanticProductTargetAdapter *semanticProductTargets,
    std::string currentScopePath,
    const EmitConversionsAndCallsOwnedStructAssignFn &emitOwnedStructAssign) {
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
      if ((targetKind == LocalInfo::ValueKind::UInt64 || targetKind == LocalInfo::ValueKind::UInt8)) {
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
    if (argKind == targetKind) {
      return true;
    }
    IrOpcode convertOp = IrOpcode::ConvertI32ToF32;
    bool needsConvert = true;
    bool needsSextI32 = false;
    if (targetKind == LocalInfo::ValueKind::Float32) {
      if (argKind == LocalInfo::ValueKind::Int32) {
        convertOp = IrOpcode::ConvertI32ToF32;
      } else if (argKind == LocalInfo::ValueKind::Int64) {
        convertOp = IrOpcode::ConvertI64ToF32;
      } else if ((argKind == LocalInfo::ValueKind::UInt64 || argKind == LocalInfo::ValueKind::UInt8)) {
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
      } else if ((argKind == LocalInfo::ValueKind::UInt64 || argKind == LocalInfo::ValueKind::UInt8)) {
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
        // Narrowing wraps modulo 2^32: the slot form of an i32 is its low 32 bits sign-extended.
        needsSextI32 =
            argKind == LocalInfo::ValueKind::Int64 || argKind == LocalInfo::ValueKind::UInt64;
      }
    } else if (targetKind == LocalInfo::ValueKind::Int64) {
      if (argKind == LocalInfo::ValueKind::Float32) {
        convertOp = IrOpcode::ConvertF32ToI64;
      } else if (argKind == LocalInfo::ValueKind::Float64) {
        convertOp = IrOpcode::ConvertF64ToI64;
      } else {
        needsConvert = false;
      }
    } else if ((targetKind == LocalInfo::ValueKind::UInt64 || targetKind == LocalInfo::ValueKind::UInt8)) {
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
    if (needsSextI32) {
      instructions.push_back({IrOpcode::SextI32, 0});
    }
    const bool floatToByte =
        targetKind == LocalInfo::ValueKind::UInt8 &&
        (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64);
    if (floatToByte) {
      // A float saturates at the byte's range: the u64 conversion already maps a negative float and NaN to 0, so
      // only the upper limit is left.
      const int32_t tempValue = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
      instructions.push_back({IrOpcode::PushI64, 255});
      instructions.push_back({IrOpcode::CmpGtU64, 0});
      const size_t jumpIfInRange = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      instructions.push_back({IrOpcode::PushI64, 255});
      const size_t jumpEnd = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      instructions[jumpIfInRange].imm = static_cast<int32_t>(instructions.size());
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
      instructions[jumpEnd].imm = static_cast<int32_t>(instructions.size());
    }
    if (targetKind == LocalInfo::ValueKind::UInt8) {
      instructions.push_back({IrOpcode::ZextU8, 0});
    }
    return true;
  }

  ConversionsAndCallsOperatorContext context{localsIn,
                                             nextLocal,
                                             emitExpr,
                                             inferExprKind,
                                             emitCompareToZero,
                                             allocTempLocal,
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
                                             semanticProductTargets,
                                             std::move(currentScopePath),
                                             emitOwnedStructAssign};

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
    const ExprLocalsPredicateFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const Int32ProviderFn &allocTempLocal,
    const ActionFn &emitPointerIndexOutOfBounds,
    const ActionFn &emitArrayIndexOutOfBounds,
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

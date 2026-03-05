#include "IrLowererInlineStructArgHelpers.h"

namespace primec::ir_lowerer {

bool emitInlineStructDefinitionArguments(const std::string &calleePath,
                                         const std::vector<const Expr *> &orderedArgs,
                                         const LocalMap &callerLocals,
                                         bool requireValue,
                                         int32_t &nextLocal,
                                         const ResolveInlineStructSlotLayoutFn &resolveStructSlotLayout,
                                         const InferInlineStructExprKindFn &inferExprKind,
                                         const InferInlineStructExprPathFn &inferStructExprPath,
                                         const EmitInlineStructExprFn &emitExpr,
                                         const EmitInlineStructCopySlotsFn &emitStructCopySlots,
                                         const AllocInlineStructTempLocalFn &allocTempLocal,
                                         const EmitInlineStructInstructionFn &emitInstruction,
                                         std::string &error) {
  StructSlotLayoutInfo layout;
  if (!resolveStructSlotLayout(calleePath, layout)) {
    return false;
  }
  if (static_cast<int32_t>(orderedArgs.size()) != static_cast<int32_t>(layout.fields.size())) {
    error = "argument count mismatch";
    return false;
  }
  const int32_t baseLocal = nextLocal;
  if (requireValue) {
    nextLocal += layout.totalSlots;
    emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1)));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));
  }

  for (size_t i = 0; i < layout.fields.size(); ++i) {
    const StructSlotFieldInfo &field = layout.fields[i];
    const Expr *arg = orderedArgs[i];
    if (!arg) {
      error = "argument count mismatch";
      return false;
    }

    if (field.structPath.empty()) {
      LocalInfo::ValueKind argKind = inferExprKind(*arg, callerLocals);
      if (argKind == LocalInfo::ValueKind::Unknown && field.valueKind != LocalInfo::ValueKind::Unknown) {
        argKind = field.valueKind;
      }
      if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
        error = "native backend requires struct field values to be numeric/bool on " + calleePath;
        return false;
      }
      if (argKind != field.valueKind) {
        error = "struct field type mismatch";
        return false;
      }
      if (!emitExpr(*arg, callerLocals)) {
        return false;
      }
      if (requireValue) {
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + field.slotOffset));
      } else {
        emitInstruction(IrOpcode::Pop, 0);
      }
      continue;
    }

    std::string argStruct = inferStructExprPath(*arg, callerLocals);
    if (argStruct.empty() || argStruct != field.structPath) {
      error = "struct field type mismatch";
      return false;
    }
    if (!emitExpr(*arg, callerLocals)) {
      return false;
    }
    if (requireValue) {
      const int32_t srcPtrLocal = allocTempLocal();
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal));
      if (!emitStructCopySlots(baseLocal + field.slotOffset, srcPtrLocal, field.slotCount)) {
        return false;
      }
    } else {
      emitInstruction(IrOpcode::Pop, 0);
    }
  }

  if (requireValue) {
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal));
  }
  return true;
}

} // namespace primec::ir_lowerer

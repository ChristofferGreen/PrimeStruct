#include "IrLowererInlineParamHelpers.h"

namespace primec::ir_lowerer {

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const IsInlineParameterStringBindingFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const EmitInlineParameterExprFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const AllocInlineParameterTempLocalFn &allocTempLocal,
    const EmitInlineParameterInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error) {
  for (size_t i = 0; i < callParams.size(); ++i) {
    const Expr &param = callParams[i];
    const Expr *orderedArg = (i < orderedArgs.size()) ? orderedArgs[i] : nullptr;
    LocalInfo paramInfo;
    paramInfo.index = nextLocal++;
    if (!inferCallParameterLocalInfo(param, paramInfo, error)) {
      return false;
    }

    if (i == packedParamIndex) {
      auto emitArgsPackAlias = [&](const Expr &argExpr) -> bool {
        if (argExpr.kind != Expr::Kind::Name) {
          return false;
        }
        auto callerIt = callerLocals.find(argExpr.name);
        if (callerIt == callerLocals.end() || !callerIt->second.isArgsPack) {
          return false;
        }
        if (callerIt->second.valueKind != paramInfo.valueKind ||
            callerIt->second.structTypeName != paramInfo.structTypeName) {
          error = "variadic parameter type mismatch";
          return false;
        }
        calleeLocals.emplace(param.name, paramInfo);
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(callerIt->second.index));
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
        return true;
      };

      if (packedArgs.size() == 1) {
        const Expr &packedArg = *packedArgs.front();
        if (emitArgsPackAlias(packedArg)) {
          continue;
        }
        if (packedArg.isSpread) {
          error = "native backend requires spread variadic forwarding to use an args<T> parameter";
          return false;
        }
      }
      for (const Expr *packedArg : packedArgs) {
        if (packedArg != nullptr && packedArg->isSpread) {
          error = "native backend does not yet support mixed variadic values with spread forwarding";
          return false;
        }
      }
      if (paramInfo.structTypeName.size() > 0 || paramInfo.valueKind == LocalInfo::ValueKind::Unknown ||
          paramInfo.valueKind == LocalInfo::ValueKind::String) {
        error = "native backend only supports numeric/bool variadic args parameters";
        return false;
      }

      const int32_t baseLocal = nextLocal;
      nextLocal += 1 + static_cast<int32_t>(packedArgs.size());
      emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(packedArgs.size())));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));

      for (size_t packedIndex = 0; packedIndex < packedArgs.size(); ++packedIndex) {
        const Expr &argExpr = *packedArgs[packedIndex];
        LocalInfo::ValueKind argKind = inferExprKind(argExpr, callerLocals);
        if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String ||
            argKind != paramInfo.valueKind) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (!emitExpr(argExpr, callerLocals)) {
          return false;
        }
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1 + static_cast<int32_t>(packedIndex)));
      }

      calleeLocals.emplace(param.name, paramInfo);
      emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      continue;
    }

    if (isStringBinding(param)) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      LocalInfo::StringSource source = LocalInfo::StringSource::None;
      int32_t index = -1;
      bool argvChecked = true;
      if (!emitStringValueForCall(*orderedArg, callerLocals, source, index, argvChecked)) {
        return false;
      }
      paramInfo.valueKind = LocalInfo::ValueKind::String;
      paramInfo.stringSource = source;
      paramInfo.stringIndex = index;
      paramInfo.argvChecked = argvChecked;
      calleeLocals.emplace(param.name, paramInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      continue;
    }

    if (paramInfo.kind == LocalInfo::Kind::Value && paramInfo.isMutable && !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      std::string argStruct = inferStructExprPath(*orderedArg, callerLocals);
      if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
        error = "struct parameter type mismatch";
        return false;
      }
      const Expr &argExpr = *orderedArg;
      auto emitStructPointer = [&](const Expr &arg) -> bool {
        if (arg.kind == Expr::Kind::Name) {
          auto it = callerLocals.find(arg.name);
          if (it != callerLocals.end()) {
            if (it->second.kind == LocalInfo::Kind::Reference) {
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
            if (it->second.kind == LocalInfo::Kind::Value && !it->second.structTypeName.empty()) {
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
          }
        }
        return emitExpr(arg, callerLocals);
      };
      if (!emitStructPointer(argExpr)) {
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      continue;
    }

    if (paramInfo.kind == LocalInfo::Kind::Value && !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      std::string argStruct = inferStructExprPath(*orderedArg, callerLocals);
      if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
        error = "struct parameter type mismatch";
        return false;
      }
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(paramInfo.structTypeName, layout)) {
        return false;
      }
      const int32_t baseLocal = nextLocal;
      nextLocal += layout.totalSlots;
      emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1)));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));
      emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      if (!emitExpr(*orderedArg, callerLocals)) {
        return false;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal));
      if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      continue;
    }

    if (paramInfo.kind == LocalInfo::Kind::Reference && !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      std::string argStruct = inferStructExprPath(*orderedArg, callerLocals);
      if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
        error = "struct parameter type mismatch";
        return false;
      }
      const Expr &argExpr = *orderedArg;
      auto emitStructReference = [&](const Expr &arg) -> bool {
        if (arg.kind == Expr::Kind::Name) {
          auto it = callerLocals.find(arg.name);
          if (it != callerLocals.end()) {
            if (it->second.kind == LocalInfo::Kind::Reference) {
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
            if (it->second.kind == LocalInfo::Kind::Value && !it->second.structTypeName.empty()) {
              emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
          }
        }
        if (!emitExpr(arg, callerLocals)) {
          return false;
        }
        const int32_t tempLocal = allocTempLocal();
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tempLocal));
        emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(tempLocal));
        return true;
      };
      if (!emitStructReference(argExpr)) {
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      continue;
    }

    if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown || paramInfo.valueKind == LocalInfo::ValueKind::String) {
      error = "native backend only supports numeric/bool, string, or struct parameters";
      return false;
    }

    if (!orderedArg) {
      error = "argument count mismatch";
      return false;
    }
    if (!emitExpr(*orderedArg, callerLocals)) {
      return false;
    }
    calleeLocals.emplace(param.name, paramInfo);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
    if (paramInfo.isFileHandle) {
      trackFileHandleLocal(paramInfo.index);
    }
  }
  return true;
}

} // namespace primec::ir_lowerer

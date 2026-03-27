#include "IrLowererInlineParamHelpers.h"
#include "IrLowererInlinePackedArgs.h"

#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"

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
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo) {
  for (size_t i = 0; i < callParams.size(); ++i) {
    const Expr &param = callParams[i];
    const Expr *orderedArg = (i < orderedArgs.size()) ? orderedArgs[i] : nullptr;
    LocalInfo paramInfo;
    paramInfo.index = nextLocal++;
    if (!inferCallParameterLocalInfo(param, paramInfo, error)) {
      return false;
    }

    if (i == packedParamIndex) {
      if (!emitInlinePackedCallParameter(param,
                                         paramInfo,
                                         packedArgs,
                                         callerLocals,
                                         nextLocal,
                                         calleeLocals,
                                         emitStringValueForCall,
                                         inferStructExprPath,
                                         inferExprKind,
                                         resolveStructSlotLayout,
                                         emitExpr,
                                         emitStructCopySlots,
                                         allocTempLocal,
                                         emitInstruction,
                                         error,
                                         inferExprLocalInfo)) {
        return false;
      }
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

    if (paramInfo.kind == LocalInfo::Kind::Value && paramInfo.isMutable &&
        paramInfo.structTypeName.empty() && !paramInfo.isFileHandle &&
        !paramInfo.isFileError && !paramInfo.isResult &&
        paramInfo.valueKind != LocalInfo::ValueKind::Unknown &&
        paramInfo.valueKind != LocalInfo::ValueKind::String) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      const Expr &argExpr = *orderedArg;
      auto emitMutableScalarReference = [&](const Expr &arg) -> bool {
        if (arg.kind == Expr::Kind::Name) {
          auto it = callerLocals.find(arg.name);
          if (it != callerLocals.end() && it->second.structTypeName.empty() &&
              it->second.valueKind == paramInfo.valueKind && !it->second.isFileHandle &&
              !it->second.isFileError && !it->second.isResult) {
            if (it->second.kind == LocalInfo::Kind::Reference &&
                !it->second.referenceToArray && !it->second.referenceToVector &&
                !it->second.referenceToMap && !it->second.referenceToBuffer) {
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
            if (it->second.kind == LocalInfo::Kind::Value) {
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

      if (!emitMutableScalarReference(argExpr)) {
        return false;
      }
      LocalInfo aliasedParamInfo = paramInfo;
      aliasedParamInfo.kind = LocalInfo::Kind::Reference;
      calleeLocals.emplace(param.name, aliasedParamInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(aliasedParamInfo.index));
      continue;
    }

    if (paramInfo.kind == LocalInfo::Kind::Value && paramInfo.isMutable && !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      std::string argStruct = inferStructExprPath(*orderedArg, callerLocals);
      if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
        error = "struct parameter type mismatch: expected " + paramInfo.structTypeName + ", got " +
                (argStruct.empty() ? std::string("<unknown>") : argStruct);
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
      LocalInfo aliasedParamInfo = paramInfo;
      aliasedParamInfo.kind = LocalInfo::Kind::Reference;
      calleeLocals.emplace(param.name, aliasedParamInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(aliasedParamInfo.index));
      continue;
    }

    if (paramInfo.kind == LocalInfo::Kind::Value && !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      std::string argStruct = inferStructExprPath(*orderedArg, callerLocals);
      if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
        error = "struct parameter type mismatch: expected " + paramInfo.structTypeName + ", got " +
                (argStruct.empty() ? std::string("<unknown>") : argStruct);
        return false;
      }
      if (paramInfo.isMutable) {
        if (!emitExpr(*orderedArg, callerLocals)) {
          return false;
        }
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
        calleeLocals.emplace(param.name, paramInfo);
        continue;
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
      if (orderedArg->kind == Expr::Kind::Call) {
        emitDisarmTemporaryStructAfterCopy(emitInstruction, srcPtrLocal, paramInfo.structTypeName);
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
        error = "struct parameter type mismatch: expected " + paramInfo.structTypeName + ", got " +
                (argStruct.empty() ? std::string("<unknown>") : argStruct);
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
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
          }
        }
        // Struct-valued expressions already lower to a pointer to their storage.
        return emitExpr(arg, callerLocals);
      };
      if (!emitStructReference(argExpr)) {
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      continue;
    }

    if (paramInfo.kind == LocalInfo::Kind::Reference &&
        paramInfo.structTypeName.empty() &&
        !paramInfo.referenceToArray && !paramInfo.referenceToVector &&
        !paramInfo.referenceToMap && !paramInfo.referenceToBuffer &&
        !paramInfo.isFileHandle && !paramInfo.isFileError && !paramInfo.isResult &&
        paramInfo.valueKind != LocalInfo::ValueKind::Unknown &&
        paramInfo.valueKind != LocalInfo::ValueKind::String) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      const Expr &argExpr = *orderedArg;
      auto emitScalarReference = [&](const Expr &arg) -> bool {
        if (arg.kind == Expr::Kind::Name) {
          auto it = callerLocals.find(arg.name);
          if (it != callerLocals.end() && it->second.structTypeName.empty() &&
              it->second.valueKind == paramInfo.valueKind && !it->second.isFileHandle &&
              !it->second.isFileError && !it->second.isResult) {
            if (it->second.kind == LocalInfo::Kind::Reference &&
                !it->second.referenceToArray && !it->second.referenceToVector &&
                !it->second.referenceToMap && !it->second.referenceToBuffer) {
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
            if (it->second.kind == LocalInfo::Kind::Value) {
              emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
          }
        }

        if (isSimpleCallName(arg, "location") && arg.args.size() == 1) {
          return emitExpr(arg, callerLocals);
        }

        if (!emitExpr(arg, callerLocals)) {
          return false;
        }
        const int32_t tempLocal = allocTempLocal();
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tempLocal));
        emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(tempLocal));
        return true;
      };

      if (!emitScalarReference(argExpr)) {
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
      continue;
    }

    if (paramInfo.isFileHandle) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      if (orderedArg->kind == Expr::Kind::Name) {
        auto it = callerLocals.find(orderedArg->name);
        if (it != callerLocals.end() && it->second.isFileHandle) {
          LocalInfo aliasedParamInfo = paramInfo;
          aliasedParamInfo.index = it->second.index;
          calleeLocals.emplace(param.name, aliasedParamInfo);
          continue;
        }
      }
    }

    if (paramInfo.kind == LocalInfo::Kind::Value &&
        (paramInfo.valueKind == LocalInfo::ValueKind::Unknown ||
         paramInfo.valueKind == LocalInfo::ValueKind::String)) {
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

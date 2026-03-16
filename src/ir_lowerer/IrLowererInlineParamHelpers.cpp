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
      StructSlotLayoutInfo structLayout;
      const bool isStructPack =
          !paramInfo.structTypeName.empty() && paramInfo.argsPackElementKind == LocalInfo::Kind::Value;
      const bool isWrappedStructPack =
          !paramInfo.structTypeName.empty() &&
          (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
           paramInfo.argsPackElementKind == LocalInfo::Kind::Reference);

      auto emitPackedValueToLocal = [&](const Expr &argExpr, int32_t destLocal) -> bool {
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Array ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Vector ||
            (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
             (paramInfo.referenceToArray || paramInfo.referenceToVector || paramInfo.referenceToMap)) ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Map) {
          if (!emitExpr(argExpr, callerLocals)) {
            return false;
          }
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
          return true;
        }
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
            !paramInfo.referenceToArray && !paramInfo.referenceToVector && !paramInfo.referenceToMap) {
          if (argExpr.kind != Expr::Kind::Name) {
            error = "variadic parameter type mismatch";
            return false;
          }
          auto it = callerLocals.find(argExpr.name);
          if (it == callerLocals.end() || it->second.kind != LocalInfo::Kind::Reference ||
              it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToMap ||
              it->second.isFileHandle != paramInfo.isFileHandle ||
              it->second.isFileError != paramInfo.isFileError ||
              it->second.isResult != paramInfo.isResult ||
              it->second.resultHasValue != paramInfo.resultHasValue ||
              it->second.resultValueKind != paramInfo.resultValueKind ||
              it->second.resultErrorType != paramInfo.resultErrorType ||
              it->second.valueKind != paramInfo.valueKind ||
              it->second.structTypeName != paramInfo.structTypeName) {
            error = "variadic parameter type mismatch";
            return false;
          }
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
          return true;
        }
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer) {
          if (argExpr.kind != Expr::Kind::Name) {
            error = "variadic parameter type mismatch";
            return false;
          }
          auto it = callerLocals.find(argExpr.name);
          if (it == callerLocals.end() || it->second.kind != LocalInfo::Kind::Pointer ||
              it->second.pointerToArray != paramInfo.pointerToArray ||
              it->second.isSoaVector != paramInfo.isSoaVector ||
              it->second.pointerToVector != paramInfo.pointerToVector ||
              it->second.pointerToMap != paramInfo.pointerToMap ||
              it->second.isFileHandle != paramInfo.isFileHandle ||
              it->second.isFileError != paramInfo.isFileError ||
              it->second.isResult != paramInfo.isResult ||
              it->second.resultHasValue != paramInfo.resultHasValue ||
              it->second.resultValueKind != paramInfo.resultValueKind ||
              it->second.resultErrorType != paramInfo.resultErrorType ||
              (paramInfo.pointerToMap &&
               (it->second.mapKeyKind != paramInfo.mapKeyKind ||
                it->second.mapValueKind != paramInfo.mapValueKind)) ||
              it->second.valueKind != paramInfo.valueKind ||
              it->second.structTypeName != paramInfo.structTypeName) {
            error = "variadic parameter type mismatch";
            return false;
          }
          if (!emitExpr(argExpr, callerLocals)) {
            return false;
          }
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
          return true;
        }
        if (paramInfo.valueKind == LocalInfo::ValueKind::String) {
          LocalInfo::StringSource source = LocalInfo::StringSource::None;
          int32_t index = -1;
          bool argvChecked = true;
          if (!emitStringValueForCall(argExpr, callerLocals, source, index, argvChecked)) {
            return false;
          }
        } else {
          LocalInfo::ValueKind argKind = inferExprKind(argExpr, callerLocals);
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String ||
              argKind != paramInfo.valueKind) {
            error = "variadic parameter type mismatch";
            return false;
          }
          if (!emitExpr(argExpr, callerLocals)) {
            return false;
          }
        }
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
        return true;
      };

      auto emitStructPackedValueToSlots = [&](const Expr &argExpr, int32_t destBaseLocal) -> bool {
        const std::string argStruct = inferStructExprPath(argExpr, callerLocals);
        if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (!emitExpr(argExpr, callerLocals)) {
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal));
        return emitStructCopySlots(destBaseLocal, srcPtrLocal, structLayout.totalSlots);
      };

      auto emitArgsPackAlias = [&](const Expr &argExpr,
                                  bool emitAliasInstructions,
                                  LocalInfo *sourceInfoOut = nullptr) -> bool {
        if (argExpr.kind != Expr::Kind::Name) {
          return false;
        }
        auto callerIt = callerLocals.find(argExpr.name);
        if (callerIt == callerLocals.end() || !callerIt->second.isArgsPack) {
          return false;
        }
        if (callerIt->second.valueKind != paramInfo.valueKind ||
            callerIt->second.structTypeName != paramInfo.structTypeName ||
            callerIt->second.argsPackElementKind != paramInfo.argsPackElementKind) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.isSoaVector != paramInfo.isSoaVector) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.isFileHandle != paramInfo.isFileHandle) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.isFileError != paramInfo.isFileError) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.isResult != paramInfo.isResult ||
            callerIt->second.resultHasValue != paramInfo.resultHasValue ||
            callerIt->second.resultValueKind != paramInfo.resultValueKind ||
            callerIt->second.resultErrorType != paramInfo.resultErrorType) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.referenceToArray != paramInfo.referenceToArray ||
            callerIt->second.pointerToArray != paramInfo.pointerToArray ||
            callerIt->second.referenceToVector != paramInfo.referenceToVector ||
            callerIt->second.pointerToVector != paramInfo.pointerToVector ||
            callerIt->second.referenceToMap != paramInfo.referenceToMap ||
            callerIt->second.pointerToMap != paramInfo.pointerToMap) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if ((paramInfo.argsPackElementKind == LocalInfo::Kind::Map ||
             (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference && paramInfo.referenceToMap) ||
             (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer && paramInfo.pointerToMap)) &&
            (callerIt->second.mapKeyKind != paramInfo.mapKeyKind ||
             callerIt->second.mapValueKind != paramInfo.mapValueKind)) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (sourceInfoOut != nullptr) {
          *sourceInfoOut = callerIt->second;
        }
        if (!emitAliasInstructions) {
          return true;
        }
        paramInfo.argsPackElementCount = callerIt->second.argsPackElementCount;
        paramInfo.argsPackElementKind = callerIt->second.argsPackElementKind;
        paramInfo.structSlotCount = callerIt->second.structSlotCount;
        paramInfo.mapKeyKind = callerIt->second.mapKeyKind;
        paramInfo.mapValueKind = callerIt->second.mapValueKind;
        paramInfo.referenceToArray = callerIt->second.referenceToArray;
        paramInfo.pointerToArray = callerIt->second.pointerToArray;
        paramInfo.referenceToVector = callerIt->second.referenceToVector;
        paramInfo.pointerToVector = callerIt->second.pointerToVector;
        paramInfo.referenceToMap = callerIt->second.referenceToMap;
        paramInfo.pointerToMap = callerIt->second.pointerToMap;
        paramInfo.isSoaVector = callerIt->second.isSoaVector;
        paramInfo.isFileHandle = callerIt->second.isFileHandle;
        paramInfo.isFileError = callerIt->second.isFileError;
        paramInfo.isResult = callerIt->second.isResult;
        paramInfo.resultHasValue = callerIt->second.resultHasValue;
        paramInfo.resultValueKind = callerIt->second.resultValueKind;
        paramInfo.resultErrorType = callerIt->second.resultErrorType;
        calleeLocals.emplace(param.name, paramInfo);
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(callerIt->second.index));
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
        return true;
      };

      if (packedArgs.size() == 1) {
        const Expr &packedArg = *packedArgs.front();
        if (emitArgsPackAlias(packedArg, true)) {
          continue;
        }
        if (packedArg.isSpread) {
          if (error.empty()) {
            error = "native backend requires spread variadic forwarding to use an args<T> parameter";
          }
          return false;
        }
      }
      for (const Expr *packedArg : packedArgs) {
        if (packedArg != nullptr && packedArg->isSpread) {
          if (packedArg != packedArgs.back()) {
            error = "native backend does not yet support mixed variadic values with spread forwarding";
            return false;
          }
        }
      }
      if (isStructPack) {
        if (!resolveStructSlotLayout(paramInfo.structTypeName, structLayout)) {
          return false;
        }
        paramInfo.structSlotCount = structLayout.totalSlots;
      } else if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown &&
                 !paramInfo.isSoaVector &&
                 !isWrappedStructPack) {
        error = "native backend only supports numeric/bool/string variadic args parameters";
        return false;
      }

      LocalInfo spreadSourceInfo;
      const bool hasSpreadSuffix = !packedArgs.empty() && packedArgs.back() != nullptr && packedArgs.back()->isSpread;
      const size_t explicitPackedCount = hasSpreadSuffix ? (packedArgs.size() - 1) : packedArgs.size();
      if (hasSpreadSuffix) {
        if (!emitArgsPackAlias(*packedArgs.back(), false, &spreadSourceInfo)) {
          if (error.empty()) {
            error = "native backend requires spread variadic forwarding to use an args<T> parameter";
          }
          return false;
        }
        if (spreadSourceInfo.argsPackElementCount < 0) {
          error = "native backend requires known-size args<T> packs for mixed variadic forwarding";
          return false;
        }
      }

      const int32_t baseLocal = nextLocal;
      const int32_t totalPackedCount =
          static_cast<int32_t>(explicitPackedCount) +
          (hasSpreadSuffix ? spreadSourceInfo.argsPackElementCount : 0);
      const int32_t elementSlotCount = isStructPack ? structLayout.totalSlots : 1;
      nextLocal += 1 + totalPackedCount * elementSlotCount;
      emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(totalPackedCount));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));

      for (size_t packedIndex = 0; packedIndex < explicitPackedCount; ++packedIndex) {
        const Expr &argExpr = *packedArgs[packedIndex];
        const int32_t destLocal = baseLocal + 1 + static_cast<int32_t>(packedIndex) * elementSlotCount;
        if (isStructPack) {
          if (!emitStructPackedValueToSlots(argExpr, destLocal)) {
            return false;
          }
        } else {
          if (!emitPackedValueToLocal(argExpr, destLocal)) {
            return false;
          }
        }
      }

      if (hasSpreadSuffix) {
        const int32_t sourcePtrLocal = allocTempLocal();
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(spreadSourceInfo.index));
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sourcePtrLocal));
        for (int32_t spreadIndex = 0; spreadIndex < spreadSourceInfo.argsPackElementCount; ++spreadIndex) {
          const int32_t destLocal =
              baseLocal + 1 + (static_cast<int32_t>(explicitPackedCount) + spreadIndex) * elementSlotCount;
          const uint64_t offsetBytes =
              static_cast<uint64_t>(1 + spreadIndex * elementSlotCount) * IrSlotBytes;
          if (isStructPack) {
            const int32_t srcElementPtrLocal = allocTempLocal();
            emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(sourcePtrLocal));
            if (offsetBytes != 0) {
              emitInstruction(IrOpcode::PushI64, offsetBytes);
              emitInstruction(IrOpcode::AddI64, 0);
            }
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcElementPtrLocal));
            if (!emitStructCopySlots(destLocal, srcElementPtrLocal, structLayout.totalSlots)) {
              return false;
            }
            continue;
          }
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(sourcePtrLocal));
          if (offsetBytes != 0) {
            emitInstruction(IrOpcode::PushI64, offsetBytes);
            emitInstruction(IrOpcode::AddI64, 0);
          }
          emitInstruction(IrOpcode::LoadIndirect, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
        }
      }

      paramInfo.argsPackElementCount = totalPackedCount;
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

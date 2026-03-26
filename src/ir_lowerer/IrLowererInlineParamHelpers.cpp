#include "IrLowererInlineParamHelpers.h"

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
      StructSlotLayoutInfo structLayout;
      const bool isStructPack =
          !paramInfo.structTypeName.empty() && paramInfo.argsPackElementKind == LocalInfo::Kind::Value;
      const bool isWrappedStructPack =
          !paramInfo.structTypeName.empty() &&
          (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
           paramInfo.argsPackElementKind == LocalInfo::Kind::Reference);

      auto inferDirectLocationTargetInfo = [&](const Expr &argExpr,
                                               LocalInfo &targetInfoOut) -> bool {
        targetInfoOut = LocalInfo{};
        if (!isSimpleCallName(argExpr, "location") || argExpr.args.size() != 1) {
          return false;
        }
        const Expr &targetExpr = argExpr.args.front();
        if (targetExpr.kind == Expr::Kind::Name) {
          auto it = callerLocals.find(targetExpr.name);
          if (it == callerLocals.end()) {
            return false;
          }
          targetInfoOut = it->second;
          return true;
        }
        if (!inferExprLocalInfo) {
          return false;
        }
        return inferExprLocalInfo(targetExpr, callerLocals, targetInfoOut, error);
      };

      const auto matchesWrappedScalarOrStructValueKind = [&](const LocalInfo &info) {
        if (!paramInfo.structTypeName.empty() || !info.structTypeName.empty()) {
          return true;
        }
        return info.valueKind == paramInfo.valueKind;
      };

      const auto matchesResultMetadata = [&](const LocalInfo &info) {
        return info.isResult == paramInfo.isResult &&
               info.resultHasValue == paramInfo.resultHasValue &&
               info.resultValueKind == paramInfo.resultValueKind &&
               info.resultValueCollectionKind == paramInfo.resultValueCollectionKind &&
               info.resultValueMapKeyKind == paramInfo.resultValueMapKeyKind &&
               info.resultValueIsFileHandle == paramInfo.resultValueIsFileHandle &&
               info.resultErrorType == paramInfo.resultErrorType;
      };

      auto matchesWrappedLocationTarget = [&](const LocalInfo &targetInfo,
                                             LocalInfo::Kind expectedKind) -> bool {
        const bool expectsArray =
            expectedKind == LocalInfo::Kind::Reference ? paramInfo.referenceToArray : paramInfo.pointerToArray;
        const bool expectsVector =
            expectedKind == LocalInfo::Kind::Reference ? paramInfo.referenceToVector : paramInfo.pointerToVector;
        const bool expectsMap =
            expectedKind == LocalInfo::Kind::Reference ? paramInfo.referenceToMap : paramInfo.pointerToMap;
        const bool expectsBuffer =
            expectedKind == LocalInfo::Kind::Reference ? paramInfo.referenceToBuffer : paramInfo.pointerToBuffer;
        const bool expectsAggregate = expectsArray || expectsVector || expectsMap || expectsBuffer;
        const auto matchesDirectStorageFlag = [&](const LocalInfo &info) {
          return info.isUninitializedStorage == paramInfo.targetsUninitializedStorage;
        };

        if (targetInfo.kind == LocalInfo::Kind::Pointer) {
          return false;
        }
        if (targetInfo.kind == LocalInfo::Kind::Reference) {
          if (expectsArray) {
            return targetInfo.referenceToArray && targetInfo.valueKind == paramInfo.valueKind;
          }
          if (expectsVector) {
            return targetInfo.referenceToVector && targetInfo.valueKind == paramInfo.valueKind &&
                   targetInfo.structTypeName == paramInfo.structTypeName &&
                   targetInfo.isSoaVector == paramInfo.isSoaVector;
          }
          if (expectsMap) {
            return targetInfo.referenceToMap &&
                   targetInfo.mapKeyKind == paramInfo.mapKeyKind &&
                   targetInfo.mapValueKind == paramInfo.mapValueKind;
          }
          if (expectsBuffer) {
            return targetInfo.referenceToBuffer && targetInfo.valueKind == paramInfo.valueKind;
          }
          if (expectsAggregate) {
            return false;
          }
          return !targetInfo.referenceToArray &&
                 !targetInfo.referenceToVector &&
                 !targetInfo.referenceToMap &&
                 !targetInfo.referenceToBuffer &&
                 targetInfo.referenceToBuffer == paramInfo.referenceToBuffer &&
                 targetInfo.targetsUninitializedStorage == paramInfo.targetsUninitializedStorage &&
                 targetInfo.isFileHandle == paramInfo.isFileHandle &&
                 targetInfo.isFileError == paramInfo.isFileError &&
                 matchesResultMetadata(targetInfo) &&
                 matchesWrappedScalarOrStructValueKind(targetInfo) &&
                 targetInfo.structTypeName == paramInfo.structTypeName;
        }
        if (expectsArray) {
          return targetInfo.kind == LocalInfo::Kind::Array &&
                 matchesDirectStorageFlag(targetInfo) &&
                 targetInfo.valueKind == paramInfo.valueKind;
        }
        if (expectsVector) {
          return targetInfo.kind == LocalInfo::Kind::Vector &&
                 matchesDirectStorageFlag(targetInfo) &&
                 targetInfo.valueKind == paramInfo.valueKind &&
                 targetInfo.structTypeName == paramInfo.structTypeName &&
                 targetInfo.isSoaVector == paramInfo.isSoaVector;
        }
        if (expectsMap) {
          return targetInfo.kind == LocalInfo::Kind::Map &&
                 matchesDirectStorageFlag(targetInfo) &&
                 targetInfo.mapKeyKind == paramInfo.mapKeyKind &&
                 targetInfo.mapValueKind == paramInfo.mapValueKind;
        }
        if (expectsBuffer) {
          return targetInfo.kind == LocalInfo::Kind::Buffer &&
                 matchesDirectStorageFlag(targetInfo) &&
                 targetInfo.valueKind == paramInfo.valueKind;
        }
        if (expectsAggregate) {
          return false;
        }
        return targetInfo.kind == LocalInfo::Kind::Value &&
               matchesDirectStorageFlag(targetInfo) &&
               targetInfo.isFileHandle == paramInfo.isFileHandle &&
               targetInfo.isFileError == paramInfo.isFileError &&
               matchesResultMetadata(targetInfo) &&
               matchesWrappedScalarOrStructValueKind(targetInfo) &&
               targetInfo.structTypeName == paramInfo.structTypeName;
      };

      auto emitPackedValueToLocal = [&](const Expr &argExpr, int32_t destLocal) -> bool {
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Array ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Vector ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Buffer ||
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
          if (argExpr.kind == Expr::Kind::Name) {
            auto it = callerLocals.find(argExpr.name);
            if (it == callerLocals.end() || it->second.kind != LocalInfo::Kind::Reference ||
                it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToMap ||
                it->second.referenceToBuffer != paramInfo.referenceToBuffer ||
                it->second.targetsUninitializedStorage != paramInfo.targetsUninitializedStorage ||
                it->second.isFileHandle != paramInfo.isFileHandle ||
                it->second.isFileError != paramInfo.isFileError ||
                !matchesResultMetadata(it->second) ||
                !matchesWrappedScalarOrStructValueKind(it->second) ||
                it->second.structTypeName != paramInfo.structTypeName) {
              error = "variadic parameter type mismatch";
              return false;
            }
            emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
            return true;
          }
          LocalInfo locationTargetInfo;
          if (inferDirectLocationTargetInfo(argExpr, locationTargetInfo) &&
              matchesWrappedLocationTarget(locationTargetInfo, LocalInfo::Kind::Reference)) {
            if (!emitExpr(argExpr, callerLocals)) {
              return false;
            }
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
            return true;
          }
          if (isSimpleCallName(argExpr, "location") && argExpr.args.size() == 1) {
            if (!emitExpr(argExpr, callerLocals)) {
              return false;
            }
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
            return true;
          }
          error = "variadic parameter type mismatch";
          return false;
        }
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer) {
          if (argExpr.kind == Expr::Kind::Name) {
            auto it = callerLocals.find(argExpr.name);
            if (it == callerLocals.end() || it->second.kind != LocalInfo::Kind::Pointer ||
                it->second.pointerToArray != paramInfo.pointerToArray ||
                it->second.pointerToBuffer != paramInfo.pointerToBuffer ||
                it->second.isSoaVector != paramInfo.isSoaVector ||
                it->second.pointerToVector != paramInfo.pointerToVector ||
                it->second.pointerToMap != paramInfo.pointerToMap ||
                it->second.targetsUninitializedStorage != paramInfo.targetsUninitializedStorage ||
                it->second.isFileHandle != paramInfo.isFileHandle ||
                it->second.isFileError != paramInfo.isFileError ||
                !matchesResultMetadata(it->second) ||
                (paramInfo.pointerToMap &&
                 (it->second.mapKeyKind != paramInfo.mapKeyKind ||
                  it->second.mapValueKind != paramInfo.mapValueKind)) ||
                !matchesWrappedScalarOrStructValueKind(it->second) ||
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
          LocalInfo locationTargetInfo;
          if (inferDirectLocationTargetInfo(argExpr, locationTargetInfo) &&
              matchesWrappedLocationTarget(locationTargetInfo, LocalInfo::Kind::Pointer)) {
            if (!emitExpr(argExpr, callerLocals)) {
              return false;
            }
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
            return true;
          }
          if (isSimpleCallName(argExpr, "location") && argExpr.args.size() == 1) {
            if (!emitExpr(argExpr, callerLocals)) {
              return false;
            }
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
            return true;
          }
          error = "variadic parameter type mismatch";
          return false;
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

      auto validatePackedValueExpr = [&](const Expr &argExpr) -> bool {
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Array ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Vector ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Buffer ||
            (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
             (paramInfo.referenceToArray || paramInfo.referenceToVector || paramInfo.referenceToMap)) ||
            paramInfo.argsPackElementKind == LocalInfo::Kind::Map) {
          return true;
        }
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
            !paramInfo.referenceToArray && !paramInfo.referenceToVector && !paramInfo.referenceToMap) {
          if (argExpr.kind == Expr::Kind::Name) {
            auto it = callerLocals.find(argExpr.name);
            if (it == callerLocals.end() || it->second.kind != LocalInfo::Kind::Reference ||
                it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToMap ||
                it->second.referenceToBuffer != paramInfo.referenceToBuffer ||
                it->second.targetsUninitializedStorage != paramInfo.targetsUninitializedStorage ||
                it->second.isFileHandle != paramInfo.isFileHandle ||
                it->second.isFileError != paramInfo.isFileError ||
                !matchesResultMetadata(it->second) ||
                !matchesWrappedScalarOrStructValueKind(it->second) ||
                it->second.structTypeName != paramInfo.structTypeName) {
              error = "variadic parameter type mismatch";
              return false;
            }
            return true;
          }
          LocalInfo locationTargetInfo;
          if (inferDirectLocationTargetInfo(argExpr, locationTargetInfo) &&
              matchesWrappedLocationTarget(locationTargetInfo, LocalInfo::Kind::Reference)) {
            return true;
          }
          if (isSimpleCallName(argExpr, "location") && argExpr.args.size() == 1) {
            return true;
          }
          error = "variadic parameter type mismatch";
          return false;
        }
        if (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer) {
          if (argExpr.kind == Expr::Kind::Name) {
            auto it = callerLocals.find(argExpr.name);
            if (it == callerLocals.end() || it->second.kind != LocalInfo::Kind::Pointer ||
                it->second.pointerToArray != paramInfo.pointerToArray ||
                it->second.pointerToBuffer != paramInfo.pointerToBuffer ||
                it->second.isSoaVector != paramInfo.isSoaVector ||
                it->second.pointerToVector != paramInfo.pointerToVector ||
                it->second.pointerToMap != paramInfo.pointerToMap ||
                it->second.targetsUninitializedStorage != paramInfo.targetsUninitializedStorage ||
                it->second.isFileHandle != paramInfo.isFileHandle ||
                it->second.isFileError != paramInfo.isFileError ||
                !matchesResultMetadata(it->second) ||
                (paramInfo.pointerToMap &&
                 (it->second.mapKeyKind != paramInfo.mapKeyKind ||
                  it->second.mapValueKind != paramInfo.mapValueKind)) ||
                !matchesWrappedScalarOrStructValueKind(it->second) ||
                it->second.structTypeName != paramInfo.structTypeName) {
              error = "variadic parameter type mismatch";
              return false;
            }
            return true;
          }
          LocalInfo locationTargetInfo;
          if (inferDirectLocationTargetInfo(argExpr, locationTargetInfo) &&
              matchesWrappedLocationTarget(locationTargetInfo, LocalInfo::Kind::Pointer)) {
            return true;
          }
          if (isSimpleCallName(argExpr, "location") && argExpr.args.size() == 1) {
            return true;
          }
          error = "variadic parameter type mismatch";
          return false;
        }
        if (paramInfo.valueKind == LocalInfo::ValueKind::String) {
          return true;
        }
        LocalInfo::ValueKind argKind = inferExprKind(argExpr, callerLocals);
        if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String ||
            argKind != paramInfo.valueKind) {
          error = "variadic parameter type mismatch";
          return false;
        }
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
            callerIt->second.resultValueCollectionKind != paramInfo.resultValueCollectionKind ||
            callerIt->second.resultValueMapKeyKind != paramInfo.resultValueMapKeyKind ||
            callerIt->second.resultValueIsFileHandle != paramInfo.resultValueIsFileHandle ||
            callerIt->second.resultErrorType != paramInfo.resultErrorType) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.targetsUninitializedStorage != paramInfo.targetsUninitializedStorage) {
          error = "variadic parameter type mismatch";
          return false;
        }
        if (callerIt->second.referenceToArray != paramInfo.referenceToArray ||
            callerIt->second.pointerToArray != paramInfo.pointerToArray ||
            callerIt->second.referenceToVector != paramInfo.referenceToVector ||
            callerIt->second.pointerToVector != paramInfo.pointerToVector ||
            callerIt->second.referenceToBuffer != paramInfo.referenceToBuffer ||
            callerIt->second.pointerToBuffer != paramInfo.pointerToBuffer ||
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
        paramInfo.referenceToBuffer = callerIt->second.referenceToBuffer;
        paramInfo.pointerToBuffer = callerIt->second.pointerToBuffer;
        paramInfo.referenceToMap = callerIt->second.referenceToMap;
        paramInfo.pointerToMap = callerIt->second.pointerToMap;
        paramInfo.isSoaVector = callerIt->second.isSoaVector;
        paramInfo.isFileHandle = callerIt->second.isFileHandle;
        paramInfo.isFileError = callerIt->second.isFileError;
        paramInfo.isResult = callerIt->second.isResult;
        paramInfo.resultHasValue = callerIt->second.resultHasValue;
        paramInfo.resultValueKind = callerIt->second.resultValueKind;
        paramInfo.resultValueCollectionKind = callerIt->second.resultValueCollectionKind;
        paramInfo.resultValueMapKeyKind = callerIt->second.resultValueMapKeyKind;
        paramInfo.resultValueIsFileHandle = callerIt->second.resultValueIsFileHandle;
        paramInfo.resultErrorType = callerIt->second.resultErrorType;
        paramInfo.targetsUninitializedStorage = callerIt->second.targetsUninitializedStorage;
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

      const int32_t baseLocal = nextLocal;
      int32_t totalPackedCount = 0;
      std::vector<LocalInfo> spreadSourceInfos;
      spreadSourceInfos.reserve(packedArgs.size());
      for (const Expr *packedArg : packedArgs) {
        if (packedArg == nullptr) {
          continue;
        }
        if (!packedArg->isSpread) {
          if (isStructPack) {
            if (inferStructExprPath(*packedArg, callerLocals) != paramInfo.structTypeName) {
              error = "variadic parameter type mismatch";
              return false;
            }
          } else if (!validatePackedValueExpr(*packedArg)) {
            return false;
          }
          ++totalPackedCount;
          continue;
        }

        LocalInfo spreadSourceInfo;
        if (!emitArgsPackAlias(*packedArg, false, &spreadSourceInfo)) {
          if (error.empty()) {
            error = "native backend requires spread variadic forwarding to use an args<T> parameter";
          }
          return false;
        }
        if (spreadSourceInfo.argsPackElementCount < 0) {
          error = "native backend requires known-size args<T> packs for mixed variadic forwarding";
          return false;
        }
        totalPackedCount += spreadSourceInfo.argsPackElementCount;
        spreadSourceInfos.push_back(spreadSourceInfo);
      }
      const int32_t elementSlotCount = isStructPack ? structLayout.totalSlots : 1;
      nextLocal += 1 + totalPackedCount * elementSlotCount;
      emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(totalPackedCount));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));

      size_t spreadInfoIndex = 0;
      int32_t destElementIndex = 0;
      for (const Expr *packedArg : packedArgs) {
        if (packedArg == nullptr) {
          continue;
        }

        if (!packedArg->isSpread) {
          const int32_t destLocal = baseLocal + 1 + destElementIndex * elementSlotCount;
          if (isStructPack) {
            if (!emitStructPackedValueToSlots(*packedArg, destLocal)) {
              return false;
            }
          } else {
            if (!emitPackedValueToLocal(*packedArg, destLocal)) {
              return false;
            }
          }
          ++destElementIndex;
          continue;
        }

        const LocalInfo &spreadSourceInfo = spreadSourceInfos[spreadInfoIndex++];
        const int32_t sourcePtrLocal = allocTempLocal();
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(spreadSourceInfo.index));
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sourcePtrLocal));
        for (int32_t spreadIndex = 0; spreadIndex < spreadSourceInfo.argsPackElementCount; ++spreadIndex) {
          const int32_t destLocal = baseLocal + 1 + destElementIndex * elementSlotCount;
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
          } else {
            emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(sourcePtrLocal));
            if (offsetBytes != 0) {
              emitInstruction(IrOpcode::PushI64, offsetBytes);
              emitInstruction(IrOpcode::AddI64, 0);
            }
            emitInstruction(IrOpcode::LoadIndirect, 0);
            emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
          }
          ++destElementIndex;
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

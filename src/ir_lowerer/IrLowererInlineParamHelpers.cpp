#include "IrLowererInlineParamHelpers.h"
#include "IrLowererInlinePackedArgs.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/SoaPathHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isCanonicalBuiltinSoaBridgePath(const std::string &calleePath) {
  auto matchesPath = [&](std::string_view path) {
    return calleePath == path ||
           calleePath.rfind(std::string(path) + "__", 0) == 0;
  };
  return matchesPath("/std/collections/soa_vector/count") ||
         matchesPath("/std/collections/soa_vector/get") ||
         matchesPath("/std/collections/soa_vector/get_ref") ||
         matchesPath("/std/collections/soa_vector/ref") ||
         matchesPath("/std/collections/soa_vector/ref_ref") ||
         matchesPath("/std/collections/soa_vector/to_aos") ||
         matchesPath("/std/collections/soa_vector/to_aos_ref");
}

bool isExperimentalSoaVectorStructPath(const std::string &structPath) {
  return soa_paths::isExperimentalSoaVectorSpecializedTypePath(structPath);
}

bool isBuiltinSoaToAosStructMatch(const std::string &calleePath,
                                  const std::string &expectedStruct,
                                  const std::string &argStruct) {
  if (!isExperimentalSoaVectorStructPath(expectedStruct) ||
      isExperimentalSoaVectorStructPath(argStruct)) {
    return false;
  }
  if (normalizeCollectionBindingTypeName(argStruct) != "soa_vector") {
    return false;
  }
  return isCanonicalBuiltinSoaBridgePath(calleePath);
}

bool isKnownStdUiStructAlias(std::string_view bareName) {
  return bareName == "CommandList" || bareName == "HtmlCommandList" ||
         bareName == "LayoutTree" || bareName == "LoginFormNodes" ||
         bareName == "Rgba8" || bareName == "UiEventStream";
}

bool isStdUiStructAliasMatch(const std::string &expectedStruct,
                             const std::string &argStruct) {
  auto matchesBareToQualified = [](const std::string &bare,
                                   const std::string &qualified) {
    return bare.find('/') == std::string::npos &&
           isKnownStdUiStructAlias(bare) &&
           qualified == std::string("/std/ui/") + bare;
  };
  return matchesBareToQualified(expectedStruct, argStruct) ||
         matchesBareToQualified(argStruct, expectedStruct);
}

bool isStructParamMatch(const std::string &calleePath,
                        const std::string &expectedStruct,
                        const std::string &argStruct) {
  return expectedStruct == argStruct ||
         isBuiltinSoaToAosStructMatch(calleePath, expectedStruct, argStruct) ||
         isStdUiStructAliasMatch(expectedStruct, argStruct);
}

bool resolveBuiltinSoaToAosStorageField(const StructSlotLayoutInfo &layout,
                                        StructSlotFieldInfo &fieldOut) {
  if (resolveStructSlotFieldByName(layout.fields, "storage", fieldOut)) {
    return true;
  }
  if (layout.fields.size() == 1 && layout.fields.front().slotCount >= 5) {
    fieldOut = layout.fields.front();
    return true;
  }
  return false;
}

void emitCopyBuiltinSoaToAosSlotToLocal(
    int32_t destLocal,
    int32_t srcPtrLocal,
    int32_t srcSlotOffset,
    const EmitInlineParameterInstructionFn &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal));
  if (srcSlotOffset != 0) {
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(srcSlotOffset) * IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
  }
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destLocal));
}

bool emitBuiltinSoaToAosStructBridge(
    int32_t destBaseLocal,
    int32_t srcPtrLocal,
    const StructSlotLayoutInfo &layout,
    const EmitInlineParameterInstructionFn &emitInstruction,
    std::string &error) {
  StructSlotFieldInfo storageField;
  if (!resolveBuiltinSoaToAosStorageField(layout, storageField) || storageField.slotCount < 5) {
    error = "internal error: builtin soa_vector to_aos bridge requires SoaVector storage layout";
    return false;
  }

  // Struct locals store their payload-slot count in the leading header slot.
  // The experimental wrapper path expects a concrete SoaVector<T> header first,
  // then the flattened nested SoaColumn<T> field starting at its slot offset.
  emitInstruction(IrOpcode::PushI32,
                  static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1)));
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destBaseLocal));

  const int32_t storageBaseLocal = destBaseLocal + storageField.slotOffset;
  // Nested struct fields include their own leading slot-count header. Seed that
  // header explicitly, then copy the builtin soa_vector payload slots after the
  // builtin vector's own header.
  emitInstruction(IrOpcode::PushI32,
                  static_cast<uint64_t>(static_cast<int32_t>(storageField.slotCount - 1)));
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(storageBaseLocal));
  emitCopyBuiltinSoaToAosSlotToLocal(storageBaseLocal + 1, srcPtrLocal, 1, emitInstruction);
  emitCopyBuiltinSoaToAosSlotToLocal(storageBaseLocal + 2, srcPtrLocal, 2, emitInstruction);
  emitCopyBuiltinSoaToAosSlotToLocal(storageBaseLocal + 3, srcPtrLocal, 3, emitInstruction);
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(storageBaseLocal + 4));
  return true;
}

bool isBuiltinMapConstructorExpr(const Expr &callExpr) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  std::string collectionName;
  if (getBuiltinCollectionName(callExpr, collectionName) && collectionName == "map") {
    return true;
  }
  return isPublishedStdlibSurfaceConstructorExpr(
      callExpr,
      primec::StdlibSurfaceId::CollectionsMapConstructors);
}

std::string extractParameterTypeName(const Expr &paramExpr) {
  for (const auto &transform : paramExpr.transforms) {
    if (transform.name == "mut" || transform.name == "public" ||
        transform.name == "private" || transform.name == "static" ||
        transform.name == "shared" || transform.name == "placement" ||
        transform.name == "align" || transform.name == "packed" ||
        transform.name == "reflection" || transform.name == "effects" ||
        transform.name == "capabilities") {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    std::string typeName = transform.name;
    if (!transform.templateArgs.empty()) {
      typeName += "<";
      for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
        if (index != 0) {
          typeName += ", ";
        }
        typeName += trimTemplateTypeText(transform.templateArgs[index]);
      }
      typeName += ">";
    }
    return typeName;
  }
  return {};
}

bool rewriteBuiltinMapConstructorExpr(const Expr &callExpr,
                                      LocalInfo::ValueKind fallbackKeyKind,
                                      LocalInfo::ValueKind fallbackValueKind,
                                      const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
                                      Expr &rewrittenExpr) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(callExpr) : nullptr;
  const bool isResolvedExperimentalConstructor = [&]() {
    return callee != nullptr &&
           isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(
               callee->fullPath,
               primec::StdlibSurfaceId::CollectionsMapConstructors);
  }();
  if (!isBuiltinMapConstructorExpr(callExpr) && !isResolvedExperimentalConstructor) {
    return false;
  }
  rewrittenExpr = callExpr;
  rewrittenExpr.name = "/map/map";
  rewrittenExpr.namespacePrefix.clear();
  rewrittenExpr.isMethodCall = false;
  rewrittenExpr.semanticNodeId = 0;
  if (rewrittenExpr.templateArgs.empty() &&
      fallbackKeyKind != LocalInfo::ValueKind::Unknown &&
      fallbackValueKind != LocalInfo::ValueKind::Unknown) {
    rewrittenExpr.templateArgs = {
        typeNameForValueKind(fallbackKeyKind),
        typeNameForValueKind(fallbackValueKind),
    };
  } else if (rewrittenExpr.templateArgs.empty() && callee != nullptr &&
             callee->parameters.size() >= 2) {
    const std::string keyTypeName = extractParameterTypeName(callee->parameters[0]);
    const std::string valueTypeName = extractParameterTypeName(callee->parameters[1]);
    if (!keyTypeName.empty() && !valueTypeName.empty()) {
      rewrittenExpr.templateArgs = {keyTypeName, valueTypeName};
    }
  }
  return true;
}

bool resolveIdentityReferenceForwardParamIndex(const Definition &callee, size_t &indexOut) {
  indexOut = 0;
  if (!callee.returnExpr.has_value() || !callee.statements.empty()) {
    return false;
  }
  const Expr &returnExpr = *callee.returnExpr;
  if (returnExpr.kind != Expr::Kind::Name) {
    return false;
  }
  for (size_t i = 0; i < callee.parameters.size(); ++i) {
    if (callee.parameters[i].name == returnExpr.name) {
      indexOut = i;
      return true;
    }
  }
  return false;
}

bool peelMapReferenceReceiverExpr(const Expr &receiverExpr,
                                  const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
                                  Expr &peeledOut) {
  const Expr *current = &receiverExpr;
  bool changed = false;
  for (size_t peelSteps = 0; peelSteps < 8; ++peelSteps) {
    if (current->kind != Expr::Kind::Call) {
      break;
    }
    if ((isSimpleCallName(*current, "location") || isSimpleCallName(*current, "dereference")) &&
        current->args.size() == 1) {
      current = &current->args.front();
      changed = true;
      continue;
    }
    if (current->isMethodCall || !resolveDefinitionCall) {
      break;
    }
    const Definition *callee = resolveDefinitionCall(*current);
    size_t forwardedParamIndex = 0;
    if (callee == nullptr || !resolveIdentityReferenceForwardParamIndex(*callee, forwardedParamIndex) ||
        forwardedParamIndex >= current->args.size()) {
      break;
    }
    current = &current->args[forwardedParamIndex];
    changed = true;
  }
  if (!changed) {
    return false;
  }
  peeledOut = *current;
  return true;
}

bool rewriteMapReferenceFieldAccessReceiverExpr(const Expr &argExpr,
                                                const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
                                                Expr &rewrittenExpr) {
  if (argExpr.kind != Expr::Kind::Call || !argExpr.isFieldAccess || argExpr.args.size() != 1) {
    return false;
  }
  Expr peeledReceiver;
  if (!peelMapReferenceReceiverExpr(argExpr.args.front(), resolveDefinitionCall, peeledReceiver)) {
    return false;
  }
  rewrittenExpr = argExpr;
  rewrittenExpr.args.front() = std::move(peeledReceiver);
  return true;
}

bool isMapLikeStructTypeName(const std::string &structTypeName) {
  if (structTypeName.empty()) {
    return false;
  }
  return normalizeCollectionBindingTypeName(structTypeName) == "map" ||
         structTypeName == "/std/collections/experimental_map/Map" ||
         structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

bool shouldRewriteMapReferenceReceiverForParam(const LocalInfo &paramInfo) {
  if (paramInfo.structTypeName.empty()) {
    return paramInfo.kind == LocalInfo::Kind::Map ||
           ((paramInfo.kind == LocalInfo::Kind::Reference || paramInfo.kind == LocalInfo::Kind::Pointer) &&
            (paramInfo.referenceToMap || paramInfo.pointerToMap));
  }
  if (paramInfo.kind == LocalInfo::Kind::Map) {
    return true;
  }
  if (paramInfo.kind == LocalInfo::Kind::Value || paramInfo.kind == LocalInfo::Kind::Reference ||
      paramInfo.kind == LocalInfo::Kind::Pointer) {
    return isMapLikeStructTypeName(paramInfo.structTypeName);
  }
  return false;
}

bool isDirectSequentialHandle(const LocalInfo &info) {
  return info.kind == LocalInfo::Kind::Array ||
         info.kind == LocalInfo::Kind::Vector ||
         info.kind == LocalInfo::Kind::Buffer;
}

bool sequentialHandleKindsCompatible(const LocalInfo &paramInfo,
                                     const LocalInfo &argInfo) {
  if (!isDirectSequentialHandle(paramInfo) || !isDirectSequentialHandle(argInfo)) {
    return false;
  }
  if (paramInfo.kind != argInfo.kind) {
    return false;
  }
  return paramInfo.valueKind == LocalInfo::ValueKind::Unknown ||
         argInfo.valueKind == LocalInfo::ValueKind::Unknown ||
         paramInfo.valueKind == argInfo.valueKind;
}

void preserveSequentialHandleArgumentInfo(LocalInfo &paramInfo,
                                          const LocalInfo &argInfo) {
  const int32_t paramIndex = paramInfo.index;
  const bool paramIsMutable = paramInfo.isMutable;
  const bool paramIsArgsPack = paramInfo.isArgsPack;
  const int32_t paramArgsPackElementCount = paramInfo.argsPackElementCount;

  if (argInfo.valueKind != LocalInfo::ValueKind::Unknown) {
    paramInfo.valueKind = argInfo.valueKind;
  }
  if (!argInfo.structTypeName.empty()) {
    paramInfo.structTypeName = argInfo.structTypeName;
  }
  if (argInfo.structSlotCount > 0) {
    paramInfo.structSlotCount = argInfo.structSlotCount;
  }
  paramInfo.isSoaVector = paramInfo.isSoaVector || argInfo.isSoaVector;
  paramInfo.usesBuiltinCollectionLayout =
      paramInfo.usesBuiltinCollectionLayout || argInfo.usesBuiltinCollectionLayout;
  paramInfo.index = paramIndex;
  paramInfo.isMutable = paramIsMutable;
  paramInfo.isArgsPack = paramIsArgsPack;
  paramInfo.argsPackElementCount = paramArgsPackElementCount;
}

} // namespace

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
  return emitInlineDefinitionCallParameters(callParams,
                                            orderedArgs,
                                            packedArgs,
                                            packedParamIndex,
                                            std::string(),
                                            callerLocals,
                                            nextLocal,
                                            calleeLocals,
                                            inferCallParameterLocalInfo,
                                            isStringBinding,
                                            emitStringValueForCall,
                                            inferStructExprPath,
                                            inferExprKind,
                                            {},
                                            resolveStructSlotLayout,
                                            emitExpr,
                                            emitStructCopySlots,
                                            allocTempLocal,
                                            emitInstruction,
                                            trackFileHandleLocal,
                                            error,
                                            inferExprLocalInfo);
}

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const std::string &calleePath,
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
  return emitInlineDefinitionCallParameters(callParams,
                                            orderedArgs,
                                            packedArgs,
                                            packedParamIndex,
                                            calleePath,
                                            callerLocals,
                                            nextLocal,
                                            calleeLocals,
                                            inferCallParameterLocalInfo,
                                            isStringBinding,
                                            emitStringValueForCall,
                                            inferStructExprPath,
                                            inferExprKind,
                                            {},
                                            resolveStructSlotLayout,
                                            emitExpr,
                                            emitStructCopySlots,
                                            allocTempLocal,
                                            emitInstruction,
                                            trackFileHandleLocal,
                                            error,
                                            inferExprLocalInfo);
}

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
    const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const EmitInlineParameterExprFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const AllocInlineParameterTempLocalFn &allocTempLocal,
    const EmitInlineParameterInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo) {
  return emitInlineDefinitionCallParameters(callParams,
                                            orderedArgs,
                                            packedArgs,
                                            packedParamIndex,
                                            std::string(),
                                            callerLocals,
                                            nextLocal,
                                            calleeLocals,
                                            inferCallParameterLocalInfo,
                                            isStringBinding,
                                            emitStringValueForCall,
                                            inferStructExprPath,
                                            inferExprKind,
                                            resolveDefinitionCall,
                                            resolveStructSlotLayout,
                                            emitExpr,
                                            emitStructCopySlots,
                                            allocTempLocal,
                                            emitInstruction,
                                            trackFileHandleLocal,
                                            error,
                                            inferExprLocalInfo);
}

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const std::string &calleePath,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const IsInlineParameterStringBindingFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
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
    if (!inferCallParameterLocalInfo(param, paramInfo, error)) {
      return false;
    }
    if (paramInfo.isFileHandle) {
      paramInfo.structTypeName.clear();
    }
    if (orderedArg != nullptr && inferExprLocalInfo &&
        paramInfo.kind == LocalInfo::Kind::Value &&
        paramInfo.valueKind == LocalInfo::ValueKind::Unknown &&
        paramInfo.structTypeName.empty() &&
        !paramInfo.isFileHandle && !paramInfo.isFileError && !paramInfo.isResult) {
      LocalInfo inferredArgInfo;
      std::string inferredArgError;
      if (!inferExprLocalInfo(*orderedArg, callerLocals, inferredArgInfo, inferredArgError)) {
        error = inferredArgError;
        return false;
      }
      if (inferredArgInfo.kind != LocalInfo::Kind::Value ||
          !inferredArgInfo.structTypeName.empty() ||
          inferredArgInfo.isFileHandle || inferredArgInfo.isFileError ||
          inferredArgInfo.isResult) {
        const bool paramIsMutable = paramInfo.isMutable;
        const bool paramIsArgsPack = paramInfo.isArgsPack;
        const int32_t paramArgsPackElementCount = paramInfo.argsPackElementCount;
        paramInfo = inferredArgInfo;
        paramInfo.index = 0;
        paramInfo.isMutable = paramIsMutable;
        paramInfo.isArgsPack = paramIsArgsPack;
        paramInfo.argsPackElementCount = paramArgsPackElementCount;
      }
    }
    if (orderedArg != nullptr && inferExprLocalInfo &&
        (paramInfo.isSoaVector ||
         (paramInfo.kind == LocalInfo::Kind::Reference && paramInfo.referenceToVector) ||
         (paramInfo.kind == LocalInfo::Kind::Pointer && paramInfo.pointerToVector))) {
      LocalInfo inferredArgInfo;
      std::string inferredArgError;
      if (!inferExprLocalInfo(*orderedArg, callerLocals, inferredArgInfo, inferredArgError)) {
        error = inferredArgError;
        return false;
      }
      if (inferredArgInfo.usesBuiltinCollectionLayout && inferredArgInfo.isSoaVector) {
        paramInfo.usesBuiltinCollectionLayout = true;
      }
    }
    const bool isPackedParam =
        i == packedParamIndex &&
        (paramInfo.isArgsPack || !packedArgs.empty() || orderedArg == nullptr);
    const bool reserveIndexEarly =
        isPackedParam ||
        paramInfo.kind != LocalInfo::Kind::Map ||
        !paramInfo.structTypeName.empty();
    if (reserveIndexEarly) {
      paramInfo.index = nextLocal++;
    } else {
      paramInfo.index = -1;
    }

    if (isPackedParam) {
      if (!emitInlinePackedCallParameter(param,
                                         paramInfo,
                                         packedArgs,
                                         callerLocals,
                                         nextLocal,
                                         calleeLocals,
                                         emitStringValueForCall,
                                         inferStructExprPath,
                                         inferExprKind,
                                         resolveDefinitionCall,
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

    if (orderedArg != nullptr && orderedArg->kind == Expr::Kind::Name &&
        isDirectSequentialHandle(paramInfo)) {
      auto argIt = callerLocals.find(orderedArg->name);
      if (argIt != callerLocals.end() &&
          sequentialHandleKindsCompatible(paramInfo, argIt->second)) {
        preserveSequentialHandleArgumentInfo(paramInfo, argIt->second);
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(argIt->second.index));
        calleeLocals.emplace(param.name, paramInfo);
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
        continue;
      }
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
      if (paramInfo.kind == LocalInfo::Kind::Map) {
        aliasedParamInfo.referenceToMap = true;
      }
      calleeLocals.emplace(param.name, aliasedParamInfo);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(aliasedParamInfo.index));
      continue;
    }

    if ((paramInfo.kind == LocalInfo::Kind::Value ||
         paramInfo.kind == LocalInfo::Kind::Map) &&
        paramInfo.isMutable && !paramInfo.isFileHandle &&
        !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      Expr rewrittenMapReferenceArgExpr;
      const Expr *argExprPtr = orderedArg;
      if (shouldRewriteMapReferenceReceiverForParam(paramInfo) &&
          orderedArg->kind == Expr::Kind::Call &&
          rewriteMapReferenceFieldAccessReceiverExpr(
              *orderedArg, resolveDefinitionCall, rewrittenMapReferenceArgExpr)) {
        argExprPtr = &rewrittenMapReferenceArgExpr;
      }
      std::string argStruct = inferStructExprPath(*argExprPtr, callerLocals);
      if (argStruct.empty() ||
          !isStructParamMatch(calleePath, paramInfo.structTypeName, argStruct)) {
        error = "struct parameter type mismatch: expected " + paramInfo.structTypeName + ", got " +
                (argStruct.empty() ? std::string("<unknown>") : argStruct);
        return false;
      }
      const Expr &argExpr = *argExprPtr;
      auto emitStructPointer = [&](const Expr &arg) -> bool {
        if (arg.kind == Expr::Kind::Name) {
          auto it = callerLocals.find(arg.name);
          if (it != callerLocals.end()) {
            if (it->second.kind == LocalInfo::Kind::Reference) {
              emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            }
            if ((it->second.kind == LocalInfo::Kind::Value ||
                 it->second.kind == LocalInfo::Kind::Map) &&
                !it->second.structTypeName.empty()) {
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

    if ((paramInfo.kind == LocalInfo::Kind::Value ||
         paramInfo.kind == LocalInfo::Kind::Map) &&
        !paramInfo.isMutable && !paramInfo.isFileHandle &&
        !paramInfo.structTypeName.empty() && orderedArg != nullptr &&
        orderedArg->kind == Expr::Kind::Name) {
      auto argIt = callerLocals.find(orderedArg->name);
      if (argIt != callerLocals.end() &&
          argIt->second.kind == LocalInfo::Kind::Map &&
          isStructParamMatch(calleePath, paramInfo.structTypeName, argIt->second.structTypeName) &&
          (paramInfo.mapKeyKind == LocalInfo::ValueKind::Unknown ||
           paramInfo.mapKeyKind == argIt->second.mapKeyKind) &&
          (paramInfo.mapValueKind == LocalInfo::ValueKind::Unknown ||
           paramInfo.mapValueKind == argIt->second.mapValueKind)) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(argIt->second.index));
        calleeLocals.emplace(param.name, paramInfo);
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index));
        continue;
      }
    }

    if ((paramInfo.kind == LocalInfo::Kind::Value ||
         paramInfo.kind == LocalInfo::Kind::Map) &&
        !paramInfo.isFileHandle && !paramInfo.structTypeName.empty()) {
      if (!orderedArg) {
        error = "argument count mismatch";
        return false;
      }
      std::string argStruct = inferStructExprPath(*orderedArg, callerLocals);
      const bool builtinSoaToAosStructBridge =
          isBuiltinSoaToAosStructMatch(calleePath, paramInfo.structTypeName, argStruct);
      if (argStruct.empty() ||
          !isStructParamMatch(calleePath, paramInfo.structTypeName, argStruct)) {
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
      const int32_t srcPtrLocal = allocTempLocal();
      bool emittedStructArgsPackAccessArg = false;
      std::string accessName;
      if (orderedArg->kind == Expr::Kind::Call &&
          getBuiltinArrayAccessName(*orderedArg, accessName) &&
          orderedArg->args.size() == 2) {
        const auto targetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(orderedArg->args.front(), callerLocals);
        const bool isStructArgsPackAccess =
            targetInfo.isArgsPackTarget &&
            !targetInfo.isVectorTarget &&
            !targetInfo.structTypeName.empty() &&
            targetInfo.elemSlotCount > 0;
        if (isStructArgsPackAccess) {
          if (!ir_lowerer::emitArrayVectorIndexedAccess(
                  accessName,
                  orderedArg->args.front(),
                  orderedArg->args[1],
                  callerLocals,
                  [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                    return inferExprKind(valueExpr, valueLocals);
                  },
                  [&]() { return allocTempLocal(); },
                  [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                    return emitExpr(valueExpr, valueLocals);
                  },
                  []() {},
                  []() { return 0; },
                  emitInstruction,
                  [](size_t, uint64_t) {},
                  error)) {
            return false;
          }
          emittedStructArgsPackAccessArg = true;
        }
      }
      if (!emittedStructArgsPackAccessArg && !emitExpr(*orderedArg, callerLocals)) {
        return false;
      }
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal));
      if (builtinSoaToAosStructBridge) {
        if (!emitBuiltinSoaToAosStructBridge(baseLocal,
                                             srcPtrLocal,
                                             layout,
                                             emitInstruction,
                                             error)) {
          return false;
        }
      } else if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
        return false;
      }
      if (!builtinSoaToAosStructBridge && orderedArg->kind == Expr::Kind::Call) {
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
      if (argStruct.empty() ||
          !isStructParamMatch(calleePath, paramInfo.structTypeName, argStruct)) {
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
            if ((it->second.kind == LocalInfo::Kind::Value ||
                 it->second.kind == LocalInfo::Kind::Map) &&
                !it->second.structTypeName.empty()) {
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
    Expr rewrittenMapArgExpr;
    const Expr *emittedArgExpr = orderedArg;
    Expr rewrittenMapReferenceArgExpr;
    if (paramInfo.kind == LocalInfo::Kind::Map &&
        paramInfo.structTypeName.empty() &&
        orderedArg->kind == Expr::Kind::Call &&
        rewriteBuiltinMapConstructorExpr(
            *orderedArg, paramInfo.mapKeyKind, paramInfo.mapValueKind, resolveDefinitionCall, rewrittenMapArgExpr)) {
      emittedArgExpr = &rewrittenMapArgExpr;
    } else if (shouldRewriteMapReferenceReceiverForParam(paramInfo) &&
               orderedArg->kind == Expr::Kind::Call &&
               rewriteMapReferenceFieldAccessReceiverExpr(
                   *orderedArg, resolveDefinitionCall, rewrittenMapReferenceArgExpr)) {
      emittedArgExpr = &rewrittenMapReferenceArgExpr;
    }
    if (!emitExpr(*emittedArgExpr, callerLocals)) {
      return false;
    }
    if (paramInfo.index < 0) {
      paramInfo.index = nextLocal++;
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

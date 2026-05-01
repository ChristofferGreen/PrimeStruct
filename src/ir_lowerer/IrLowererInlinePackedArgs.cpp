#include "IrLowererInlinePackedArgs.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isBuiltinMapConstructorExpr(const Expr &callExpr) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
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

bool extractArgsPackElementTypeName(const Expr &paramExpr, std::string &typeNameOut) {
  typeNameOut.clear();
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
    if (transform.name != "args" || transform.templateArgs.size() != 1) {
      return false;
    }
    typeNameOut = trimTemplateTypeText(transform.templateArgs.front());
    return !typeNameOut.empty();
  }
  return false;
}

bool namesExplicitExperimentalMapType(const std::string &typeText) {
  std::string base;
  std::string argList;
  if (splitTemplateTypeName(typeText, base, argList)) {
    base = trimTemplateTypeText(base);
  } else {
    base = trimTemplateTypeText(typeText);
  }
  if (!base.empty() && base.front() == '/') {
    base.erase(base.begin());
  }
  return base == "Map" ||
         base == "std/collections/experimental_map/Map" ||
         base.rfind("std/collections/experimental_map/Map__", 0) == 0;
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

} // namespace

bool emitInlinePackedCallParameter(
    const Expr &param,
    LocalInfo &paramInfo,
    const std::vector<const Expr *> &packedArgs,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const EmitInlineParameterExprFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const AllocInlineParameterTempLocalFn &allocTempLocal,
    const EmitInlineParameterInstructionFn &emitInstruction,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo) {
  StructSlotLayoutInfo structLayout;
  const bool isUnsupportedStringPointerReferenceArgsPack =
      ((paramInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
        !paramInfo.referenceToArray &&
        !paramInfo.referenceToVector &&
        !paramInfo.referenceToMap &&
        !paramInfo.referenceToBuffer) ||
       (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
        !paramInfo.pointerToArray &&
        !paramInfo.pointerToVector &&
        !paramInfo.pointerToMap &&
        !paramInfo.pointerToBuffer)) &&
      paramInfo.valueKind == LocalInfo::ValueKind::String;
  if (isUnsupportedStringPointerReferenceArgsPack) {
    error = "variadic args<T> does not support string pointers or references";
    return false;
  }

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

  const auto requiresWrappedStructTypeMatch = [&]() {
    if (paramInfo.isResult) {
      return false;
    }
    if (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference) {
      return !(paramInfo.referenceToArray || paramInfo.referenceToVector ||
               paramInfo.referenceToMap || paramInfo.referenceToBuffer);
    }
    if (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer) {
      return !(paramInfo.pointerToArray || paramInfo.pointerToVector ||
               paramInfo.pointerToMap || paramInfo.pointerToBuffer);
    }
    return true;
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

  const auto wrappedArgsForwardingError = [&](LocalInfo::Kind wrappedKind) -> std::string {
    if (wrappedKind == LocalInfo::Kind::Reference) {
      return "variadic args<Reference<T>> requires reference values or location(...) forwarding";
    }
    return "variadic args<Pointer<T>> requires pointer values or location(...) forwarding";
  };

  auto preserveBuiltinCollectionLayout = [&](const LocalInfo &sourceInfo) {
    if (sourceInfo.isSoaVector && sourceInfo.usesBuiltinCollectionLayout) {
      paramInfo.usesBuiltinCollectionLayout = true;
    }
  };

  auto emitPackedValueToLocal = [&](const Expr &argExpr, int32_t destLocal) -> bool {
    Expr rewrittenMapArgExpr;
    const Expr *emittedArgExpr = &argExpr;
    if (paramInfo.argsPackElementKind == LocalInfo::Kind::Map &&
        paramInfo.structTypeName.empty() &&
        argExpr.kind == Expr::Kind::Call &&
        rewriteBuiltinMapConstructorExpr(
            argExpr, paramInfo.mapKeyKind, paramInfo.mapValueKind, resolveDefinitionCall, rewrittenMapArgExpr)) {
      emittedArgExpr = &rewrittenMapArgExpr;
    }
    if (paramInfo.argsPackElementKind == LocalInfo::Kind::Array ||
        paramInfo.argsPackElementKind == LocalInfo::Kind::Vector ||
        paramInfo.argsPackElementKind == LocalInfo::Kind::Buffer ||
        (paramInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
         (paramInfo.referenceToArray || paramInfo.referenceToVector || paramInfo.referenceToMap)) ||
        paramInfo.argsPackElementKind == LocalInfo::Kind::Map) {
      if (!emitExpr(*emittedArgExpr, callerLocals)) {
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
            (requiresWrappedStructTypeMatch() &&
             it->second.structTypeName != paramInfo.structTypeName)) {
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
      error = wrappedArgsForwardingError(LocalInfo::Kind::Reference);
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
            (requiresWrappedStructTypeMatch() &&
             it->second.structTypeName != paramInfo.structTypeName)) {
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
      error = wrappedArgsForwardingError(LocalInfo::Kind::Pointer);
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
            (requiresWrappedStructTypeMatch() &&
             it->second.structTypeName != paramInfo.structTypeName)) {
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
      error = wrappedArgsForwardingError(LocalInfo::Kind::Reference);
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
            (requiresWrappedStructTypeMatch() &&
             it->second.structTypeName != paramInfo.structTypeName)) {
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
      error = wrappedArgsForwardingError(LocalInfo::Kind::Pointer);
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
    if (!emitStructCopySlots(destBaseLocal, srcPtrLocal, structLayout.totalSlots)) {
      return false;
    }
    if (shouldDisarmStructCopySourceExpr(argExpr)) {
      emitDisarmTemporaryStructAfterCopy(emitInstruction, srcPtrLocal, paramInfo.structTypeName);
    }
    return true;
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
        ((requiresWrappedStructTypeMatch() ||
          (paramInfo.argsPackElementKind != LocalInfo::Kind::Pointer &&
           paramInfo.argsPackElementKind != LocalInfo::Kind::Reference)) &&
         callerIt->second.structTypeName != paramInfo.structTypeName) ||
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
    paramInfo.valueKind = callerIt->second.valueKind;
    if (!callerIt->second.structTypeName.empty()) {
      paramInfo.structTypeName = callerIt->second.structTypeName;
    }
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
    paramInfo.usesBuiltinCollectionLayout = callerIt->second.usesBuiltinCollectionLayout;
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

  auto recoverStructPackTypeFromArguments = [&]() -> bool {
    if (paramInfo.argsPackElementKind != LocalInfo::Kind::Map || !paramInfo.structTypeName.empty()) {
      return true;
    }
    std::string declaredElementType;
    if (!extractArgsPackElementTypeName(param, declaredElementType) ||
        !namesExplicitExperimentalMapType(declaredElementType)) {
      return true;
    }

    std::string inferredStructType;
    auto observeStructType = [&](const std::string &candidateStructType) -> bool {
      if (candidateStructType.empty()) {
        return true;
      }
      if (inferredStructType.empty()) {
        inferredStructType = candidateStructType;
        return true;
      }
      if (candidateStructType != inferredStructType) {
        error = "variadic parameter type mismatch";
        return false;
      }
      return true;
    };

    for (const Expr *packedArg : packedArgs) {
      if (packedArg == nullptr) {
        continue;
      }
      if (packedArg->isSpread) {
        if (packedArg->kind != Expr::Kind::Name) {
          continue;
        }
        auto callerIt = callerLocals.find(packedArg->name);
        if (callerIt == callerLocals.end() || !callerIt->second.isArgsPack) {
          continue;
        }
        if (!observeStructType(callerIt->second.structTypeName)) {
          return false;
        }
        continue;
      }
      if (!observeStructType(inferStructExprPath(*packedArg, callerLocals))) {
        return false;
      }
    }

    if (!inferredStructType.empty()) {
      paramInfo.structTypeName = std::move(inferredStructType);
    }
    return true;
  };

  if (!recoverStructPackTypeFromArguments()) {
    return false;
  }

  const bool isStructPack =
      !paramInfo.structTypeName.empty() &&
      (paramInfo.argsPackElementKind == LocalInfo::Kind::Value ||
       paramInfo.argsPackElementKind == LocalInfo::Kind::Map);
  const bool isWrappedStructPack =
      !paramInfo.structTypeName.empty() &&
      (paramInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
       paramInfo.argsPackElementKind == LocalInfo::Kind::Reference);

  if (packedArgs.size() == 1) {
    const Expr &packedArg = *packedArgs.front();
    if (emitArgsPackAlias(packedArg, true)) {
      return true;
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
      if (packedArg->kind == Expr::Kind::Name) {
        auto callerIt = callerLocals.find(packedArg->name);
        if (callerIt != callerLocals.end()) {
          preserveBuiltinCollectionLayout(callerIt->second);
        }
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
    preserveBuiltinCollectionLayout(spreadSourceInfo);
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
  return true;
}

} // namespace primec::ir_lowerer

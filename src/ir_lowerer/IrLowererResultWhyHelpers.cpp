#include "IrLowererResultInternal.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererRuntimeErrorHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

ResultWhyMethodCallEmitResult tryEmitResultWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::vector<IrInstruction> *instructionsOut,
    std::string &error) {
  if (!(expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result" && expr.name == "why")) {
    return ResultWhyMethodCallEmitResult::NotHandled;
  }

  ResultExprInfo resultInfo;
  if (!resolveResultWhyCallInfo(
          expr, localsIn, resolveResultExprInfo, resultInfo, error)) {
    return ResultWhyMethodCallEmitResult::Error;
  }

  auto hasImportedStdlibResultSum = [&]() {
    return defMap.find("/std/result/Result") != defMap.end();
  };

  auto directCallReturnsImportedStdlibResultSum = [&](const Expr &valueExpr) {
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isMethodCall ||
        resultInfo.hasValue || !resolveDefinitionCall || !hasImportedStdlibResultSum()) {
      return false;
    }
    const Definition *calleeDef = resolveDefinitionCall(valueExpr);
    if (calleeDef == nullptr) {
      return false;
    }
    for (const auto &transform : calleeDef->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string base;
      std::string argList;
      if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, argList)) {
        continue;
      }
      if (normalizeCollectionBindingTypeName(base) == "Result") {
        return true;
      }
    }
    return false;
  };

  auto tryEmitStdlibResultSumValue = [&](bool &emittedOut) -> bool {
    emittedOut = false;
    const Expr &valueExpr = expr.args[1];
    if (valueExpr.kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(valueExpr.name);
      if (localIt != localsIn.end()) {
        const std::string &structType = localIt->second.structTypeName;
        if (structType == "/std/result/Result" ||
            structType.rfind("/std/result/Result__", 0) == 0) {
          emittedOut = true;
          return emitExpr(valueExpr, localsIn);
        }
      }
    }
    if (directCallReturnsImportedStdlibResultSum(valueExpr)) {
      emittedOut = true;
      return emitExpr(valueExpr, localsIn);
    }
    if (valueExpr.kind == Expr::Kind::Call &&
        isSimpleCallName(valueExpr, "dereference") &&
        valueExpr.args.size() == 1 &&
        valueExpr.args.front().kind == Expr::Kind::Name &&
        !resultInfo.hasValue &&
        hasImportedStdlibResultSum()) {
      auto localIt = localsIn.find(valueExpr.args.front().name);
      if (localIt != localsIn.end() &&
          (localIt->second.kind == LocalInfo::Kind::Reference ||
           localIt->second.kind == LocalInfo::Kind::Pointer) &&
          localIt->second.isResult &&
          !localIt->second.resultHasValue &&
          trimTemplateTypeText(localIt->second.resultErrorType) ==
              trimTemplateTypeText(resultInfo.errorType)) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index));
        emittedOut = true;
        return true;
      }
    }
    return true;
  };

  auto emitStdlibResultSumErrorWhy = [&](int32_t resultLocal) -> bool {
    std::string errorStructPath;
    if (resolveStructTypeName(resultInfo.errorType, expr.namespacePrefix, errorStructPath)) {
      const std::string whyPath = errorStructPath + "/why";
      auto whyIt = defMap.find(whyPath);
      if (whyIt == defMap.end() || whyIt->second == nullptr ||
          whyIt->second->parameters.size() != 1) {
        return emitResultWhyEmptyString(internString, emitInstruction);
      }
      ReturnInfo whyReturn;
      if (!getReturnInfo || !getReturnInfo(whyIt->second->fullPath, whyReturn)) {
        return false;
      }
      if (whyReturn.returnsVoid || whyReturn.returnsArray ||
          whyReturn.kind != LocalInfo::ValueKind::String) {
        error = "Result.why requires a string-returning why() for " + resultInfo.errorType;
        return false;
      }
      StructSlotLayoutInfo errorLayout;
      if (!resolveStructSlotLayout(errorStructPath, errorLayout)) {
        return false;
      }
      const int32_t payloadPtrLocal = allocTempLocal();
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes);
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(payloadPtrLocal));

      LocalMap callLocals = localsIn;
      const std::string payloadLocalName =
          "__result_sum_error_payload_" + std::to_string(onErrorTempCounter++);
      LocalInfo payloadInfo;
      payloadInfo.kind = LocalInfo::Kind::Value;
      payloadInfo.valueKind = LocalInfo::ValueKind::Int64;
      payloadInfo.structTypeName = errorStructPath;
      payloadInfo.structSlotCount = errorLayout.totalSlots;
      payloadInfo.index = payloadPtrLocal;
      callLocals.emplace(payloadLocalName, payloadInfo);

      Expr payloadExpr;
      payloadExpr.kind = Expr::Kind::Name;
      payloadExpr.name = payloadLocalName;
      payloadExpr.namespacePrefix = expr.namespacePrefix;

      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = whyPath;
      callExpr.namespacePrefix = expr.namespacePrefix;
      callExpr.isMethodCall = false;
      callExpr.isBinding = false;
      callExpr.args.push_back(payloadExpr);
      callExpr.argNames.push_back(std::nullopt);
      return emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals);
    }

    LocalInfo::ValueKind errorKind = valueKindFromTypeName(resultInfo.errorType);
    if (!isSupportedResultWhyErrorKind(errorKind)) {
      return emitResultWhyEmptyString(internString, emitInstruction);
    }
    const int32_t payloadLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(payloadLocal));

    ResultWhyExprOps payloadOps = makeResultWhyExprOps(
        payloadLocal,
        expr.namespacePrefix,
        onErrorTempCounter,
        internString,
        allocTempLocal,
        emitInstruction);
    return emitResultWhyCallWithComposedOps(
        expr,
        resultInfo,
        localsIn,
        payloadLocal,
        defMap,
        payloadOps,
        resolveStructTypeName,
        getReturnInfo,
        bindingKind,
        resolveStructSlotLayout,
        valueKindFromTypeName,
        emitInlineDefinitionCall,
        emitFileErrorWhy,
        error);
  };

  bool emittedStdlibResultSum = false;
  if (instructionsOut != nullptr &&
      !tryEmitStdlibResultSumValue(emittedStdlibResultSum)) {
    return ResultWhyMethodCallEmitResult::Error;
  }
  if (emittedStdlibResultSum && instructionsOut != nullptr) {
    if (!emitExpr) {
      return ResultWhyMethodCallEmitResult::Error;
    }
    const int32_t resultLocal = allocTempLocal();
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpEmptyIndex = instructionsOut->size();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    if (!emitStdlibResultSumErrorWhy(resultLocal)) {
      return ResultWhyMethodCallEmitResult::Error;
    }
    const size_t jumpEndIndex = instructionsOut->size();
    emitInstruction(IrOpcode::Jump, 0);
    const size_t emptyIndex = instructionsOut->size();
    (*instructionsOut)[jumpEmptyIndex].imm = static_cast<int32_t>(emptyIndex);
    if (!emitResultWhyEmptyString(internString, emitInstruction)) {
      return ResultWhyMethodCallEmitResult::Error;
    }
    const size_t endIndex = instructionsOut->size();
    (*instructionsOut)[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
    return ResultWhyMethodCallEmitResult::Emitted;
  }

  int32_t errorLocal = 0;
  if (!emitResultWhyLocalsFromValueExpr(
          expr.args[1],
          localsIn,
          resultInfo,
          emitExpr,
          allocTempLocal,
          emitInstruction,
          errorLocal)) {
    return ResultWhyMethodCallEmitResult::Error;
  }

  const ResultWhyExprOps resultWhyExprOps = makeResultWhyExprOps(
      errorLocal,
      expr.namespacePrefix,
      onErrorTempCounter,
      internString,
      allocTempLocal,
      emitInstruction);

  if (instructionsOut != nullptr) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpEqI64, 0);
    const size_t jumpNonEmptyIndex = instructionsOut->size();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    if (!resultWhyExprOps.emitEmptyString()) {
      return ResultWhyMethodCallEmitResult::Error;
    }
    const size_t jumpEndIndex = instructionsOut->size();
    emitInstruction(IrOpcode::Jump, 0);
    const size_t nonEmptyIndex = instructionsOut->size();
    (*instructionsOut)[jumpNonEmptyIndex].imm = static_cast<int32_t>(nonEmptyIndex);

    if (!emitResultWhyCallWithComposedOps(
            expr,
            resultInfo,
            localsIn,
            errorLocal,
            defMap,
            resultWhyExprOps,
            resolveStructTypeName,
            getReturnInfo,
            bindingKind,
            resolveStructSlotLayout,
            valueKindFromTypeName,
            emitInlineDefinitionCall,
            emitFileErrorWhy,
            error)) {
      return ResultWhyMethodCallEmitResult::Error;
    }

    const size_t endIndex = instructionsOut->size();
    (*instructionsOut)[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
    return ResultWhyMethodCallEmitResult::Emitted;
  }

  return emitResultWhyCallWithComposedOps(
             expr,
             resultInfo,
             localsIn,
             errorLocal,
             defMap,
             resultWhyExprOps,
             resolveStructTypeName,
             getReturnInfo,
             bindingKind,
             resolveStructSlotLayout,
             valueKindFromTypeName,
             emitInlineDefinitionCall,
             emitFileErrorWhy,
             error)
             ? ResultWhyMethodCallEmitResult::Emitted
             : ResultWhyMethodCallEmitResult::Error;
}

ResultWhyDispatchEmitResult tryEmitResultWhyDispatchCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::vector<IrInstruction> *instructionsOut,
    std::string &error) {
  const ResultWhyMethodCallEmitResult resultWhyCallResult = tryEmitResultWhyCall(
      expr,
      localsIn,
      defMap,
      onErrorTempCounter,
      resolveResultExprInfo,
      resolveDefinitionCall,
      emitExpr,
      allocTempLocal,
      emitInstruction,
      internString,
      resolveStructTypeName,
      getReturnInfo,
      bindingKind,
      resolveStructSlotLayout,
      valueKindFromTypeName,
      emitInlineDefinitionCall,
      emitFileErrorWhy,
      instructionsOut,
      error);
  if (resultWhyCallResult == ResultWhyMethodCallEmitResult::Emitted) {
    return ResultWhyDispatchEmitResult::Emitted;
  }
  if (resultWhyCallResult == ResultWhyMethodCallEmitResult::Error) {
    return ResultWhyDispatchEmitResult::Error;
  }

  std::function<void(int32_t)> emitFileErrorWhyThunk;
  if (emitFileErrorWhy) {
    emitFileErrorWhyThunk = [&](int32_t errorLocal) {
      (void)emitFileErrorWhy(errorLocal);
    };
  }
  const FileErrorWhyCallEmitResult fileErrorWhyResult = tryEmitFileErrorWhyCall(
      expr,
      localsIn,
      emitExpr,
      allocTempLocal,
      emitInstruction,
      emitFileErrorWhyThunk,
      error);
  if (fileErrorWhyResult == FileErrorWhyCallEmitResult::Emitted) {
    return ResultWhyDispatchEmitResult::Emitted;
  }
  if (fileErrorWhyResult == FileErrorWhyCallEmitResult::Error) {
    return ResultWhyDispatchEmitResult::Error;
  }
  return ResultWhyDispatchEmitResult::NotHandled;
}

bool emitResultWhyLocalsFromValueExpr(
    const Expr &valueExpr,
    const LocalMap &localsIn,
    const ResultExprInfo &resultInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    int32_t &errorLocalOut) {
  if (!emitExpr || !emitExpr(valueExpr, localsIn)) {
    return false;
  }

  const int32_t resultLocal = allocTempLocal();
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal));
  errorLocalOut = allocTempLocal();
  emitResultWhyErrorLocalFromResult(
      resultLocal, resultInfo, errorLocalOut, allocTempLocal, emitInstruction);
  return true;
}

ResultWhyExprOps makeResultWhyExprOps(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t &onErrorTempCounter,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  ResultWhyExprOps ops;
  int32_t *tempCounter = &onErrorTempCounter;
  ops.emitEmptyString = [internString, emitInstruction]() {
    return emitResultWhyEmptyString(internString, emitInstruction);
  };
  ops.makeErrorValueExpr =
      [errorLocal, namespacePrefix, tempCounter](LocalMap &callLocals,
                                                 LocalInfo::ValueKind valueKind) {
        const int32_t tempOrdinal = (*tempCounter)++;
        return makeResultWhyErrorValueExpr(
            errorLocal, valueKind, namespacePrefix, tempOrdinal, callLocals);
      };
  ops.makeBoolErrorExpr =
      [errorLocal, namespacePrefix, tempCounter, allocTempLocal, emitInstruction](
          LocalMap &callLocals) {
        const int32_t tempOrdinal = (*tempCounter)++;
        return makeResultWhyBoolErrorExpr(
            errorLocal,
            namespacePrefix,
            tempOrdinal,
            callLocals,
            allocTempLocal,
            emitInstruction);
      };
  return ops;
}

ResultWhyCallOps makeResultWhyCallOps(
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<Expr(LocalMap &, LocalInfo::ValueKind)> &makeErrorValueExpr,
    const std::function<Expr(LocalMap &)> &makeBoolErrorExpr,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    const std::function<bool()> &emitEmptyString) {
  ResultWhyCallOps ops;
  ops.resolveStructTypeName = resolveStructTypeName;
  ops.getReturnInfo = getReturnInfo;
  ops.bindingKind = bindingKind;
  ops.extractFirstBindingTypeTransform =
      [](const Expr &bindingExpr, std::string &typeNameOut, std::vector<std::string> &templateArgsOut) {
        return extractFirstBindingTypeTransform(bindingExpr, typeNameOut, templateArgsOut);
      };
  ops.resolveStructSlotLayout = resolveStructSlotLayout;
  ops.valueKindFromTypeName = valueKindFromTypeName;
  ops.makeErrorValueExpr = makeErrorValueExpr;
  ops.makeBoolErrorExpr = makeBoolErrorExpr;
  ops.emitInlineDefinitionCall = emitInlineDefinitionCall;
  ops.emitFileErrorWhy = emitFileErrorWhy;
  ops.emitEmptyString = emitEmptyString;
  return ops;
}

bool emitResultWhyCallWithComposedOps(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyExprOps &exprOps,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error) {
  const ResultWhyCallOps ops = makeResultWhyCallOps(
      resolveStructTypeName,
      getReturnInfo,
      bindingKind,
      resolveStructSlotLayout,
      valueKindFromTypeName,
      [&](LocalMap &callLocals, LocalInfo::ValueKind valueKind) {
        return exprOps.makeErrorValueExpr(callLocals, valueKind);
      },
      [&](LocalMap &callLocals) { return exprOps.makeBoolErrorExpr(callLocals); },
      emitInlineDefinitionCall,
      emitFileErrorWhy,
      [&]() { return exprOps.emitEmptyString(); });
  return emitResolvedResultWhyCall(
             expr, resultInfo, localsIn, errorLocal, defMap, ops, error) ==
         ResultWhyCallEmitResult::Emitted;
}

bool isSupportedResultWhyErrorKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
         kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Bool;
}

std::string normalizeResultWhyErrorName(const std::string &errorType, LocalInfo::ValueKind errorKind) {
  if (errorType == "FileError" || errorType == "/std/file/FileError") {
    return "FileError";
  }
  switch (errorKind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    default:
      return errorType;
  }
}

void emitResultWhyErrorLocalFromResult(
    int32_t resultLocal,
    const ResultExprInfo &resultInfo,
    int32_t errorLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  if (usesInlineBufferResultErrorDiscriminator(resultInfo)) {
    const int32_t upperLocal = allocTempLocal();
    const int32_t lowerLocal = allocTempLocal();
    const int32_t isErrorLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::PushI64, 4294967296ull);
    emitInstruction(IrOpcode::DivI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(upperLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(upperLocal));
    emitInstruction(IrOpcode::PushI64, 4294967296ull);
    emitInstruction(IrOpcode::MulI64, 0);
    emitInstruction(IrOpcode::SubI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(lowerLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(lowerLocal));
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpEqI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(isErrorLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(upperLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(isErrorLocal));
    emitInstruction(IrOpcode::MulI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
    return;
  }
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
  if (resultInfo.hasValue) {
    emitInstruction(IrOpcode::PushI64, 4294967296ull);
    emitInstruction(IrOpcode::DivI64, 0);
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
}

void emitPackedResultPayloadLocalFromResult(
    int32_t resultLocal,
    const ResultExprInfo &resultInfo,
    int32_t errorLocal,
    int32_t payloadLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
  if (!usesInlineBufferResultErrorDiscriminator(resultInfo)) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
    emitInstruction(IrOpcode::PushI64, 4294967296ull);
    emitInstruction(IrOpcode::MulI64, 0);
    emitInstruction(IrOpcode::SubI64, 0);
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(payloadLocal));
}

bool emitResultWhyEmptyString(
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const int32_t index = internString("");
  emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(index));
  return true;
}

Expr makeResultWhyErrorValueExpr(int32_t errorLocal,
                                 LocalInfo::ValueKind valueKind,
                                 const std::string &namespacePrefix,
                                 int32_t tempOrdinal,
                                 LocalMap &callLocals) {
  std::string localName = "__result_why_err_" + std::to_string(tempOrdinal);
  LocalInfo info;
  info.index = errorLocal;
  info.isMutable = false;
  info.kind = LocalInfo::Kind::Value;
  info.valueKind = valueKind;
  callLocals.emplace(localName, info);

  Expr errorExpr;
  errorExpr.kind = Expr::Kind::Name;
  errorExpr.name = std::move(localName);
  errorExpr.namespacePrefix = namespacePrefix;
  return errorExpr;
}

Expr makeResultWhyBoolErrorExpr(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t tempOrdinal,
    LocalMap &callLocals,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const int32_t boolLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(boolLocal));

  std::string localName = "__result_why_bool_" + std::to_string(tempOrdinal);
  LocalInfo info;
  info.index = boolLocal;
  info.isMutable = false;
  info.kind = LocalInfo::Kind::Value;
  info.valueKind = LocalInfo::ValueKind::Bool;
  callLocals.emplace(localName, info);

  Expr errorExpr;
  errorExpr.kind = Expr::Kind::Name;
  errorExpr.name = std::move(localName);
  errorExpr.namespacePrefix = namespacePrefix;
  return errorExpr;
}

ResultWhyCallEmitResult emitResolvedResultWhyCall(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyCallOps &ops,
    std::string &error) {
  std::string errorStructPath;
  if (ops.resolveStructTypeName(resultInfo.errorType, expr.namespacePrefix, errorStructPath)) {
    const std::string whyPath = errorStructPath + "/why";
    auto whyIt = defMap.find(whyPath);
    if (whyIt != defMap.end() && whyIt->second && whyIt->second->parameters.size() == 1) {
      ReturnInfo whyReturn;
      if (!ops.getReturnInfo || !ops.getReturnInfo(whyIt->second->fullPath, whyReturn)) {
        return ResultWhyCallEmitResult::Error;
      }
      if (whyReturn.returnsVoid || whyReturn.returnsArray ||
          whyReturn.kind != LocalInfo::ValueKind::String) {
        error = "Result.why requires a string-returning why() for " + resultInfo.errorType;
        return ResultWhyCallEmitResult::Error;
      }
      const Expr &param = whyIt->second->parameters.front();
      LocalInfo::Kind paramKind = ops.bindingKind(param);
      std::string paramTypeName;
      std::vector<std::string> paramTemplateArgs;
      if (paramKind == LocalInfo::Kind::Value &&
          ops.extractFirstBindingTypeTransform(param, paramTypeName, paramTemplateArgs) &&
          paramTemplateArgs.empty()) {
        std::string paramStructPath;
        LocalMap callLocals = localsIn;
        if (ops.resolveStructTypeName(paramTypeName, param.namespacePrefix, paramStructPath) &&
            paramStructPath == errorStructPath) {
          StructSlotLayoutInfo layout;
          if (!ops.resolveStructSlotLayout(errorStructPath, layout)) {
            return ResultWhyCallEmitResult::Error;
          }
          if (layout.fields.size() != 1 || !layout.fields.front().structPath.empty() ||
              !isSupportedResultWhyErrorKind(layout.fields.front().valueKind)) {
            return ops.emitEmptyString() ? ResultWhyCallEmitResult::Emitted
                                         : ResultWhyCallEmitResult::Error;
          }
          Expr errorValueExpr = layout.fields.front().valueKind == LocalInfo::ValueKind::Bool
                                    ? ops.makeBoolErrorExpr(callLocals)
                                    : ops.makeErrorValueExpr(callLocals, layout.fields.front().valueKind);
          Expr ctorExpr;
          ctorExpr.kind = Expr::Kind::Call;
          ctorExpr.name = errorStructPath;
          ctorExpr.namespacePrefix = expr.namespacePrefix;
          ctorExpr.isMethodCall = false;
          ctorExpr.isBinding = false;
          ctorExpr.args.push_back(errorValueExpr);
          ctorExpr.argNames.push_back(std::nullopt);
          Expr callExpr;
          callExpr.kind = Expr::Kind::Call;
          callExpr.name = whyPath;
          callExpr.namespacePrefix = expr.namespacePrefix;
          callExpr.isMethodCall = false;
          callExpr.isBinding = false;
          callExpr.args.push_back(ctorExpr);
          callExpr.argNames.push_back(std::nullopt);
          return ops.emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals)
                     ? ResultWhyCallEmitResult::Emitted
                     : ResultWhyCallEmitResult::Error;
        }
        LocalInfo::ValueKind paramKindValue = ops.valueKindFromTypeName(paramTypeName);
        if (isSupportedResultWhyErrorKind(paramKindValue)) {
          Expr errorValueExpr = paramKindValue == LocalInfo::ValueKind::Bool
                                    ? ops.makeBoolErrorExpr(callLocals)
                                    : ops.makeErrorValueExpr(callLocals, paramKindValue);
          Expr callExpr;
          callExpr.kind = Expr::Kind::Call;
          callExpr.name = whyPath;
          callExpr.namespacePrefix = expr.namespacePrefix;
          callExpr.isMethodCall = false;
          callExpr.isBinding = false;
          callExpr.args.push_back(errorValueExpr);
          callExpr.argNames.push_back(std::nullopt);
          return ops.emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals)
                     ? ResultWhyCallEmitResult::Emitted
                     : ResultWhyCallEmitResult::Error;
        }
      }
    }
  }

  LocalInfo::ValueKind errorKind = ops.valueKindFromTypeName(resultInfo.errorType);
  if (isSupportedResultWhyErrorKind(errorKind)) {
    const std::string whyPath =
        "/" + normalizeResultWhyErrorName(resultInfo.errorType, errorKind) + "/why";
    auto whyIt = defMap.find(whyPath);
    if (whyIt != defMap.end() && whyIt->second && whyIt->second->parameters.size() == 1) {
      ReturnInfo whyReturn;
      if (!ops.getReturnInfo || !ops.getReturnInfo(whyIt->second->fullPath, whyReturn)) {
        return ResultWhyCallEmitResult::Error;
      }
      if (whyReturn.returnsVoid || whyReturn.returnsArray ||
          whyReturn.kind != LocalInfo::ValueKind::String) {
        error = "Result.why requires a string-returning why() for " + resultInfo.errorType;
        return ResultWhyCallEmitResult::Error;
      }
      LocalMap callLocals = localsIn;
      Expr errorValueExpr = errorKind == LocalInfo::ValueKind::Bool
                                ? ops.makeBoolErrorExpr(callLocals)
                                : ops.makeErrorValueExpr(callLocals, errorKind);
      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = whyPath;
      callExpr.namespacePrefix = expr.namespacePrefix;
      callExpr.isMethodCall = false;
      callExpr.isBinding = false;
      callExpr.args.push_back(errorValueExpr);
      callExpr.argNames.push_back(std::nullopt);
      return ops.emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals)
                 ? ResultWhyCallEmitResult::Emitted
                 : ResultWhyCallEmitResult::Error;
    }
  }

  if (resultInfo.errorType == "FileError" || resultInfo.errorType == "/std/file/FileError") {
    if (!ops.emitFileErrorWhy) {
      error = "FileError.why emitter is unavailable";
      return ResultWhyCallEmitResult::Error;
    }
    return ops.emitFileErrorWhy(errorLocal) ? ResultWhyCallEmitResult::Emitted
                                            : ResultWhyCallEmitResult::Error;
  }
  return ops.emitEmptyString() ? ResultWhyCallEmitResult::Emitted
                               : ResultWhyCallEmitResult::Error;
}


} // namespace primec::ir_lowerer

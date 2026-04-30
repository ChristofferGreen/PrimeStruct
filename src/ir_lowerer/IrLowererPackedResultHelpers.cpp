#include "IrLowererResultInternal.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool rewritePackedResultMapConstructorExpr(const Expr &callExpr,
                                           LocalInfo::ValueKind fallbackKeyKind,
                                           LocalInfo::ValueKind fallbackValueKind,
                                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                                           Expr &rewrittenExpr) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(callExpr) : nullptr;
  if (callee == nullptr ||
      !isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(
          callee->fullPath,
          primec::StdlibSurfaceId::CollectionsMapConstructors)) {
    return false;
  }

  rewrittenExpr = callExpr;
  rewrittenExpr.name = "/map/map";
  rewrittenExpr.namespacePrefix.clear();
  rewrittenExpr.isMethodCall = false;
  rewrittenExpr.semanticNodeId = 0;
  if (!rewrittenExpr.templateArgs.empty()) {
    return rewrittenExpr.templateArgs.size() == 2;
  }

  if (fallbackKeyKind != LocalInfo::ValueKind::Unknown &&
      fallbackValueKind != LocalInfo::ValueKind::Unknown) {
    rewrittenExpr.templateArgs = {
        typeNameForValueKind(fallbackKeyKind),
        typeNameForValueKind(fallbackValueKind),
    };
    return true;
  }

  std::string collectionName;
  std::vector<std::string> collectionArgs;
  if (callee != nullptr &&
      inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) &&
      collectionName == "map" && collectionArgs.size() == 2) {
    collectionArgs[0] = trimTemplateTypeText(collectionArgs[0]);
    collectionArgs[1] = trimTemplateTypeText(collectionArgs[1]);
    rewrittenExpr.templateArgs = std::move(collectionArgs);
    return true;
  }

  if (callee != nullptr && callee->parameters.size() >= 2) {
    auto extractParameterTypeName = [&](const Expr &paramExpr) {
      for (const auto &transform : paramExpr.transforms) {
        if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
            transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
            transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
            transform.name == "effects" || transform.name == "capabilities") {
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
      return std::string{};
    };
    const std::string keyTypeName = extractParameterTypeName(callee->parameters[0]);
    const std::string valueTypeName = extractParameterTypeName(callee->parameters[1]);
    if (!keyTypeName.empty() && !valueTypeName.empty()) {
      rewrittenExpr.templateArgs = {keyTypeName, valueTypeName};
      return true;
    }
  }

  if (callExpr.args.size() % 2 == 0) {
    auto inferLiteralKind = [&](const Expr &value) {
      if (value.kind == Expr::Kind::Literal) {
        return valueKindFromTypeName(value.isUnsigned ? "u64" : (value.intWidth == 64 ? "i64" : "i32"));
      }
      if (value.kind == Expr::Kind::BoolLiteral) {
        return LocalInfo::ValueKind::Bool;
      }
      if (value.kind == Expr::Kind::FloatLiteral) {
        return value.floatWidth == 64 ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      }
      if (value.kind == Expr::Kind::StringLiteral) {
        return LocalInfo::ValueKind::String;
      }
      return LocalInfo::ValueKind::Unknown;
    };
    LocalInfo::ValueKind inferredKeyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind inferredValueKind = LocalInfo::ValueKind::Unknown;
    bool validLiteralMap = true;
    for (size_t index = 0; index < callExpr.args.size(); index += 2) {
      const LocalInfo::ValueKind currentKeyKind = inferLiteralKind(callExpr.args[index]);
      const LocalInfo::ValueKind currentValueKind = inferLiteralKind(callExpr.args[index + 1]);
      if (currentKeyKind == LocalInfo::ValueKind::Unknown ||
          currentValueKind == LocalInfo::ValueKind::Unknown) {
        validLiteralMap = false;
        break;
      }
      if (inferredKeyKind == LocalInfo::ValueKind::Unknown) {
        inferredKeyKind = currentKeyKind;
      } else if (inferredKeyKind != currentKeyKind) {
        validLiteralMap = false;
        break;
      }
      if (inferredValueKind == LocalInfo::ValueKind::Unknown) {
        inferredValueKind = currentValueKind;
      } else if (inferredValueKind != currentValueKind) {
        validLiteralMap = false;
        break;
      }
    }
    if (validLiteralMap &&
        inferredKeyKind != LocalInfo::ValueKind::Unknown &&
        inferredValueKind != LocalInfo::ValueKind::Unknown) {
      rewrittenExpr.templateArgs = {
          typeNameForValueKind(inferredKeyKind),
          typeNameForValueKind(inferredValueKind),
      };
      return true;
    }
  }

  return false;
}

} // namespace

bool isSupportedPackedResultValueKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool ||
         kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::String;
}

bool isSupportedPackedResultCollectionKind(LocalInfo::Kind kind) {
  return kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector ||
         kind == LocalInfo::Kind::Map || kind == LocalInfo::Kind::Buffer;
}

bool resolveSupportedResultCollectionType(const std::string &typeText,
                                          LocalInfo::Kind &collectionKindOut,
                                          LocalInfo::ValueKind &valueKindOut,
                                          LocalInfo::ValueKind *mapKeyKindOut) {
  collectionKindOut = LocalInfo::Kind::Value;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  if (mapKeyKindOut != nullptr) {
    *mapKeyKindOut = LocalInfo::ValueKind::Unknown;
  }
  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argList)) {
    return false;
  }
  base = normalizeCollectionBindingTypeName(base);
  std::vector<std::string> args;
  if (!splitTemplateArgs(argList, args)) {
    return false;
  }
  if (base == "array") {
    if (args.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Array;
  } else if (base == "vector") {
    if (args.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Vector;
  } else if (base == "map") {
    if (args.size() != 2) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Map;
    const LocalInfo::ValueKind mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
    if (mapKeyKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    if (mapKeyKindOut != nullptr) {
      *mapKeyKindOut = mapKeyKind;
    }
  } else if (base == "Buffer") {
    if (args.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Buffer;
  } else {
    return false;
  }
  valueKindOut =
      valueKindFromTypeName(trimTemplateTypeText(collectionKindOut == LocalInfo::Kind::Map ? args.back() : args.front()));
  return valueKindOut != LocalInfo::ValueKind::Unknown;
}

bool resolvePackedResultStructPayloadInfo(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    PackedResultStructPayloadInfo &out) {
  out = PackedResultStructPayloadInfo{};
  if (structType.empty() || !resolveStructSlotLayout) {
    return false;
  }

  const std::string trimmedType = trimTemplateTypeText(structType);
  std::string baseType = trimmedType;
  std::string templateArgs;
  std::string parsedBaseType;
  if (splitTemplateTypeName(trimmedType, parsedBaseType, templateArgs)) {
    baseType = trimTemplateTypeText(parsedBaseType);
  } else {
    baseType = trimTemplateTypeText(baseType);
  }
  const std::string normalizedBase = normalizeCollectionBindingTypeName(baseType);
  if (normalizedBase == "File" || normalizedBase == "array" || normalizedBase == "vector" ||
      normalizedBase == "map" || normalizedBase == "Buffer") {
    return false;
  }

  StructSlotLayoutInfo layout;
  if (!resolveStructSlotLayout(structType, layout) || layout.totalSlots <= 0) {
    return false;
  }
  out.supported = true;
  out.slotCount = layout.totalSlots;

  if (layout.fields.size() != 1) {
    return true;
  }

  const StructSlotFieldInfo &field = layout.fields.front();
  if (field.slotCount != 1 || !field.structPath.empty() || !isSupportedPackedResultValueKind(field.valueKind)) {
    return true;
  }

  out.isPackedSingleSlot = true;
  out.packedKind = field.valueKind;
  out.fieldOffset = field.slotOffset;
  return true;
}

bool resolveSupportedResultStructPayloadInfo(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    bool &isPackedSingleSlotOut,
    LocalInfo::ValueKind &packedKindOut,
    int32_t &slotCountOut) {
  PackedResultStructPayloadInfo payloadInfo;
  if (!resolvePackedResultStructPayloadInfo(structType, resolveStructSlotLayout, payloadInfo)) {
    isPackedSingleSlotOut = false;
    packedKindOut = LocalInfo::ValueKind::Unknown;
    slotCountOut = 0;
    return false;
  }
  isPackedSingleSlotOut = payloadInfo.isPackedSingleSlot;
  packedKindOut = payloadInfo.packedKind;
  slotCountOut = payloadInfo.slotCount;
  return true;
}

bool isSupportedPackedResultValueInfo(
    const ResultExprInfo &info,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout) {
  if (isSupportedPackedResultCollectionKind(info.valueCollectionKind)) {
    return info.valueKind != LocalInfo::ValueKind::Unknown;
  }
  if (info.valueIsFileHandle && info.valueKind == LocalInfo::ValueKind::Int64) {
    return true;
  }
  if (!info.valueStructType.empty()) {
    bool isPackedSingleSlot = false;
    LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
    int32_t slotCount = 0;
    return resolveSupportedResultStructPayloadInfo(
        info.valueStructType, resolveStructSlotLayout, isPackedSingleSlot, packedStructKind, slotCount);
  }
  return isSupportedPackedResultValueKind(info.valueKind);
}

bool resolveSupportedPackedResultStructValueKind(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    LocalInfo::ValueKind &out) {
  out = LocalInfo::ValueKind::Unknown;
  bool isPackedSingleSlot = false;
  int32_t slotCount = 0;
  if (!resolveSupportedResultStructPayloadInfo(
          structType, resolveStructSlotLayout, isPackedSingleSlot, out, slotCount)) {
    return false;
  }
  return isPackedSingleSlot;
}

std::string unsupportedPackedResultValueKindError(const std::string &builtinName) {
  return "IR backends only support " + builtinName + " with supported payload values";
}

ResultOkMethodCallEmitResult tryEmitResultOkCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isFileHandleExpr,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  if (!(expr.kind == Expr::Kind::Call && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result" && expr.name == "ok")) {
    return ResultOkMethodCallEmitResult::NotHandled;
  }
  if (expr.args.size() == 1) {
    emitInstruction(IrOpcode::PushI32, 0);
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (expr.args.size() != 2) {
    error = "Result.ok accepts at most one argument";
    return ResultOkMethodCallEmitResult::Error;
  }
  Expr rewrittenDirectMapExpr;
  if (rewritePackedResultMapConstructorExpr(
          expr.args[1],
          LocalInfo::ValueKind::Unknown,
          LocalInfo::ValueKind::Unknown,
          resolveDefinitionCall,
          rewrittenDirectMapExpr)) {
    if (!emitExpr(rewrittenDirectMapExpr, localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }
  LocalInfo::ValueKind argKind = inferExprKind(expr.args[1], localsIn);
  if (argKind == LocalInfo::ValueKind::Unknown) {
    std::string builtinComparison;
    if (getBuiltinComparisonName(expr.args[1], builtinComparison)) {
      argKind = LocalInfo::ValueKind::Bool;
    }
  }
  if (isFileHandleExpr && argKind == LocalInfo::ValueKind::Int64 &&
      isFileHandleExpr(expr.args[1], localsIn)) {
    if (!emitExpr(expr.args[1], localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }
  LocalInfo::Kind collectionKind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind collectionValueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind collectionMapKeyKind = LocalInfo::ValueKind::Unknown;
  if (inferDirectResultValueCollectionInfo(
          expr.args[1], localsIn, resolveDefinitionCall, collectionKind, collectionValueKind, collectionMapKeyKind) &&
      isSupportedPackedResultCollectionKind(collectionKind)) {
    const Expr *payloadExpr = &expr.args[1];
    Expr rewrittenMapExpr;
    if (collectionKind == LocalInfo::Kind::Map &&
        rewritePackedResultMapConstructorExpr(
            expr.args[1],
            collectionMapKeyKind,
            collectionValueKind,
            resolveDefinitionCall,
            rewrittenMapExpr)) {
      payloadExpr = &rewrittenMapExpr;
    }
    if (!emitExpr(*payloadExpr, localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }

  std::string structType;
  if (!inferPackedResultStructType(expr.args[1], localsIn, resolveDefinitionCall, inferStructExprPath, structType)) {
    structType.clear();
  }
  PackedResultStructPayloadInfo payloadInfo;
  if (!resolvePackedResultStructPayloadInfo(structType, resolveStructSlotLayout, payloadInfo)) {
    if (!isSupportedPackedResultValueKind(argKind)) {
      error = unsupportedPackedResultValueKindError("Result.ok");
      return ResultOkMethodCallEmitResult::Error;
    }
    if (!emitExpr(expr.args[1], localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (payloadInfo.isPackedSingleSlot && expr.args[1].kind == Expr::Kind::Call && !expr.args[1].isMethodCall &&
      expr.args[1].args.size() == 1) {
    if (!emitExpr(expr.args[1].args.front(), localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (!emitExpr(expr.args[1], localsIn)) {
    return ResultOkMethodCallEmitResult::Error;
  }
  if (!payloadInfo.isPackedSingleSlot) {
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (!allocTempLocal) {
    error = "native backend missing temporary local for Result.ok";
    return ResultOkMethodCallEmitResult::Error;
  }
  const int32_t ptrLocal = allocTempLocal();
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  const uint64_t fieldOffsetBytes = static_cast<uint64_t>(payloadInfo.fieldOffset) * IrSlotBytes;
  if (fieldOffsetBytes != 0) {
    emitInstruction(IrOpcode::PushI64, fieldOffsetBytes);
    emitInstruction(IrOpcode::AddI64, 0);
  }
  emitInstruction(IrOpcode::LoadIndirect, 0);
  return ResultOkMethodCallEmitResult::Emitted;
}

bool inferPackedResultStructType(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    std::string &structTypeOut) {
  structTypeOut.clear();
  if (inferStructExprPath) {
    structTypeOut = inferStructExprPath(expr, localsIn);
    if (!structTypeOut.empty()) {
      return true;
    }
  }
  return inferDirectResultValueStructType(expr, localsIn, resolveDefinitionCall, structTypeOut);
}

bool resolveResultWhyCallInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                              ResultExprInfo &resultInfo,
                              std::string &error) {
  if (expr.args.size() != 2) {
    error = "Result.why requires exactly one argument";
    return false;
  }
  if (!resolveResultExprInfo ||
      !resolveResultExprInfo(expr.args[1], localsIn, resultInfo) ||
      !resultInfo.isResult) {
    error = "Result.why requires Result argument";
    return false;
  }
  return true;
}

bool resolveResultErrorCallInfo(const Expr &expr,
                                const LocalMap &localsIn,
                                const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                                ResultExprInfo &resultInfo,
                                std::string &error) {
  if (expr.args.size() != 2) {
    error = "Result.error requires exactly one argument";
    return false;
  }
  if (!resolveResultExprInfo ||
      !resolveResultExprInfo(expr.args[1], localsIn, resultInfo) ||
      !resultInfo.isResult) {
    error = "Result.error requires Result argument";
    return false;
  }
  return true;
}

ResultErrorMethodCallEmitResult tryEmitResultErrorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const SemanticProductTargetAdapter *semanticProductTargets,
    std::string &error) {
  if (!(expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result" && expr.name == "error")) {
    return ResultErrorMethodCallEmitResult::NotHandled;
  }

  ResultExprInfo resultInfo;
  if (!resolveResultErrorCallInfo(
          expr, localsIn, resolveResultExprInfo, resultInfo, error)) {
    return ResultErrorMethodCallEmitResult::Error;
  }

  auto hasImportedStdlibResultSum = [&]() {
    return defMap.find("/std/result/Result") != defMap.end();
  };

  auto directCallReturnsImportedStdlibResultSum =
      [&](const Expr &valueExpr, bool &returnsResultOut) {
    returnsResultOut = false;
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isMethodCall ||
        resultInfo.hasValue || !resolveDefinitionCall || !hasImportedStdlibResultSum()) {
      return true;
    }
    if (semanticProductTargets != nullptr &&
        semanticProductTargets->hasSemanticProduct &&
        valueExpr.semanticNodeId != 0) {
      const SemanticProgramQueryFact *queryFact =
          findSemanticProductQueryFact(*semanticProductTargets, valueExpr);
      if (queryFact == nullptr) {
        error = "missing semantic-product Result.error source query fact: " +
                valueExpr.name;
        return false;
      }
      if (!queryFact->hasResultType || queryFact->resultTypeHasValue ||
          trimTemplateTypeText(queryFact->resultErrorType) !=
              trimTemplateTypeText(resultInfo.errorType)) {
        error = "stale semantic-product Result.error source query metadata: " +
                valueExpr.name;
        return false;
      }
      returnsResultOut = true;
      return true;
    }
    const Definition *calleeDef = resolveDefinitionCall(valueExpr);
    if (calleeDef == nullptr) {
      return true;
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
        returnsResultOut = true;
        return true;
      }
    }
    return true;
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
    bool directCallReturnsStdlibResult = false;
    if (!directCallReturnsImportedStdlibResultSum(
            valueExpr, directCallReturnsStdlibResult)) {
      return false;
    }
    if (directCallReturnsStdlibResult) {
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

  bool emittedStdlibResultSum = false;
  if (!tryEmitStdlibResultSumValue(emittedStdlibResultSum)) {
    return ResultErrorMethodCallEmitResult::Error;
  }
  if (emittedStdlibResultSum) {
    if (!emitExpr) {
      return ResultErrorMethodCallEmitResult::Error;
    }
    const int32_t resultLocal = allocTempLocal();
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    return ResultErrorMethodCallEmitResult::Emitted;
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
    return ResultErrorMethodCallEmitResult::Error;
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  return ResultErrorMethodCallEmitResult::Emitted;
}


} // namespace primec::ir_lowerer

#include "IrLowererInlineStructArgHelpers.h"

#include "IrLowererFlowHelpers.h"

#include <limits>
#include <string_view>

namespace primec::ir_lowerer {

namespace {

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return "/std/collections/experimental_" + std::string(collectionName) +
         "/" + std::string(typeName);
}

bool isVectorStructPath(const std::string &structPath) {
  const std::string vectorTypePath = experimentalCollectionTypePath("vec" "tor", "Vector");
  return structPath == "/vector" || structPath == vectorTypePath ||
         structPath.rfind(vectorTypePath + "__", 0) == 0;
}

bool isSoaVectorStructPath(const std::string &structPath) {
  return structPath == "/soa" "_vector" ||
         structPath == "std/collections/" "soa" "_vector" ||
         structPath == "/std/collections/" "soa" "_vector" ||
         structPath == "Soa" "Vector" ||
         structPath == "/Soa" "Vector" ||
         structPath == "/std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
         structPath.rfind("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0;
}

std::string stripGeneratedStructSuffix(std::string structPath) {
  const size_t leafStart = structPath.find_last_of('/');
  const size_t suffixStart =
      structPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (suffixStart != std::string::npos) {
    structPath.erase(suffixStart);
  }
  return structPath;
}

std::string stripTemplateArguments(std::string structPath) {
  const size_t templateStart = structPath.find('<');
  if (templateStart != std::string::npos) {
    structPath.erase(templateStart);
  }
  return structPath;
}

std::string normalizedInternalSoaStorageLeaf(std::string structPath) {
  structPath = stripTemplateArguments(std::move(structPath));
  structPath = stripGeneratedStructSuffix(std::move(structPath));
  if (!structPath.empty() && structPath.front() == '/') {
    structPath.erase(structPath.begin());
  }
  constexpr std::string_view Prefix = "std/collections/internal_soa_storage/";
  if (structPath.rfind(Prefix, 0) == 0) {
    structPath.erase(0, Prefix.size());
  }
  if (structPath == "SoaColumn" || structPath == "SoaFieldView" ||
      structPath.rfind("SoaColumns", 0) == 0) {
    return structPath;
  }
  return {};
}

bool isCompatibleInternalSoaStorageFieldPath(const std::string &expectedStructPath,
                                             const std::string &actualStructPath) {
  const std::string expectedLeaf = normalizedInternalSoaStorageLeaf(expectedStructPath);
  if (expectedLeaf.empty()) {
    return false;
  }
  return expectedLeaf == normalizedInternalSoaStorageLeaf(actualStructPath);
}

bool isKnownStdUiStructAlias(std::string_view bareName) {
  return bareName == "CommandList" || bareName == "HtmlCommandList" ||
         bareName == "LayoutTree" || bareName == "LoginFormNodes" ||
         bareName == "Rgba8" || bareName == "UiEventStream";
}

bool isStdUiStructAliasMatch(const std::string &expectedStructPath,
                             const std::string &actualStructPath) {
  auto matchesBareToQualified = [](const std::string &bare,
                                   const std::string &qualified) {
    return bare.find('/') == std::string::npos &&
           isKnownStdUiStructAlias(bare) &&
           qualified == std::string("/std/ui/") + bare;
  };
  return matchesBareToQualified(expectedStructPath, actualStructPath) ||
         matchesBareToQualified(actualStructPath, expectedStructPath);
}

bool isCompatibleInlineStructFieldPath(const std::string &expectedStructPath,
                                       const std::string &actualStructPath) {
  if (expectedStructPath == actualStructPath) {
    return true;
  }
  if (isVectorStructPath(expectedStructPath) && isVectorStructPath(actualStructPath)) {
    return true;
  }
  if (isSoaVectorStructPath(expectedStructPath) && isSoaVectorStructPath(actualStructPath)) {
    return true;
  }
  if (isCompatibleInternalSoaStorageFieldPath(expectedStructPath, actualStructPath)) {
    return true;
  }
  if (isStdUiStructAliasMatch(expectedStructPath, actualStructPath)) {
    return true;
  }
  return stripGeneratedStructSuffix(expectedStructPath) ==
         stripGeneratedStructSuffix(actualStructPath);
}

std::string pathLeaf(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string_view::npos) {
    return std::string(path);
  }
  return std::string(path.substr(slash + 1));
}

bool isCanonicalBraceBlock(const Expr &expr) {
  return expr.kind == Expr::Kind::Call &&
         expr.name == "block" &&
         expr.args.empty() &&
         expr.bodyArguments.size() == 1;
}

bool isExpectedStructBraceConstructor(const Expr &arg,
                                      const std::string &expectedStructPath) {
  if (isCanonicalBraceBlock(arg)) {
    return isExpectedStructBraceConstructor(arg.bodyArguments.front(),
                                            expectedStructPath);
  }
  if (arg.kind != Expr::Kind::Call ||
      arg.isMethodCall ||
      arg.isFieldAccess ||
      arg.name.empty()) {
    return false;
  }
  const bool canonicalBraceBlock =
      arg.args.size() == 1 &&
      isCanonicalBraceBlock(arg.args.front());
  if (!arg.isBraceConstructor && !canonicalBraceBlock) {
    return false;
  }

  std::string candidatePath = arg.name;
  if (candidatePath.find('/') == std::string::npos && !arg.namespacePrefix.empty()) {
    candidatePath = arg.namespacePrefix == "/" ? "/" + candidatePath
                                               : arg.namespacePrefix + "/" + candidatePath;
  }
  if (isCompatibleInlineStructFieldPath(expectedStructPath, candidatePath)) {
    return true;
  }

  return arg.namespacePrefix.empty() &&
         arg.name.find('/') == std::string::npos &&
         pathLeaf(expectedStructPath) == arg.name;
}

bool isExpectedUninitializedStructStorageField(const Expr &param,
                                               const std::string &expectedStructPath) {
  for (const Transform &transform : param.transforms) {
    if (transform.name != "uninitialized" || transform.templateArgs.size() != 1) {
      continue;
    }
    if (isCompatibleInlineStructFieldPath(expectedStructPath, transform.templateArgs.front())) {
      return true;
    }
  }
  return false;
}

void materializeInlineStructFieldLocal(const StructSlotFieldInfo &field,
                                       int32_t baseLocal,
                                       int32_t &nextLocal,
                                       LocalInfo &fieldInfo,
                                       const EmitInlineStructInstructionFn &emitInstruction) {
  if (!field.structPath.empty()) {
    if (fieldInfo.structTypeName.empty()) {
      fieldInfo.structTypeName = field.structPath;
    }
    const int32_t fieldPtrLocal = nextLocal++;
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal + field.slotOffset));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fieldPtrLocal));
    fieldInfo.index = fieldPtrLocal;
    return;
  }
  fieldInfo.index = baseLocal + field.slotOffset;
}

} // namespace

bool emitInlineStructDefinitionArguments(const std::string &calleePath,
                                         const std::vector<Expr> &params,
                                         const std::vector<const Expr *> &orderedArgs,
                                         const LocalMap &callerLocals,
                                         bool requireValue,
                                         int32_t &nextLocal,
                                         const ResolveInlineStructSlotLayoutFn &resolveStructSlotLayout,
                                         const InferInlineStructExprKindFn &inferExprKind,
                                         const InferInlineStructExprPathFn &inferStructExprPath,
                                         const EmitInlineStructExprFn &emitExpr,
                                         const InferInlineStructFieldLocalInfoFn &inferFieldLocalInfo,
                                         const EmitInlineStructCopySlotsFn &emitStructCopySlots,
                                         const AllocInlineStructTempLocalFn &allocTempLocal,
                                         const EmitInlineStructInstructionFn &emitInstruction,
                                         std::string &error,
                                         std::optional<int32_t> destBaseLocal) {
  StructSlotLayoutInfo layout;
  if (!resolveStructSlotLayout(calleePath, layout)) {
    return false;
  }
  if (static_cast<int32_t>(orderedArgs.size()) != static_cast<int32_t>(layout.fields.size())) {
    error = "argument count mismatch";
    return false;
  }
  if (params.size() != layout.fields.size()) {
    error = "internal error: struct field parameter mismatch";
    return false;
  }
  const int32_t baseLocal = destBaseLocal.value_or(nextLocal);
  if (!destBaseLocal.has_value()) {
    nextLocal += layout.totalSlots;
  }
  if (requireValue) {
    emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1)));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));
  }

  LocalMap structLocals = callerLocals;
  for (size_t i = 0; i < layout.fields.size(); ++i) {
    const StructSlotFieldInfo &field = layout.fields[i];
    size_t paramIndex = i;
    for (size_t candidate = 0; candidate < params.size(); ++candidate) {
      if (params[candidate].name == field.name) {
        paramIndex = candidate;
        break;
      }
    }
    const Expr *arg = orderedArgs[paramIndex];
    const Expr &param = params[paramIndex];
    if (!arg) {
      error = "argument count mismatch";
      return false;
    }
    const bool usesDefaultInitializer = !param.args.empty() && arg == &param.args.front();
    const LocalMap &argLocals = usesDefaultInitializer ? structLocals : callerLocals;

    if (field.structPath.empty()) {
      LocalInfo::ValueKind argKind = inferExprKind(*arg, argLocals);
      if (argKind == LocalInfo::ValueKind::Unknown && field.valueKind != LocalInfo::ValueKind::Unknown) {
        argKind = field.valueKind;
      }
      if (argKind == LocalInfo::ValueKind::Unknown) {
        error = "native backend requires known scalar struct field values on " + calleePath;
        return false;
      }
      bool emittedFieldValue = false;
      if (argKind != field.valueKind) {
        if (isVectorStructPath(calleePath) &&
            field.valueKind == LocalInfo::ValueKind::Int32 &&
            (field.name == "fieldCount" || field.name == "fieldCapacity") &&
            argKind == LocalInfo::ValueKind::Bool) {
          emitInstruction(IrOpcode::PushI32, 0);
          emittedFieldValue = true;
        } else if (field.valueKind == LocalInfo::ValueKind::Int32 &&
            arg->kind == Expr::Kind::Literal && !arg->isUnsigned &&
            arg->literalValue <=
                static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
          emitInstruction(IrOpcode::PushI32, arg->literalValue);
          emittedFieldValue = true;
        } else if (field.valueKind == LocalInfo::ValueKind::Int32 &&
                   argKind == LocalInfo::ValueKind::Int64) {
          // Unsuffixed integer literals can flow through explicit i32
          // parameters as i64 facts; the slot representation is still
          // compatible for field materialization.
        } else {
          error = "struct field type mismatch: expected " +
                  std::to_string(static_cast<int>(field.valueKind)) +
                  ", got " + std::to_string(static_cast<int>(argKind)) +
                  " in " + calleePath + "::" + field.name;
          return false;
        }
      }
      if (!emittedFieldValue && !emitExpr(*arg, argLocals)) {
        return false;
      }
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + field.slotOffset));
      LocalInfo fieldInfo;
      if (!inferFieldLocalInfo(param, structLocals, fieldInfo, error)) {
        return false;
      }
      materializeInlineStructFieldLocal(field, baseLocal, nextLocal, fieldInfo, emitInstruction);
      structLocals[param.name] = std::move(fieldInfo);
      continue;
    }

    std::string argStruct = inferStructExprPath(*arg, argLocals);
    if (argStruct.empty() && isExpectedStructBraceConstructor(*arg, field.structPath)) {
      argStruct = field.structPath;
    }
    const bool isUninitializedStructStorage =
        arg->kind == Expr::Kind::Call &&
        !arg->isMethodCall &&
        !arg->isFieldAccess &&
        arg->name == "uninitialized" &&
        arg->args.empty() &&
        arg->templateArgs.size() == 1;
    const bool isDefaultUninitializedStructStorage =
        usesDefaultInitializer &&
        isExpectedUninitializedStructStorageField(param, field.structPath);
    if (argStruct.empty() && (isUninitializedStructStorage || isDefaultUninitializedStructStorage)) {
      argStruct = field.structPath;
    }
    if (argStruct.empty() ||
        !isCompatibleInlineStructFieldPath(field.structPath, argStruct)) {
      error = "struct field type mismatch: expected " + field.structPath + ", got " +
              (argStruct.empty() ? std::string("<unknown>") : argStruct) +
              " in " + calleePath + "::" + field.name;
      return false;
    }
    if (isUninitializedStructStorage || isDefaultUninitializedStructStorage) {
      emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(field.slotCount - 1)));
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + field.slotOffset));
      LocalInfo fieldInfo;
      if (!inferFieldLocalInfo(param, structLocals, fieldInfo, error)) {
        return false;
      }
      materializeInlineStructFieldLocal(field, baseLocal, nextLocal, fieldInfo, emitInstruction);
      structLocals[param.name] = std::move(fieldInfo);
      continue;
    }
    if (!emitExpr(*arg, argLocals)) {
      return false;
    }
    const int32_t srcPtrLocal = allocTempLocal();
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal));
    if (!emitStructCopySlots(baseLocal + field.slotOffset, srcPtrLocal, field.slotCount)) {
      return false;
    }
    if (shouldDisarmStructCopySourceExpr(*arg)) {
      emitDisarmTemporaryStructAfterCopy(emitInstruction, srcPtrLocal, field.structPath);
    }
    LocalInfo fieldInfo;
    if (!inferFieldLocalInfo(param, structLocals, fieldInfo, error)) {
      return false;
    }
    materializeInlineStructFieldLocal(field, baseLocal, nextLocal, fieldInfo, emitInstruction);
    structLocals[param.name] = std::move(fieldInfo);
  }

  if (requireValue) {
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal));
  }
  return true;
}

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
  std::vector<Expr> synthesizedParams(orderedArgs.size());
  return emitInlineStructDefinitionArguments(calleePath,
                                             synthesizedParams,
                                             orderedArgs,
                                             callerLocals,
                                             requireValue,
                                             nextLocal,
                                             resolveStructSlotLayout,
                                             inferExprKind,
                                             inferStructExprPath,
                                             emitExpr,
                                             [](const Expr &, const LocalMap &, LocalInfo &fieldInfoOut, std::string &) {
                                               fieldInfoOut = LocalInfo{};
                                               return true;
                                             },
                                             emitStructCopySlots,
                                             allocTempLocal,
                                             emitInstruction,
                                             error);
}

} // namespace primec::ir_lowerer

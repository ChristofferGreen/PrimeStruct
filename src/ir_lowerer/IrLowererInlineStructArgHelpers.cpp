#include "IrLowererInlineStructArgHelpers.h"

#include "IrLowererFlowHelpers.h"

#include <string_view>

namespace primec::ir_lowerer {

namespace {

bool isVectorStructPath(const std::string &structPath) {
  return structPath == "/vector" || structPath == "/std/collections/experimental_vector/Vector" ||
         structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool isSoaVectorStructPath(const std::string &structPath) {
  return structPath == "/soa_vector" ||
         structPath == "/std/collections/experimental_soa_vector/SoaVector" ||
         structPath.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0;
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

std::string normalizedInternalSoaStorageLeaf(std::string structPath) {
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
  return stripGeneratedStructSuffix(expectedStructPath) ==
         stripGeneratedStructSuffix(actualStructPath);
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
    const Expr *arg = orderedArgs[i];
    const Expr &param = params[i];
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
      if (argKind != field.valueKind) {
        error = "struct field type mismatch";
        return false;
      }
      if (!emitExpr(*arg, argLocals)) {
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
    if (argStruct.empty() ||
        !isCompatibleInlineStructFieldPath(field.structPath, argStruct)) {
      error = "struct field type mismatch: expected " + field.structPath + ", got " +
              (argStruct.empty() ? std::string("<unknown>") : argStruct) +
              " in " + calleePath + "::" + field.name;
      return false;
    }
    const bool isUninitializedStructStorage =
        arg->kind == Expr::Kind::Call &&
        !arg->isMethodCall &&
        !arg->isFieldAccess &&
        arg->name == "uninitialized" &&
        arg->args.empty() &&
        arg->templateArgs.size() == 1;
    if (isUninitializedStructStorage) {
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
    if (arg->kind == Expr::Kind::Call) {
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

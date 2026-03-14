#include "IrLowererStatementBindingHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStringLiteralHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool hasSoaVectorTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "soa_vector") {
      return true;
    }
  }
  return false;
}

bool isPointerMemoryIntrinsicCall(const Expr &expr) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return false;
  }
  return builtinName == "alloc" || builtinName == "realloc" || builtinName == "at" ||
         builtinName == "at_unsafe";
}

LocalInfo::ValueKind inferPointerMemoryIntrinsicValueKind(const Expr &expr,
                                                          const LocalMap &localsIn,
                                                          const InferBindingExprKindFn &inferExprKind) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (builtinName == "alloc" && expr.templateArgs.size() == 1) {
    return valueKindFromTypeName(expr.templateArgs.front());
  }
  if ((builtinName == "realloc" && expr.args.size() == 2) || (builtinName == "at" && expr.args.size() == 3) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2)) {
    if (expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        return it->second.valueKind;
      }
    }
    return inferExprKind(expr.args.front(), localsIn);
  }
  return LocalInfo::ValueKind::Unknown;
}

std::string inferPointerMemoryIntrinsicStructType(const Expr &expr, const LocalMap &localsIn) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return "";
  }
  if (builtinName == "realloc" && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
      return it->second.structTypeName;
    }
  }
  if ((builtinName == "at" && expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name)) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
      return it->second.structTypeName;
    }
  }
  return "";
}

} // namespace

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind) {
  StatementBindingTypeInfo info;
  info.kind = bindingKind(stmt);
  const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  LocalInfo::ValueKind inferredInitValueKind = LocalInfo::ValueKind::Unknown;
  if (!hasExplicitType && info.kind == LocalInfo::Kind::Value) {
      if (init.kind == Expr::Kind::Name) {
        auto it = localsIn.find(init.name);
        if (it != localsIn.end()) {
          info.kind = it->second.kind;
        }
      } else if (init.kind == Expr::Kind::Call) {
        inferredInitValueKind = inferExprKind(init, localsIn);
        if (isPointerMemoryIntrinsicCall(init)) {
          info.kind = LocalInfo::Kind::Pointer;
        } else if (inferredInitValueKind == LocalInfo::ValueKind::Unknown) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection)) {
          if (collection == "array") {
            info.kind = LocalInfo::Kind::Array;
          } else if (collection == "vector") {
            info.kind = LocalInfo::Kind::Vector;
          } else if (collection == "map") {
            info.kind = LocalInfo::Kind::Map;
          }
        }
      }
    }
  }

  if (info.kind == LocalInfo::Kind::Map) {
    if (hasExplicitType) {
      for (const auto &transform : stmt.transforms) {
        const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
        if (normalizedName == "map" && transform.templateArgs.size() == 2) {
          info.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
          info.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
          break;
        }
      }
    } else if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
        info.mapKeyKind = it->second.mapKeyKind;
        info.mapValueKind = it->second.mapValueKind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && collection == "map" && init.templateArgs.size() == 2) {
        info.mapKeyKind = valueKindFromTypeName(init.templateArgs[0]);
        info.mapValueKind = valueKindFromTypeName(init.templateArgs[1]);
      }
    }
    info.valueKind = info.mapValueKind;
    return info;
  }

  if (hasExplicitType) {
    info.valueKind = bindingValueKind(stmt, info.kind);
    return info;
  }

  if (info.kind == LocalInfo::Kind::Value) {
    info.valueKind = (inferredInitValueKind != LocalInfo::ValueKind::Unknown)
                         ? inferredInitValueKind
                         : inferExprKind(init, localsIn);
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = LocalInfo::ValueKind::Int32;
    }
    return info;
  }

  if (info.kind == LocalInfo::Kind::Pointer || info.kind == LocalInfo::Kind::Reference) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        info.valueKind = it->second.valueKind;
      }
      info.structTypeName = (it != localsIn.end()) ? it->second.structTypeName : "";
    } else if (info.kind == LocalInfo::Kind::Pointer && init.kind == Expr::Kind::Call &&
               isPointerMemoryIntrinsicCall(init)) {
      info.valueKind = inferPointerMemoryIntrinsicValueKind(init, localsIn, inferExprKind);
      info.structTypeName = inferPointerMemoryIntrinsicStructType(init, localsIn);
    }
    return info;
  }

  if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
        info.valueKind = it->second.valueKind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && (collection == "array" || collection == "vector") &&
          init.templateArgs.size() == 1) {
        info.valueKind = valueKindFromTypeName(init.templateArgs.front());
      }
    }
  }

  return info;
}

bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const IsBindingMutableFn &isBindingMutable,
                                 const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const InferBindingExprKindFn &inferExprKind,
                                 const IsFileErrorBindingFn &isFileErrorBinding,
                                 const SetReferenceArrayInfoForBindingFn &setReferenceArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructValueInfo,
                                 const IsStringBindingFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error) {
  infoOut.isMutable = isBindingMutable(param);
  infoOut.isSoaVector = hasSoaVectorTypeTransform(param);
  infoOut.kind = bindingKind(param);
  if (hasExplicitBindingTypeTransform(param)) {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value &&
             isPointerMemoryIntrinsicCall(param.args.front())) {
    infoOut.kind = LocalInfo::Kind::Pointer;
    infoOut.valueKind = inferPointerMemoryIntrinsicValueKind(param.args.front(), localsForKindInference, inferExprKind);
    infoOut.structTypeName = inferPointerMemoryIntrinsicStructType(param.args.front(), localsForKindInference);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value) {
    infoOut.valueKind = inferExprKind(param.args.front(), localsForKindInference);
    if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = LocalInfo::ValueKind::Int32;
    }
  } else {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
  }

  if (infoOut.kind == LocalInfo::Kind::Map) {
    for (const auto &transform : param.transforms) {
      const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
      if (normalizedName == "map" && transform.templateArgs.size() == 2) {
        infoOut.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
        infoOut.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
        break;
      }
    }
  }
  for (const auto &transform : param.transforms) {
    if (transform.name == "File") {
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
    } else if (transform.name == "Result") {
      infoOut.isResult = true;
      infoOut.resultHasValue = (transform.templateArgs.size() == 2);
      infoOut.resultValueKind =
          infoOut.resultHasValue ? valueKindFromTypeName(transform.templateArgs.front())
                                 : LocalInfo::ValueKind::Unknown;
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      if (!transform.templateArgs.empty()) {
        infoOut.resultErrorType = transform.templateArgs.back();
      }
    }
  }

  infoOut.isFileError = isFileErrorBinding(param);
  setReferenceArrayInfo(param, infoOut);
  applyStructArrayInfo(param, infoOut);
  applyStructValueInfo(param, infoOut);
  if (!isStringBinding(param)) {
    return true;
  }
  if (infoOut.kind != LocalInfo::Kind::Value && infoOut.kind != LocalInfo::Kind::Map) {
    error = "native backend does not support string pointers or references";
    return false;
  }
  infoOut.valueKind = LocalInfo::ValueKind::String;
  infoOut.stringSource = LocalInfo::StringSource::RuntimeIndex;
  infoOut.stringIndex = -1;
  infoOut.argvChecked = true;
  return true;
}

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error) {
  zeroOp = IrOpcode::PushI32;
  zeroImm = 0;
  if (kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector || kind == LocalInfo::Kind::Map ||
      kind == LocalInfo::Kind::Buffer) {
    zeroOp = IrOpcode::PushI64;
    return true;
  }

  switch (valueKind) {
    case LocalInfo::ValueKind::Int64:
    case LocalInfo::ValueKind::UInt64:
      zeroOp = IrOpcode::PushI64;
      return true;
    case LocalInfo::ValueKind::Float32:
      zeroOp = IrOpcode::PushF32;
      return true;
    case LocalInfo::ValueKind::Float64:
      zeroOp = IrOpcode::PushF64;
      return true;
    case LocalInfo::ValueKind::Int32:
    case LocalInfo::ValueKind::Bool:
      zeroOp = IrOpcode::PushI32;
      return true;
    case LocalInfo::ValueKind::String:
      zeroOp = IrOpcode::PushI64;
      return true;
    default:
      error = "native backend requires a concrete uninitialized storage type on " + bindingName;
      return false;
  }
}

bool emitStringStatementBindingInitializer(const Expr &stmt,
                                           const Expr &init,
                                           LocalMap &localsIn,
                                           int32_t &nextLocal,
                                           std::vector<IrInstruction> &instructions,
                                           const IsBindingMutableFn &isBindingMutable,
                                           const std::function<int32_t(const std::string &)> &internString,
                                           const EmitExprForBindingFn &emitExpr,
                                           const InferBindingExprKindFn &inferExprKind,
                                           const std::function<int32_t()> &allocTempLocal,
                                           const IsEntryArgsNameFn &isEntryArgsName,
                                           const std::function<void()> &emitArrayIndexOutOfBounds,
                                           std::string &error) {
  int32_t index = -1;
  LocalInfo::StringSource source = LocalInfo::StringSource::None;
  bool argvChecked = true;
  bool emittedValue = false;

  if (init.kind == Expr::Kind::StringLiteral) {
    std::string decoded;
    if (!parseLowererStringLiteral(init.stringValue, decoded, error)) {
      return false;
    }
    index = internString(decoded);
    source = LocalInfo::StringSource::TableIndex;
    emittedValue = true;
    instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
  } else if (init.kind == Expr::Kind::Name) {
    auto it = localsIn.find(init.name);
    if (it == localsIn.end()) {
      error = "native backend does not know identifier: " + init.name;
      return false;
    }
    if (it->second.valueKind != LocalInfo::ValueKind::String || it->second.stringSource == LocalInfo::StringSource::None) {
      error = "native backend requires string bindings to use string literals, bindings, or entry args";
      return false;
    }
    source = it->second.stringSource;
    index = it->second.stringIndex;
    if (source == LocalInfo::StringSource::ArgvIndex) {
      argvChecked = it->second.argvChecked;
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
    emittedValue = true;
  } else if (init.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(init, accessName)) {
      if (init.args.size() != 2) {
        error = accessName + " requires exactly two arguments";
        return false;
      }
      if (!isEntryArgsName(init.args.front(), localsIn)) {
        error = "native backend only supports entry argument indexing";
        return false;
      }
      LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(init.args[1], localsIn));
      if (!isSupportedIndexKind(indexKind)) {
        error = "native backend requires integer indices for " + accessName;
        return false;
      }

      const int32_t argvIndexLocal = allocTempLocal();
      if (!emitExpr(init.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

      if (accessName == "at") {
        if (indexKind != LocalInfo::ValueKind::UInt64) {
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
          instructions.push_back({pushZeroForIndex(indexKind), 0});
          instructions.push_back({cmpLtForIndex(indexKind), 0});
          size_t jumpNonNegative = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitArrayIndexOutOfBounds();
          size_t nonNegativeIndex = instructions.size();
          instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
        }

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
        instructions.push_back({IrOpcode::PushArgc, 0});
        instructions.push_back({cmpGeForIndex(indexKind), 0});
        size_t jumpInRange = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        size_t inRangeIndex = instructions.size();
        instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
      }

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
      source = LocalInfo::StringSource::ArgvIndex;
      argvChecked = (accessName == "at");
      emittedValue = true;
    } else {
      LocalInfo::ValueKind initKind = inferExprKind(init, localsIn);
      if (initKind != LocalInfo::ValueKind::String) {
        error = "native backend requires string bindings to use string literals, bindings, or entry args";
        return false;
      }
      if (!emitExpr(init, localsIn)) {
        return false;
      }
      source = LocalInfo::StringSource::RuntimeIndex;
      emittedValue = true;
    }
  } else {
    error = "native backend requires string bindings to use string literals, bindings, or entry args";
    return false;
  }

  LocalInfo info;
  info.index = nextLocal++;
  info.isMutable = isBindingMutable(stmt);
  info.kind = LocalInfo::Kind::Value;
  info.valueKind = LocalInfo::ValueKind::String;
  info.stringSource = source;
  info.stringIndex = index;
  info.argvChecked = argvChecked;
  if (!emittedValue) {
    error = "native backend requires string bindings to use string literals, bindings, or entry args";
    return false;
  }

  localsIn.emplace(stmt.name, info);
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
  return true;
}

UninitializedStorageInitDropEmitResult tryEmitUninitializedStorageInitDropStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    const ResolveStructSlotLayoutForStatementFn &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    const EmitStructCopyFromPtrsForStatementFn &emitStructCopyFromPtrs,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall ||
      (!isSimpleCallName(stmt, "init") && !isSimpleCallName(stmt, "drop"))) {
    return UninitializedStorageInitDropEmitResult::NotMatched;
  }

  const bool isInit = isSimpleCallName(stmt, "init");
  const size_t expectedArgs = isInit ? 2 : 1;
  if (stmt.args.size() != expectedArgs) {
    error = std::string(isInit ? "init" : "drop") + " requires exactly " + std::to_string(expectedArgs) + " argument" +
            (expectedArgs == 1 ? "" : "s");
    return UninitializedStorageInitDropEmitResult::Error;
  }

  UninitializedStorageAccessInfo access;
  bool resolved = false;
  if (!resolveUninitializedStorage(stmt.args.front(), localsIn, access, resolved)) {
    return UninitializedStorageInitDropEmitResult::Error;
  }
  if (!resolved) {
    error = std::string(isInit ? "init" : "drop") + " requires uninitialized storage";
    return UninitializedStorageInitDropEmitResult::Error;
  }

  if (!isInit) {
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  auto emitFieldPointer = [&](const Expr &receiver, const StructSlotFieldInfo &field, int32_t ptrLocal) -> bool {
    if (!emitExpr(receiver, localsIn)) {
      return false;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    const uint64_t offsetBytes = static_cast<uint64_t>(field.slotOffset) * IrSlotBytes;
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    return true;
  };

  const Expr &valueExpr = stmt.args.back();
  if (access.location == UninitializedStorageAccessInfo::Location::Local) {
    const LocalInfo &storageInfo = *access.local;
    if (!storageInfo.structTypeName.empty()) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(storageInfo.structTypeName, layout)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      if (!emitExpr(valueExpr, localsIn)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
      const int32_t destPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
      if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, layout.totalSlots)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      return UninitializedStorageInitDropEmitResult::Emitted;
    }
    if (!emitExpr(valueExpr, localsIn)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(storageInfo.index)});
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  if (access.location == UninitializedStorageAccessInfo::Location::Field) {
    const Expr &receiver = stmt.args.front().args.front();
    const StructSlotFieldInfo &field = access.fieldSlot;
    const int32_t ptrLocal = allocTempLocal();
    if (!emitFieldPointer(receiver, field, ptrLocal)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    if (!field.structPath.empty()) {
      if (!emitExpr(valueExpr, localsIn)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
      if (!emitStructCopyFromPtrs(ptrLocal, srcPtrLocal, field.slotCount)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      return UninitializedStorageInitDropEmitResult::Emitted;
    }
    if (!emitExpr(valueExpr, localsIn)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    const int32_t valueLocal = allocTempLocal();
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  return UninitializedStorageInitDropEmitResult::Emitted;
}

UninitializedStorageTakeEmitResult tryEmitUninitializedStorageTakeStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall || !isSimpleCallName(stmt, "take") || stmt.args.size() != 1) {
    return UninitializedStorageTakeEmitResult::NotMatched;
  }

  UninitializedStorageAccessInfo access;
  bool resolved = false;
  if (!resolveUninitializedStorage(stmt.args.front(), localsIn, access, resolved)) {
    return UninitializedStorageTakeEmitResult::Error;
  }
  if (!resolved) {
    return UninitializedStorageTakeEmitResult::NotMatched;
  }
  if (!emitExpr(stmt, localsIn)) {
    return UninitializedStorageTakeEmitResult::Error;
  }
  instructions.push_back({IrOpcode::Pop, 0});
  return UninitializedStorageTakeEmitResult::Emitted;
}

StatementPrintPathSpaceEmitResult tryEmitPrintPathSpaceStatementBuiltin(
    const Expr &stmt,
    const LocalMap &localsIn,
    const EmitPrintArgForStatementFn &emitPrintArg,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const EmitExprForBindingFn &emitExpr,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  PrintBuiltin printBuiltin;
  if (stmt.kind == Expr::Kind::Call && getPrintBuiltin(stmt, printBuiltin)) {
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error = printBuiltin.name + " does not support body arguments";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (stmt.args.size() != 1) {
      error = printBuiltin.name + " requires exactly one argument";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (!emitPrintArg(stmt.args.front(), localsIn, printBuiltin)) {
      return StatementPrintPathSpaceEmitResult::Error;
    }
    return StatementPrintPathSpaceEmitResult::Emitted;
  }

  PathSpaceBuiltin pathSpaceBuiltin;
  if (stmt.kind == Expr::Kind::Call && getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && !resolveDefinitionCall(stmt)) {
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error = pathSpaceBuiltin.name + " does not support body arguments";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (!stmt.templateArgs.empty()) {
      error = pathSpaceBuiltin.name + " does not support template arguments";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
      error = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
              " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
      return StatementPrintPathSpaceEmitResult::Error;
    }
    for (const auto &arg : stmt.args) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        continue;
      }
      if (!emitExpr(arg, localsIn)) {
        return StatementPrintPathSpaceEmitResult::Error;
      }
      instructions.push_back({IrOpcode::Pop, 0});
    }
    return StatementPrintPathSpaceEmitResult::Emitted;
  }

  return StatementPrintPathSpaceEmitResult::NotMatched;
}

ReturnStatementEmitResult tryEmitReturnStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::optional<ReturnStatementInlineContext> &inlineContext,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferArrayElementKind,
    const std::function<void()> &emitFileScopeCleanupAll,
    std::string &error) {
  if (!isReturnCall(stmt)) {
    return ReturnStatementEmitResult::NotMatched;
  }

  auto isSupportedScalarKind = [](LocalInfo::ValueKind kind) {
    return kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64 ||
           kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool ||
           kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64 ||
           kind == LocalInfo::ValueKind::String;
  };

  if (inlineContext.has_value()) {
    const ReturnStatementInlineContext &context = *inlineContext;
    if (stmt.args.empty()) {
      if (!context.returnsVoid) {
        error = "return requires exactly one argument";
        return ReturnStatementEmitResult::Error;
      }
      const size_t jumpIndex = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      if (context.returnJumps) {
        context.returnJumps->push_back(jumpIndex);
      }
      return ReturnStatementEmitResult::Emitted;
    }
    if (stmt.args.size() != 1) {
      error = "return requires exactly one argument";
      return ReturnStatementEmitResult::Error;
    }
    if (context.returnsVoid) {
      error = "return value not allowed for void definition";
      return ReturnStatementEmitResult::Error;
    }
    if (context.returnLocal < 0) {
      error = "native backend missing inline return local";
      return ReturnStatementEmitResult::Error;
    }

    const Expr &valueExpr = stmt.args.front();
    if (!emitExpr(valueExpr, localsIn)) {
      return ReturnStatementEmitResult::Error;
    }

    if (context.returnsArray) {
      const LocalInfo::ValueKind arrayKind = inferArrayElementKind(valueExpr, localsIn);
      if (arrayKind == LocalInfo::ValueKind::Unknown) {
        error = "native backend only supports returning array values";
        return ReturnStatementEmitResult::Error;
      }
      if (arrayKind == LocalInfo::ValueKind::String) {
        error = "native backend does not support string array return types";
        return ReturnStatementEmitResult::Error;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
      const size_t jumpIndex = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      if (context.returnJumps) {
        context.returnJumps->push_back(jumpIndex);
      }
      return ReturnStatementEmitResult::Emitted;
    }

    const LocalInfo::ValueKind returnKind = inferExprKind(valueExpr, localsIn);
    if (!isSupportedScalarKind(returnKind)) {
      error = "native backend only supports returning numeric, bool, or string values";
      return ReturnStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
    const size_t jumpIndex = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    if (context.returnJumps) {
      context.returnJumps->push_back(jumpIndex);
    }
    return ReturnStatementEmitResult::Emitted;
  }

  if (stmt.args.empty()) {
    if (!definitionReturnsVoid) {
      error = "return requires exactly one argument";
      return ReturnStatementEmitResult::Error;
    }
    if (emitFileScopeCleanupAll) {
      emitFileScopeCleanupAll();
    }
    instructions.push_back({IrOpcode::ReturnVoid, 0});
    sawReturn = true;
    return ReturnStatementEmitResult::Emitted;
  }
  if (stmt.args.size() != 1) {
    error = "return requires exactly one argument";
    return ReturnStatementEmitResult::Error;
  }
  if (definitionReturnsVoid) {
    error = "return value not allowed for void definition";
    return ReturnStatementEmitResult::Error;
  }

  const Expr &valueExpr = stmt.args.front();
  if (!emitExpr(valueExpr, localsIn)) {
    return ReturnStatementEmitResult::Error;
  }
  if (emitFileScopeCleanupAll) {
    emitFileScopeCleanupAll();
  }

  const LocalInfo::ValueKind arrayKind = inferArrayElementKind(valueExpr, localsIn);
  if (arrayKind != LocalInfo::ValueKind::Unknown) {
    if (arrayKind == LocalInfo::ValueKind::String) {
      error = "native backend does not support string array return types";
      return ReturnStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::ReturnI64, 0});
    sawReturn = true;
    return ReturnStatementEmitResult::Emitted;
  }

  const LocalInfo::ValueKind returnKind = inferExprKind(valueExpr, localsIn);
  if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64 ||
      returnKind == LocalInfo::ValueKind::String) {
    instructions.push_back({IrOpcode::ReturnI64, 0});
  } else if (returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
    instructions.push_back({IrOpcode::ReturnI32, 0});
  } else if (returnKind == LocalInfo::ValueKind::Float32) {
    instructions.push_back({IrOpcode::ReturnF32, 0});
  } else if (returnKind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::ReturnF64, 0});
  } else {
    error = "native backend only supports returning numeric, bool, or string values";
    return ReturnStatementEmitResult::Error;
  }

  sawReturn = true;
  return ReturnStatementEmitResult::Emitted;
}

StatementMatchIfEmitResult tryEmitMatchIfStatement(const Expr &stmt,
                                                   LocalMap &localsIn,
                                                   const EmitExprForBindingFn &emitExpr,
                                                   const InferBindingExprKindFn &inferExprKind,
                                                   const EmitBlockForBindingFn &emitBlock,
                                                   const EmitStatementForBindingFn &emitStatement,
                                                   std::vector<IrInstruction> &instructions,
                                                   std::string &error) {
  if (isMatchCall(stmt)) {
    Expr expanded;
    if (!lowerMatchToIf(stmt, expanded, error)) {
      return StatementMatchIfEmitResult::Error;
    }
    if (!emitStatement(expanded, localsIn)) {
      return StatementMatchIfEmitResult::Error;
    }
    return StatementMatchIfEmitResult::Emitted;
  }

  if (!isIfCall(stmt)) {
    return StatementMatchIfEmitResult::NotMatched;
  }

  if (stmt.args.size() != 3) {
    error = "if requires condition, then, else";
    return StatementMatchIfEmitResult::Error;
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    error = "if does not accept trailing block arguments";
    return StatementMatchIfEmitResult::Error;
  }
  if (!emitExpr(stmt.args[0], localsIn)) {
    return StatementMatchIfEmitResult::Error;
  }
  const LocalInfo::ValueKind condKind = inferExprKind(stmt.args[0], localsIn);
  if (condKind != LocalInfo::ValueKind::Bool) {
    error = "if condition requires bool";
    return StatementMatchIfEmitResult::Error;
  }

  const Expr &thenArg = stmt.args[1];
  const Expr &elseArg = stmt.args[2];
  auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  if (!isIfBlockEnvelope(thenArg) || !isIfBlockEnvelope(elseArg)) {
    error = "if branches require block envelopes";
    return StatementMatchIfEmitResult::Error;
  }

  auto emitBranch = [&](const Expr &branch, LocalMap &branchLocals) -> bool { return emitBlock(branch, branchLocals); };
  const size_t jumpIfZeroIndex = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  LocalMap thenLocals = localsIn;
  if (!emitBranch(thenArg, thenLocals)) {
    return StatementMatchIfEmitResult::Error;
  }
  const size_t jumpIndex = instructions.size();
  instructions.push_back({IrOpcode::Jump, 0});
  const size_t elseIndex = instructions.size();
  instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
  LocalMap elseLocals = localsIn;
  if (!emitBranch(elseArg, elseLocals)) {
    return StatementMatchIfEmitResult::Error;
  }
  const size_t endIndex = instructions.size();
  instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
  return StatementMatchIfEmitResult::Emitted;
}

} // namespace primec::ir_lowerer

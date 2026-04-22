#include "IrLowererStatementBindingInternal.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStringCallHelpers.h"
#include "IrLowererStringLiteralHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

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
  if (!emitStringValueForCallFromLocals(
          init,
          localsIn,
          internString,
          [&](IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
          [&](const Expr &expr, std::string &accessName) { return getBuiltinArrayAccessName(expr, accessName); },
          [&](const Expr &expr) { return isEntryArgsName(expr, localsIn); },
          [&](const Expr &indexExpr, const std::string &accessName, StringIndexOps &ops, std::string &errorOut) {
            LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              errorOut = "native backend requires integer indices for " + accessName;
              return false;
            }
            ops.pushZero = pushZeroForIndex(indexKind);
            ops.cmpLt = cmpLtForIndex(indexKind);
            ops.cmpGe = cmpGeForIndex(indexKind);
            ops.skipNegativeCheck = (indexKind == LocalInfo::ValueKind::UInt64);
            return true;
          },
          [&](const Expr &expr) { return emitExpr(expr, localsIn); },
          [&](const Expr &expr) { return inferExprKind(expr, localsIn) == LocalInfo::ValueKind::String; },
          allocTempLocal,
          [&]() { return instructions.size(); },
          [&](size_t indexToPatch, int32_t target) { instructions[indexToPatch].imm = target; },
          emitArrayIndexOutOfBounds,
          source,
          index,
          argvChecked,
          error)) {
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

  auto emitFieldPointer = [&](const Expr &receiver,
                              const LocalInfo *receiverLocal,
                              const StructSlotFieldInfo &field,
                              int32_t ptrLocal) -> bool {
    if (receiverLocal != nullptr) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(receiverLocal->index)});
    } else {
      if (!emitExpr(receiver, localsIn)) {
        return false;
      }
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
  auto emitIndirectPointer = [&](const Expr &pointerExpr, int32_t ptrLocal) -> bool {
    if (pointerExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(pointerExpr.name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        return true;
      }
    }
    if (!emitExpr(pointerExpr, localsIn)) {
      return false;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
    return true;
  };

  const Expr &valueExpr = stmt.args.back();
  if (access.location == UninitializedStorageAccessInfo::Location::Local) {
    const LocalInfo &storageInfo = *access.local;
    if (!storageInfo.isFileHandle && !storageInfo.structTypeName.empty()) {
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
    if (!emitFieldPointer(receiver, access.receiver, field, ptrLocal)) {
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

  if (access.location == UninitializedStorageAccessInfo::Location::Indirect) {
    if (access.pointerExpr == nullptr) {
      error = "native backend could not resolve uninitialized pointer target";
      return UninitializedStorageInitDropEmitResult::Error;
    }
    const int32_t ptrLocal = allocTempLocal();
    if (!emitIndirectPointer(*access.pointerExpr, ptrLocal)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    if (!access.typeInfo.structPath.empty()) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(access.typeInfo.structPath, layout)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      if (!emitExpr(valueExpr, localsIn)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
      if (!emitStructCopyFromPtrs(ptrLocal, srcPtrLocal, layout.totalSlots)) {
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
    const EmitExprForBindingFn &emitExpr) {
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
    bool declaredReturnIsReferenceHandle,
    const std::optional<ResultReturnInfo> &resultReturnInfo,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
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

  auto shouldPackResultErrorReturn = [&](const Expr &valueExpr) {
    if (!resultReturnInfo.has_value() || !resultReturnInfo->isResult || !resultReturnInfo->hasValue) {
      return false;
    }
    if (isExplicitPackedResultReturnExpr(valueExpr)) {
      return false;
    }
    if (!resolveResultExprInfo) {
      return true;
    }
    ResultExprInfo resultExprInfo;
    return !(resolveResultExprInfo(valueExpr, localsIn, resultExprInfo) && resultExprInfo.isResult);
  };

  auto emitPackedResultErrorReturn = [&]() {
    instructions.push_back({IrOpcode::PushI64, 4294967296ull});
    instructions.push_back({IrOpcode::MulI64, 0});
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
    auto emitOpaqueReturnHandle = [&](const Expr &exprIn) -> bool {
      if (context.returnsArray || exprIn.kind != Expr::Kind::Name) {
        return false;
      }
      auto it = localsIn.find(exprIn.name);
      if (it == localsIn.end()) {
        return false;
      }
      const LocalInfo &info = it->second;
      const bool isOpaqueHandle =
          info.kind == LocalInfo::Kind::Pointer ||
          (declaredReturnIsReferenceHandle && info.kind == LocalInfo::Kind::Reference) ||
          info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
          info.kind == LocalInfo::Kind::Map || info.kind == LocalInfo::Kind::Buffer ||
          !info.structTypeName.empty() || info.referenceToArray || info.pointerToArray ||
          info.referenceToVector || info.pointerToVector || info.referenceToBuffer || info.pointerToBuffer ||
          info.referenceToMap || info.pointerToMap || info.isFileHandle || info.isResult;
      if (!isOpaqueHandle) {
        return false;
      }
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(info.index)});
      return true;
    };

    if (!emitOpaqueReturnHandle(valueExpr) && !emitExpr(valueExpr, localsIn)) {
      return ReturnStatementEmitResult::Error;
    }
    if (shouldPackResultErrorReturn(valueExpr)) {
      emitPackedResultErrorReturn();
    }

    if (context.returnsArray) {
      LocalInfo::ValueKind arrayKind = inferArrayElementKind(valueExpr, localsIn);
      if (arrayKind == LocalInfo::ValueKind::Unknown) {
        arrayKind = context.returnKind;
      }
      if (arrayKind == LocalInfo::ValueKind::Unknown) {
        const LocalInfo::ValueKind scalarKind = inferExprKind(valueExpr, localsIn);
        const bool isOpaqueAggregateHandle =
            scalarKind == LocalInfo::ValueKind::Int64 || scalarKind == LocalInfo::ValueKind::UInt64;
        if (isSupportedScalarKind(scalarKind) && !isOpaqueAggregateHandle) {
          error = "native backend only supports returning array values"
                  " [inline scalar=" + typeNameForValueKind(scalarKind) +
                  ", declared=" + typeNameForValueKind(context.returnKind) + "]";
          return ReturnStatementEmitResult::Error;
        }
      } else if (arrayKind == LocalInfo::ValueKind::String) {
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
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
      const size_t jumpIndex = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      if (context.returnJumps) {
        context.returnJumps->push_back(jumpIndex);
      }
      return ReturnStatementEmitResult::Emitted;
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
  auto emitOpaqueReturnHandle = [&](const Expr &exprIn) -> bool {
    if (exprIn.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(exprIn.name);
    if (it == localsIn.end()) {
      return false;
    }
    const LocalInfo &info = it->second;
    const bool isOpaqueHandle =
        info.kind == LocalInfo::Kind::Pointer ||
        (declaredReturnIsReferenceHandle && info.kind == LocalInfo::Kind::Reference) ||
        info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
        info.kind == LocalInfo::Kind::Map || info.kind == LocalInfo::Kind::Buffer || !info.structTypeName.empty() ||
        info.referenceToArray || info.pointerToArray || info.referenceToVector || info.pointerToVector ||
        info.referenceToBuffer || info.pointerToBuffer || info.referenceToMap || info.pointerToMap ||
        info.isFileHandle || info.isResult;
    if (!isOpaqueHandle) {
      return false;
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(info.index)});
    return true;
  };

  if (!emitOpaqueReturnHandle(valueExpr) && !emitExpr(valueExpr, localsIn)) {
    return ReturnStatementEmitResult::Error;
  }
  if (shouldPackResultErrorReturn(valueExpr)) {
    emitPackedResultErrorReturn();
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
    instructions.push_back({IrOpcode::ReturnI64, 0});
  }

  sawReturn = true;
  return ReturnStatementEmitResult::Emitted;
}

ReturnStatementEmitResult tryEmitReturnStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::optional<ReturnStatementInlineContext> &inlineContext,
    const std::optional<ResultReturnInfo> &resultReturnInfo,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferArrayElementKind,
    const std::function<void()> &emitFileScopeCleanupAll,
    std::string &error) {
  return tryEmitReturnStatement(stmt,
                                localsIn,
                                instructions,
                                inlineContext,
                                false,
                                resultReturnInfo,
                                definitionReturnsVoid,
                                sawReturn,
                                emitExpr,
                                inferExprKind,
                                resolveResultExprInfo,
                                inferArrayElementKind,
                                emitFileScopeCleanupAll,
                                error);
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
  LocalInfo::ValueKind condKind = inferExprKind(stmt.args[0], localsIn);
  if (condKind == LocalInfo::ValueKind::Unknown) {
    std::string builtinComparison;
    if (getBuiltinComparisonName(stmt.args[0], builtinComparison)) {
      condKind = LocalInfo::ValueKind::Bool;
    }
  }
  const bool isIntegralCondition =
      condKind == LocalInfo::ValueKind::Int32 || condKind == LocalInfo::ValueKind::Int64 ||
      condKind == LocalInfo::ValueKind::UInt64;
  if (condKind != LocalInfo::ValueKind::Bool && !isIntegralCondition) {
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

#include "IrLowererLowerStatementsCallsStep.h"

#include <string_view>

#include "IrLowererCallHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

enum class VectorMutationStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};

bool isNamedArgument(const Expr &expr, size_t index, std::string_view name) {
  return index < expr.argNames.size() && expr.argNames[index].has_value() &&
         *expr.argNames[index] == name;
}

bool isVectorMutationHelperName(std::string_view helperName) {
  return helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isVectorNoPayloadHelperName(std::string_view helperName) {
  return helperName == "pop" || helperName == "clear";
}

bool isVectorIndexedRemovalHelperName(std::string_view helperName) {
  return helperName == "remove_at" || helperName == "remove_swap";
}

std::string_view vectorMutationPayloadName(std::string_view helperName) {
  if (helperName == "reserve") {
    return "capacity";
  }
  if (isVectorIndexedRemovalHelperName(helperName)) {
    return "index";
  }
  return "value";
}

std::string resolveDirectHelperPath(const Expr &callExpr) {
  if (!callExpr.name.empty() && callExpr.name.front() == '/') {
    return callExpr.name;
  }
  if (!callExpr.namespacePrefix.empty()) {
    std::string scoped = callExpr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + callExpr.name;
  }
  return callExpr.name;
}

bool resolveVectorMutationHelperName(const SemanticProgram *semanticProgram,
                                     const Expr &stmt,
                                     std::string &helperNameOut,
                                     bool &isSemanticMatchOut) {
  helperNameOut.clear();
  isSemanticMatchOut = false;
  if (resolvePublishedSemanticStdlibSurfaceMemberName(
          semanticProgram,
          stmt,
          StdlibSurfaceId::CollectionsManifestSurface0,
          helperNameOut)) {
    isSemanticMatchOut = true;
    return isVectorMutationHelperName(helperNameOut);
  }

  const std::string directHelperPath = resolveDirectHelperPath(stmt);
  if (isCanonicalPublishedStdlibSurfaceHelperPath(
          directHelperPath, StdlibSurfaceId::CollectionsManifestSurface0) &&
      resolvePublishedStdlibSurfaceMemberName(
          directHelperPath,
          StdlibSurfaceId::CollectionsManifestSurface0,
          helperNameOut)) {
    isSemanticMatchOut = true;
    return isVectorMutationHelperName(helperNameOut);
  }

  if (stmt.namespacePrefix.empty() && stmt.name.find('/') == std::string::npos &&
      isVectorMutationHelperName(stmt.name)) {
    helperNameOut = stmt.name;
    return true;
  }
  return false;
}

bool selectVectorMutationArgs(const Expr &stmt,
                              std::string_view helperName,
                              const Expr *&valuesArgOut,
                              const Expr *&payloadArgOut) {
  valuesArgOut = nullptr;
  payloadArgOut = nullptr;
  if (stmt.isMethodCall) {
    if (stmt.args.size() != 2) {
      return false;
    }
    valuesArgOut = &stmt.args[0];
    payloadArgOut = &stmt.args[1];
    return true;
  }

  if (stmt.args.size() != 2) {
    return false;
  }

  const std::string_view payloadName = vectorMutationPayloadName(helperName);
  for (size_t index = 0; index < stmt.args.size(); ++index) {
    if (isNamedArgument(stmt, index, "values")) {
      if (valuesArgOut != nullptr) {
        return false;
      }
      valuesArgOut = &stmt.args[index];
      continue;
    }
    if (isNamedArgument(stmt, index, payloadName)) {
      if (payloadArgOut != nullptr) {
        return false;
      }
      payloadArgOut = &stmt.args[index];
      continue;
    }
    if (index < stmt.argNames.size() && stmt.argNames[index].has_value()) {
      return false;
    }
  }

  size_t positionalIndex = 0;
  for (size_t index = 0; index < stmt.args.size(); ++index) {
    if (index < stmt.argNames.size() && stmt.argNames[index].has_value()) {
      continue;
    }
    if (positionalIndex == 0 && valuesArgOut == nullptr) {
      valuesArgOut = &stmt.args[index];
    } else if (positionalIndex <= 1 && payloadArgOut == nullptr) {
      payloadArgOut = &stmt.args[index];
    } else {
      return false;
    }
    ++positionalIndex;
  }

  return valuesArgOut != nullptr && payloadArgOut != nullptr;
}

VectorMutationStatementEmitResult tryEmitCanonicalVectorMutationStatement(
    const LowerStatementsCallsStepInput &input,
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    std::string &errorOut) {
  if (stmt.kind != Expr::Kind::Call || stmt.hasBodyArguments ||
      !stmt.bodyArguments.empty() || input.semanticProgram == nullptr ||
      !input.emitVectorCapacityExceeded || !input.emitVectorReserveNegative ||
      !input.emitVectorReserveExceeded) {
    return VectorMutationStatementEmitResult::NotMatched;
  }

  std::string helperName;
  bool isSemanticMatch = false;
  if (!resolveVectorMutationHelperName(
          input.semanticProgram, stmt, helperName, isSemanticMatch)) {
    return VectorMutationStatementEmitResult::NotMatched;
  }

  if (isVectorNoPayloadHelperName(helperName)) {
    if (stmt.isMethodCall) {
      errorOut = "missing semantic-product method-call target: " + helperName;
      return VectorMutationStatementEmitResult::Error;
    }
    if (stmt.args.size() != 1) {
      return VectorMutationStatementEmitResult::NotMatched;
    }
    const Expr *valuesArg = &stmt.args[0];
    const ArrayVectorAccessTargetInfo targetInfo =
        resolveArrayVectorAccessTargetInfo(*valuesArg,
                                           localsIn,
                                           {},
                                           input.semanticProgram,
                                           input.semanticIndex);
    if (!targetInfo.isArrayOrVectorTarget) {
      return VectorMutationStatementEmitResult::NotMatched;
    }
    if (!targetInfo.isVectorTarget || targetInfo.isSoaVector ||
        targetInfo.isKeyValueTarget || targetInfo.isWrappedKeyValueTarget) {
      errorOut = helperName + " requires vector binding";
      return VectorMutationStatementEmitResult::Error;
    }
    if (!isSemanticMatch || targetInfo.elemSlotCount > 1) {
      return VectorMutationStatementEmitResult::NotMatched;
    }

    const int32_t valuesPtrLocal = input.allocTempLocal();
    if (!input.emitExpr(*valuesArg, localsIn)) {
      return VectorMutationStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valuesPtrLocal)});

    if (helperName == "clear") {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesPtrLocal)});
      instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
      instructions.push_back({IrOpcode::PushI32, 0});
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      instructions.push_back({IrOpcode::Pop, 0});
      return VectorMutationStatementEmitResult::Emitted;
    }

    // pop: trap if empty, then decrement count
    if (!input.emitVectorPopOnEmpty) {
      return VectorMutationStatementEmitResult::NotMatched;
    }
    const int32_t countLocal = input.allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesPtrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpEqI32, 0});
    const size_t okJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    input.emitVectorPopOnEmpty();
    instructions[okJump].imm = static_cast<uint64_t>(instructions.size());

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesPtrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorMutationStatementEmitResult::Emitted;
  }

  const Expr *valuesArg = nullptr;
  const Expr *payloadArg = nullptr;
  if (!selectVectorMutationArgs(stmt, helperName, valuesArg, payloadArg) ||
      valuesArg == nullptr || payloadArg == nullptr) {
    return VectorMutationStatementEmitResult::NotMatched;
  }

  const ArrayVectorAccessTargetInfo targetInfo =
      resolveArrayVectorAccessTargetInfo(*valuesArg,
                                         localsIn,
                                         {},
                                         input.semanticProgram,
                                         input.semanticIndex);
  if (!targetInfo.isArrayOrVectorTarget) {
    return VectorMutationStatementEmitResult::NotMatched;
  }
  if (!targetInfo.isVectorTarget || targetInfo.isSoaVector ||
      targetInfo.isKeyValueTarget || targetInfo.isWrappedKeyValueTarget) {
    errorOut = helperName + " requires vector binding";
    return VectorMutationStatementEmitResult::Error;
  }
  if (!isSemanticMatch) {
    return VectorMutationStatementEmitResult::NotMatched;
  }
  const LocalInfo::ValueKind payloadKind = input.inferExprKind(*payloadArg, localsIn);
  if (isVectorIndexedRemovalHelperName(helperName)) {
    if (stmt.isMethodCall && payloadKind != LocalInfo::ValueKind::Int32) {
      errorOut = helperName + " requires integer index";
      return VectorMutationStatementEmitResult::Error;
    }
    return VectorMutationStatementEmitResult::NotMatched;
  }
  if (helperName == "reserve" && payloadKind != LocalInfo::ValueKind::Int32) {
    if (stmt.isMethodCall) {
      errorOut = "reserve requires integer capacity";
      return VectorMutationStatementEmitResult::Error;
    }
    return VectorMutationStatementEmitResult::NotMatched;
  }

  // elemSlotCountForBuffer: slots per element in the backing buffer.
  // For primitive elements this is 1; for struct elements it's the struct's totalSlots.
  // Must be resolved before defining lambdas that capture it.
  int32_t elemSlotCountForBuffer = 1;
  if (helperName == "push") {
    if (payloadKind == LocalInfo::ValueKind::Unknown) {
      // Struct push: resolve the element slot count from the payload struct layout.
      if (!input.inferStructExprPath || !input.resolveStructSlotLayout) {
        return VectorMutationStatementEmitResult::NotMatched;
      }
      const std::string payloadStructPath = input.inferStructExprPath(*payloadArg, localsIn);
      if (payloadStructPath.empty()) {
        return VectorMutationStatementEmitResult::NotMatched;
      }
      StructSlotLayoutInfo payloadLayout;
      if (!input.resolveStructSlotLayout(payloadStructPath, payloadLayout) ||
          payloadLayout.totalSlots <= 0) {
        return VectorMutationStatementEmitResult::NotMatched;
      }
      elemSlotCountForBuffer = payloadLayout.totalSlots;
    } else {
      // Primitive push: check type compatibility.
      if (payloadKind == LocalInfo::ValueKind::String ||
          (targetInfo.elemKind != LocalInfo::ValueKind::Unknown &&
           targetInfo.elemKind != payloadKind)) {
        return VectorMutationStatementEmitResult::NotMatched;
      }
    }
  }

  auto emitFieldAddress = [&](int32_t valuesPtrLocal, int32_t slotOffset) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesPtrLocal)});
    if (slotOffset != 0) {
      instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
  };
  auto emitFieldLoad = [&](int32_t valuesPtrLocal, int32_t slotOffset) {
    emitFieldAddress(valuesPtrLocal, slotOffset);
    instructions.push_back({IrOpcode::LoadIndirect, 0});
  };
  auto emitFieldStoreFromLocal = [&](int32_t valuesPtrLocal,
                                     int32_t slotOffset,
                                     int32_t sourceLocal) {
    emitFieldAddress(valuesPtrLocal, slotOffset);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
  };
  auto emitTrapIfStackTrue = [&](const std::function<void()> &emitTrap) {
    const size_t okJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitTrap();
    instructions[okJump].imm = static_cast<uint64_t>(instructions.size());
  };
  auto emitVectorShapeChecks = [&](int32_t valuesPtrLocal,
                                   int32_t countLocal,
                                   int32_t capacityLocal) {
    emitFieldLoad(valuesPtrLocal, 1);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});
    emitFieldLoad(valuesPtrLocal, 2);
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    emitTrapIfStackTrue(input.emitArrayIndexOutOfBounds);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    emitTrapIfStackTrue(input.emitVectorReserveNegative);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::CmpGtI32, 0});
    emitTrapIfStackTrue(input.emitVectorCapacityExceeded);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, 1073741823});
    instructions.push_back({IrOpcode::CmpGtI32, 0});
    emitTrapIfStackTrue(input.emitVectorCapacityExceeded);
  };
  auto emitReallocDataToCapacity = [&](int32_t valuesPtrLocal, int32_t capacityLocal) {
    emitFieldAddress(valuesPtrLocal, 3);
    emitFieldLoad(valuesPtrLocal, 3);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    if (elemSlotCountForBuffer > 1) {
      instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(elemSlotCountForBuffer)});
      instructions.push_back({IrOpcode::MulI32, 0});
    }
    instructions.push_back({IrOpcode::HeapRealloc, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    emitFieldStoreFromLocal(valuesPtrLocal, 2, capacityLocal);
  };

  const int32_t valuesPtrLocal = input.allocTempLocal();
  if (!input.emitExpr(*valuesArg, localsIn)) {
    return VectorMutationStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valuesPtrLocal)});

  const int32_t payloadLocal = input.allocTempLocal();
  if (!input.emitExpr(*payloadArg, localsIn)) {
    return VectorMutationStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadLocal)});

  const int32_t countLocal = input.allocTempLocal();
  const int32_t capacityLocal = input.allocTempLocal();
  emitVectorShapeChecks(valuesPtrLocal, countLocal, capacityLocal);

  if (helperName == "reserve") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    emitTrapIfStackTrue(input.emitVectorReserveNegative);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
    instructions.push_back({IrOpcode::PushI32, 1073741823});
    instructions.push_back({IrOpcode::CmpGtI32, 0});
    emitTrapIfStackTrue(input.emitVectorReserveExceeded);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::CmpGtI32, 0});
    const size_t skipGrowJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitReallocDataToCapacity(valuesPtrLocal, payloadLocal);
    instructions[skipGrowJump].imm = static_cast<uint64_t>(instructions.size());
    return VectorMutationStatementEmitResult::Emitted;
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
  instructions.push_back({IrOpcode::CmpEqI32, 0});
  const size_t skipGrowJump = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
  instructions.push_back({IrOpcode::PushI32, 536870911});
  instructions.push_back({IrOpcode::CmpGtI32, 0});
  emitTrapIfStackTrue(input.emitVectorCapacityExceeded);

  const int32_t nextCapacityLocal = input.allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::CmpEqI32, 0});
  const size_t useDoubleJump = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(nextCapacityLocal)});
  const size_t afterNextCapacityJump = instructions.size();
  instructions.push_back({IrOpcode::Jump, 0});
  instructions[useDoubleJump].imm = static_cast<uint64_t>(instructions.size());
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(nextCapacityLocal)});
  instructions[afterNextCapacityJump].imm = static_cast<uint64_t>(instructions.size());
  emitReallocDataToCapacity(valuesPtrLocal, nextCapacityLocal);
  instructions[skipGrowJump].imm = static_cast<uint64_t>(instructions.size());

  if (elemSlotCountForBuffer > 1) {
    const int32_t targetAddrLocal = input.allocTempLocal();
    emitFieldLoad(valuesPtrLocal, 3);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(elemSlotCountForBuffer * IrSlotBytesI32)});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetAddrLocal)});
    emitStructCopyFromPtrs(instructions, targetAddrLocal, payloadLocal, elemSlotCountForBuffer);
  } else {
    emitFieldLoad(valuesPtrLocal, 3);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
  }

  emitFieldAddress(valuesPtrLocal, 1);
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return VectorMutationStatementEmitResult::Emitted;
}

} // namespace

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut) {
  if (!input.inferExprKind) {
    errorOut = "native backend missing statements/calls step dependency: inferExprKind";
    return false;
  }
  if (!input.emitExpr) {
    errorOut = "native backend missing statements/calls step dependency: emitExpr";
    return false;
  }
  if (!input.allocTempLocal) {
    errorOut = "native backend missing statements/calls step dependency: allocTempLocal";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing statements/calls step dependency: resolveExprPath";
    return false;
  }
  if (!input.findDefinitionByPath) {
    errorOut = "native backend missing statements/calls step dependency: findDefinitionByPath";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing statements/calls step dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing statements/calls step dependency: isStringCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing statements/calls step dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.resolveMethodCallDefinition) {
    errorOut = "native backend missing statements/calls step dependency: resolveMethodCallDefinition";
    return false;
  }
  if (!input.resolveDefinitionCall) {
    errorOut = "native backend missing statements/calls step dependency: resolveDefinitionCall";
    return false;
  }
  if (!input.getReturnInfo) {
    errorOut = "native backend missing statements/calls step dependency: getReturnInfo";
    return false;
  }
  if (!input.emitInlineDefinitionCall) {
    errorOut = "native backend missing statements/calls step dependency: emitInlineDefinitionCall";
    return false;
  }
  if (!input.emitArrayIndexOutOfBounds) {
    errorOut = "native backend missing statements/calls step dependency: emitArrayIndexOutOfBounds";
    return false;
  }
  if (input.instructions == nullptr) {
    errorOut = "native backend missing statements/calls step dependency: instructions";
    return false;
  }

  auto &instructions = *input.instructions;
  const auto bufferStoreResult = tryEmitBufferStoreStatement(
      stmt,
      localsIn,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.inferExprKind(valueExpr, valueLocals); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      [&]() { return input.allocTempLocal(); },
      instructions,
      errorOut,
      input.semanticProgram,
      input.semanticIndex);
  if (bufferStoreResult == BufferStoreStatementEmitResult::Error) {
    return false;
  }
  if (bufferStoreResult == BufferStoreStatementEmitResult::Emitted) {
    return true;
  }

  const auto dispatchResult = tryEmitDispatchStatement(
      stmt,
      localsIn,
      [&](const Expr &valueExpr) { return input.resolveExprPath(valueExpr); },
      [&](const std::string &path) { return input.findDefinitionByPath(path); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.inferExprKind(valueExpr, valueLocals); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      [&]() { return input.allocTempLocal(); },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
      },
      instructions,
      errorOut,
      input.semanticProgram,
      input.semanticIndex);
  if (dispatchResult == DispatchStatementEmitResult::Error) {
    return false;
  }
  if (dispatchResult == DispatchStatementEmitResult::Emitted) {
    return true;
  }

  const auto vectorMutationResult = tryEmitCanonicalVectorMutationStatement(
      input, stmt, localsIn, instructions, errorOut);
  if (vectorMutationResult == VectorMutationStatementEmitResult::Error) {
    return false;
  }
  if (vectorMutationResult == VectorMutationStatementEmitResult::Emitted) {
    return true;
  }

  const auto directCallResult = tryEmitDirectCallStatement(
      stmt,
      localsIn,
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isArrayCountCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isStringCountCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isVectorCapacityCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.emitExpr(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) {
        return input.resolveMethodCallDefinition(callExpr, callLocals);
      },
      [&](const Expr &callExpr) { return input.resolveDefinitionCall(callExpr); },
      [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return input.getReturnInfo(definitionPath, returnInfo); },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
      },
      [&]() { input.emitArrayIndexOutOfBounds(); },
      instructions,
      errorOut,
      input.semanticProgram,
      input.semanticIndex);
  if (directCallResult == DirectCallStatementEmitResult::Error) {
    return false;
  }
  if (directCallResult == DirectCallStatementEmitResult::Emitted) {
    return true;
  }

  const auto assignOrExprResult = emitAssignOrExprStatementWithPop(
      stmt,
      localsIn,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      instructions);
  if (assignOrExprResult == AssignOrExprStatementEmitResult::Error) {
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer

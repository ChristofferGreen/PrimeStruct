#include "primec/support/CollectionHelperNames.h"
  allocTempLocal = [&]() -> int32_t {
    return nextLocal++;
  };
  emitStructCopyFromPtrs = [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) -> bool {
    return ir_lowerer::emitStructCopyFromPtrs(function.instructions, destPtrLocal, srcPtrLocal, slotCount);
  };
  emitStructCopySlots = [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) -> bool {
    return ir_lowerer::emitStructCopySlots(
        function.instructions, destBaseLocal, srcPtrLocal, slotCount, [&]() { return allocTempLocal(); });
  };
  // Destroys each element of a builtin vector record (count in slot 1, data pointer in slot 3)
  // and frees its data buffer.
  auto emitBuiltinVectorDrop = [&](const LowerSetupStageState::DropEntry &entry) -> bool {
    auto &instructions = function.instructions;
    const int32_t dataLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(entry.ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, 3 * IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(dataLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal)});
    instructions.push_back({IrOpcode::PushI64, 0});
    instructions.push_back({IrOpcode::CmpEqI64, 0});
    const size_t skipEmptyJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    const size_t toEndJump = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    instructions[skipEmptyJump].imm = static_cast<uint64_t>(instructions.size());
    if (!entry.structPath.empty()) {
      const int32_t countLocal = allocTempLocal();
      const int32_t indexLocal = allocTempLocal();
      const int32_t elementPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(entry.ptrLocal)});
      instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
      instructions.push_back({IrOpcode::LoadIndirect, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});
      instructions.push_back({IrOpcode::PushI32, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
      const size_t loopStart = instructions.size();
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
      instructions.push_back({IrOpcode::CmpLtI32, 0});
      const size_t loopExitJump = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      instructions.push_back(
          {IrOpcode::PushI32,
           static_cast<uint64_t>(entry.builtinVectorElementSlots * IrSlotBytesI32)});
      instructions.push_back({IrOpcode::MulI32, 0});
      instructions.push_back({IrOpcode::AddI64, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(elementPtrLocal)});
      if (!ir_lowerer::emitStructDestroyHelpersFromPtr(
              elementPtrLocal,
              entry.structPath,
              [&](const std::string &path) {
                return ir_lowerer::findStackDestroyHelper(defMap, path);
              },
              [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                return resolveStructSlotLayout(path, layoutOut);
              },
              [&]() { return allocTempLocal(); },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              LocalMap{},
              emitInlineDefinitionCall,
              error)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      function.instructions.push_back({IrOpcode::PushI32, 1});
      function.instructions.push_back({IrOpcode::AddI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});
      function.instructions[loopExitJump].imm = static_cast<uint64_t>(function.instructions.size());
    }
    function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal)});
    function.instructions.push_back({IrOpcode::HeapFree, 0});
    function.instructions[toEndJump].imm = static_cast<uint64_t>(function.instructions.size());
    return true;
  };
  // Destroys an owning struct local whose drop flag is still set, then clears the flag, so a
  // cleanup that runs again on another exit path is a no-op.
  auto emitDropEntryCleanup = [&](const LowerSetupStageState::DropEntry &entry) {
    function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(entry.flagLocal)});
    const size_t skipJump = function.instructions.size();
    function.instructions.push_back({IrOpcode::JumpIfZero, 0});
    function.instructions.push_back({IrOpcode::PushI32, 0});
    function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(entry.flagLocal)});
    if (entry.sumDef != nullptr) {
      const bool emitted = sumHelpers.emitActiveSumPayloadDestroyFromSumPtr(
          *entry.sumDef, entry.ptrLocal, LocalMap{});
      function.instructions[skipJump].imm = static_cast<uint64_t>(function.instructions.size());
      return emitted;
    }
    if (entry.builtinVectorElementSlots > 0) {
      const bool emitted = emitBuiltinVectorDrop(entry);
      function.instructions[skipJump].imm = static_cast<uint64_t>(function.instructions.size());
      return emitted;
    }
    const bool emitted = ir_lowerer::emitStructDestroyHelpersFromPtr(
        entry.ptrLocal,
        entry.structPath,
        [&](const std::string &path) { return ir_lowerer::findStackDestroyHelper(defMap, path); },
        [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
          return resolveStructSlotLayout(path, layoutOut);
        },
        [&]() { return allocTempLocal(); },
        [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
        LocalMap{},
        emitInlineDefinitionCall,
        error);
    function.instructions[skipJump].imm = static_cast<uint64_t>(function.instructions.size());
    return emitted;
  };
  emitFileScopeCleanup = [&](const std::vector<int32_t> &scopeRef) {
    // A copy: a destroy helper's inline call pushes scopes of its own, which can reallocate the
    // stack `scopeRef` lives in.
    const std::vector<int32_t> scope = scopeRef;
    for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
      if (*it >= 0) {
        ir_lowerer::emitFileScopeCleanup(function.instructions, std::vector<int32_t>{*it});
      } else {
        const LowerSetupStageState::DropEntry entry =
            setupStage.dropEntries[static_cast<size_t>(-*it - 1)];
        (void)emitDropEntryCleanup(entry);
      }
    }
  };
  emitFileScopeCleanupAll = [&]() {
    for (size_t depth = fileScopeStack.size(); depth > 0; --depth) {
      const std::vector<int32_t> scope = fileScopeStack[depth - 1];
      emitFileScopeCleanup(scope);
    }
  };
  pushFileScope = [&]() { fileScopeStack.emplace_back(); };
  popFileScope = [&]() { fileScopeStack.pop_back(); };

  emitBlock = [&](const Expr &blockExpr, LocalMap &blockLocals) -> bool {
    if (blockExpr.kind != Expr::Kind::Call) {
      error = "native backend expects if branch blocks to be calls";
      return false;
    }
    if (!blockExpr.args.empty()) {
      error = "native backend does not support arguments on if branch blocks";
      return false;
    }
    OnErrorScope onErrorScope(currentOnError, std::nullopt);
    pushFileScope();
    for (const auto &stmt : blockExpr.bodyArguments) {
      if (!emitStatement(stmt, blockLocals)) {
        return false;
      }
    }
    emitFileScopeCleanup(fileScopeStack.back());
    popFileScope();
    return true;
  };

  emitFloatLiteral = [&](const Expr &expr) -> bool {
    return ir_lowerer::emitFloatLiteral(function.instructions, expr, error);
  };

  emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    return ir_lowerer::emitCompareToZero(function.instructions, kind, equals, error);
  };

  resolveDefinitionCall = ir_lowerer::makeResolveDefinitionCall(
      defMap,
      resolveExprPath,
      callResolutionAdapters.semanticProgram);
  resolveResultExprInfo = ir_lowerer::makeResolveResultExprInfoFromLocals(
      [&](const Expr &callExpr, const LocalMap &localsForCall) -> const Definition * {
        return resolveMethodCallDefinition(callExpr, localsForCall);
      },
      resolveDefinitionCall,
      [&](const std::string &path, ReturnInfo &info) -> bool {
        return getReturnInfo && getReturnInfo(path, info);
      },
      [&](const Expr &expr, const LocalMap &localsForKind) -> LocalInfo::ValueKind {
        return inferExprKind(expr, localsForKind);
      },
      callResolutionAdapters.semanticProgram,
      &callResolutionAdapters.semanticProductTargets.semanticIndex,
      &error);

  emitStringValueForCall = [&](const Expr &arg,
                               const LocalMap &callerLocals,
                               LocalInfo::StringSource &sourceOut,
                               int32_t &stringIndexOut,
                               bool &argvCheckedOut) -> bool {
    return ir_lowerer::emitStringValueForCallFromLocals(
        arg,
        callerLocals,
        internString,
        [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
        [&](const Expr &callExpr, std::string &accessName) {
          if (getBuiltinArrayAccessName(callExpr, accessName)) {
            return true;
          }
          if (callExpr.name == "at" || callExpr.name == "/at") {
            accessName = "at";
            return true;
          }
          if (callExpr.name == "at_unsafe" || callExpr.name == "/at_unsafe") {
            accessName = "at_unsafe";
            return true;
          }
          return false;
        },
        [&](const Expr &targetExpr) { return isEntryArgsName(targetExpr, callerLocals); },
        [&](const Expr &indexExpr,
            const std::string &accessName,
            ir_lowerer::StringIndexOps &ops,
            std::string &helperError) -> bool {
          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(indexExpr, callerLocals));
          if (!isSupportedIndexKind(indexKind)) {
            helperError = "native backend requires integer indices for " + accessName;
            return false;
          }
          ops.pushZero = pushZeroForIndex(indexKind);
          ops.cmpLt = cmpLtForIndex(indexKind);
          ops.cmpGe = cmpGeForIndex(indexKind);
          ops.skipNegativeCheck = (indexKind == LocalInfo::ValueKind::UInt64);
          return true;
        },
        [&](const Expr &exprToEmit) { return emitExpr(exprToEmit, callerLocals); },
        [&](const Expr &callExpr) {
          if (inferExprKind(callExpr, callerLocals) ==
              LocalInfo::ValueKind::String) {
            return true;
          }
          if (callResolutionAdapters.semanticProgram == nullptr) {
            return false;
          }
          const auto *queryFact = ir_lowerer::findSemanticProductQueryFact(
              callResolutionAdapters.semanticProgram,
              callResolutionAdapters.semanticProductTargets.semanticIndex,
              callExpr);
          if (queryFact == nullptr) {
            return false;
          }
          const std::string queryType = resolveSemanticProductTypeText(
              callResolutionAdapters.semanticProgram, queryFact->queryTypeText,
              queryFact->queryTypeTextId);
          const std::string bindingType = resolveSemanticProductTypeText(
              callResolutionAdapters.semanticProgram,
              queryFact->bindingTypeText,
              queryFact->bindingTypeTextId);
          return queryType == "string" || queryType == primec::collection_helpers::kRootedString ||
                 bindingType == "string" || bindingType == primec::collection_helpers::kRootedString;
        },
        [&](const Expr &valueExpr) { return inferExprKind(valueExpr, callerLocals); },
        [&]() { return allocTempLocal(); },
        [&]() { return function.instructions.size(); },
        [&](size_t index, int32_t imm) { function.instructions.at(index).imm = imm; },
        [&]() { emitArrayIndexOutOfBounds(); },
        sourceOut,
        stringIndexOut,
        argvCheckedOut,
        error);
  };

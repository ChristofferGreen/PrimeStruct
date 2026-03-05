    const auto bufferStoreResult = ir_lowerer::tryEmitBufferStoreStatement(
        stmt,
        localsIn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&]() { return allocTempLocal(); },
        function.instructions,
        error);
    if (bufferStoreResult == ir_lowerer::BufferStoreStatementEmitResult::Error) {
      return false;
    }
    if (bufferStoreResult == ir_lowerer::BufferStoreStatementEmitResult::Emitted) {
      return true;
    }
    const auto dispatchResult = ir_lowerer::tryEmitDispatchStatement(
        stmt,
        localsIn,
        [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
        [&](const std::string &path) -> const Definition * {
          auto it = defMap.find(path);
          return it == defMap.end() ? nullptr : it->second;
        },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&]() { return allocTempLocal(); },
        [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
          return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
        },
        function.instructions,
        error);
    if (dispatchResult == ir_lowerer::DispatchStatementEmitResult::Error) {
      return false;
    }
    if (dispatchResult == ir_lowerer::DispatchStatementEmitResult::Emitted) {
      return true;
    }
    const auto directCallResult = ir_lowerer::tryEmitDirectCallStatement(
        stmt,
        localsIn,
        [&](const Expr &callExpr, const LocalMap &callLocals) { return isArrayCountCall(callExpr, callLocals); },
        [&](const Expr &callExpr, const LocalMap &callLocals) { return isStringCountCall(callExpr, callLocals); },
        [&](const Expr &callExpr, const LocalMap &callLocals) { return isVectorCapacityCall(callExpr, callLocals); },
        [&](const Expr &callExpr, const LocalMap &callLocals) { return resolveMethodCallDefinition(callExpr, callLocals); },
        [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
        [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return getReturnInfo(definitionPath, returnInfo); },
        [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
          return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
        },
        function.instructions,
        error);
    if (directCallResult == ir_lowerer::DirectCallStatementEmitResult::Error) {
      return false;
    }
    if (directCallResult == ir_lowerer::DirectCallStatementEmitResult::Emitted) {
      return true;
    }
    const auto assignOrExprResult = ir_lowerer::emitAssignOrExprStatementWithPop(
        stmt,
        localsIn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        function.instructions);
    if (assignOrExprResult == ir_lowerer::AssignOrExprStatementEmitResult::Error) {
      return false;
    }
    return true;
  };

  std::optional<OnErrorHandler> entryOnError;
  auto entryOnErrorIt = onErrorByDef.find(entryDef->fullPath);
  if (entryOnErrorIt != onErrorByDef.end()) {
    entryOnError = entryOnErrorIt->second;
  }
  std::optional<ResultReturnInfo> entryResult;
  if (entryHasResultInfo) {
    entryResult = entryResultInfo;
  }
  const auto entryExecutionResult = ir_lowerer::emitEntryCallableExecutionWithCleanup(
      *entryDef,
      returnsVoid,
      sawReturn,
      currentOnError,
      entryOnError,
      currentReturnResult,
      entryResult,
      [&](const Expr &stmt) { return emitStatement(stmt, locals); },
      [&]() { pushFileScope(); },
      [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
      [&]() { popFileScope(); },
      function.instructions,
      error);
  if (entryExecutionResult == ir_lowerer::EntryCallableExecutionResult::Error) {
    return false;
  }

  const auto functionTableResult = ir_lowerer::finalizeEntryFunctionTableAndLowerCallables(
      program,
      *entryDef,
      function,
      loweredCallTargets,
      [&](const Definition &def) { return isStructDefinition(def); },
      [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return getReturnInfo(definitionPath, returnInfo); },
      defaultEffects,
      entryDefaultEffects,
      [&](const Expr &expr) { return isTailCallCandidate(expr); },
      [&]() {
        onErrorTempCounter = 0;
        sawReturn = false;
        activeInlineContext = nullptr;
        inlineStack.clear();
        fileScopeStack.clear();
        currentOnError.reset();
        currentReturnResult.reset();
      },
      [&](const Definition &def, int32_t &definitionNextLocal, LocalMap &definitionLocals, Expr &callExpr, std::string &callError) {
        return ir_lowerer::buildCallableDefinitionCallContext(
            def,
            definitionNextLocal,
            definitionLocals,
            callExpr,
            [&](const Expr &param, const LocalMap &localsForKindInference, LocalInfo &info, std::string &inferError) {
              return ir_lowerer::inferCallParameterLocalInfo(param,
                                                             localsForKindInference,
                                                             isBindingMutable,
                                                             hasExplicitBindingTypeTransform,
                                                             bindingKind,
                                                             bindingValueKind,
                                                             inferExprKind,
                                                             isFileErrorBinding,
                                                             setReferenceArrayInfo,
                                                             applyStructArrayInfo,
                                                             applyStructValueInfo,
                                                             isStringBinding,
                                                             info,
                                                             inferError);
            },
            callError);
      },
      [&](const Expr &callExpr, const Definition &def, const LocalMap &definitionLocals, bool expectValue) {
        return emitInlineDefinitionCall(callExpr, def, definitionLocals, expectValue);
      },
      nextLocal,
      out.functions,
      out.entryIndex,
      error);
  if (functionTableResult == ir_lowerer::FunctionTableFinalizationResult::Error) {
    return false;
  }

  out.stringTable = std::move(stringTable);
  return true;
}

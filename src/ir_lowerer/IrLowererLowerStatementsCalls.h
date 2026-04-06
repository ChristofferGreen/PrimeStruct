    return ir_lowerer::runLowerStatementsCallsStep(
        {
            .inferExprKind =
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
            .emitExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            .allocTempLocal = [&]() { return allocTempLocal(); },
            .resolveExprPath = [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
            .findDefinitionByPath = [&](const std::string &path) -> const Definition * {
              auto it = defMap.find(path);
              return it == defMap.end() ? nullptr : it->second;
            },
            .isArrayCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isArrayCountCall(callExpr, callLocals);
            },
            .isStringCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isStringCountCall(callExpr, callLocals);
            },
            .isVectorCapacityCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isVectorCapacityCall(callExpr, callLocals);
            },
            .resolveMethodCallDefinition = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return resolveMethodCallDefinition(callExpr, callLocals);
            },
            .resolveDefinitionCall = [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
            .getReturnInfo =
                [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                  return getReturnInfo(definitionPath, returnInfo);
                },
            .emitInlineDefinitionCall =
                [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
                  return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
                },
            .instructions = &function.instructions,
        },
        stmt,
        localsIn,
        error);
  };

  auto emitEntryStatement = [&](const Expr &stmt) -> bool {
    return ir_lowerer::runLowerStatementsEntryStatementStep(
        {
            .function = &function,
            .emitStatement = [&](const Expr &entryStmt) { return emitStatement(entryStmt, locals); },
            .appendInstructionSourceRange =
                [&](const std::string &functionName, const Expr &entryStmt, size_t beginIndex, size_t endIndex) {
                  appendInstructionSourceRange(functionName, entryStmt, beginIndex, endIndex);
                },
        },
        stmt,
        error);
  };
  if (!ir_lowerer::runLowerStatementsEntryExecutionStep(
          {
              .entryDef = entryDef,
              .returnsVoid = returnsVoid,
              .sawReturn = &sawReturn,
              .onErrorByDef = &onErrorByDef,
              .currentOnError = &currentOnError,
              .currentReturnResult = &currentReturnResult,
              .entryHasResultInfo = entryHasResultInfo,
              .entryResultInfo = &entryResultInfo,
              .emitEntryStatement = emitEntryStatement,
              .pushFileScope = [&]() { pushFileScope(); },
              .emitCurrentFileScopeCleanup = [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
              .popFileScope = [&]() { popFileScope(); },
              .instructions = &function.instructions,
          },
          error)) {
    return false;
  }

  if (!ir_lowerer::runLowerStatementsFunctionTableStep(
          {
              .program = &program,
              .entryDef = entryDef,
              .semanticProgram = semanticProgram,
              .function = &function,
              .loweredCallTargets = &loweredCallTargets,
              .isStructDefinition = [&](const Definition &def) { return isStructDefinition(def); },
              .getReturnInfo =
                  [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                    return getReturnInfo(definitionPath, returnInfo);
                  },
              .defaultEffects = &defaultEffects,
              .entryDefaultEffects = &entryDefaultEffects,
              .isTailCallCandidate = [&](const Expr &expr) { return isTailCallCandidate(expr); },
              .resetDefinitionLoweringState = [&]() {
                onErrorTempCounter = 0;
                sawReturn = false;
                activeInlineContext = nullptr;
                inlineStack.clear();
                fileScopeStack.clear();
                currentOnError.reset();
                currentReturnResult.reset();
              },
              .buildDefinitionCallContext =
                  [&](const Definition &def,
                      int32_t &definitionNextLocal,
                      LocalMap &definitionLocals,
                      Expr &callExpr,
                      std::string &callError) {
                    return ir_lowerer::buildCallableDefinitionCallContext(
                        def,
                        structNames,
                        definitionNextLocal,
                        definitionLocals,
                        callExpr,
                        [&](const std::string &structPath, ir_lowerer::StructSlotLayoutInfo &layoutOut) {
                          return resolveStructSlotLayout(structPath, layoutOut);
                        },
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
                                                                         inferError,
                                                                         [&](const Expr &callExpr,
                                                                             const LocalMap &callLocals) {
                                                                           return resolveMethodCallDefinition(
                                                                               callExpr, callLocals);
                                                                         },
                                                                         [&](const Expr &callExpr) {
                                                                           return resolveDefinitionCall(callExpr);
                                                                         },
                                                                         [&](const std::string &definitionPath,
                                                                             ReturnInfo &returnInfo) {
                                                                           return getReturnInfo(definitionPath, returnInfo);
                                                                         },
                                                                         &callResolutionAdapters.semanticProductTargets);
                        },
                        callError);
                  },
              .emitInlineDefinitionCall =
                  [&](const Expr &callExpr, const Definition &def, const LocalMap &definitionLocals, bool expectValue) {
                    return emitInlineDefinitionCall(callExpr, def, definitionLocals, expectValue);
                  },
              .nextLocal = &nextLocal,
              .outFunctions = &out.functions,
              .entryIndex = &out.entryIndex,
          },
          error)) {
    return false;
  }

  if (!ir_lowerer::runLowerStatementsSourceMapStep(
          {
              .defMap = &defMap,
              .instructionSourceRangesByFunction = &instructionSourceRangesByFunction,
              .stringTable = &stringTable,
              .outModule = &out,
          },
          error)) {
    return false;
  }

  error.clear();
  loweringSucceeded = true;
  return true;
}

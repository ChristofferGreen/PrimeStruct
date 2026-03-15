  auto emitInlineDefinitionCall = [&](const Expr &callExpr,
                                      const Definition &callee,
                                      const LocalMap &callerLocals,
                                      bool requireValue) -> bool {
    ir_lowerer::InlineDefinitionCallContextSetup callSetup;
    if (!ir_lowerer::prepareInlineDefinitionCallContext(
            callee,
            requireValue,
            [&](const std::string &path, ReturnInfo &infoOut) { return getReturnInfo(path, infoOut); },
            [&](const Definition &candidate) { return isStructDefinition(candidate); },
            inlineStack,
            loweredCallTargets,
            onErrorByDef,
            callSetup,
            error)) {
      return false;
    }
    const ReturnInfo &returnInfo = callSetup.returnInfo;
    const bool structDef = callSetup.structDefinition;
    OnErrorScope onErrorScope(currentOnError, callSetup.scopedOnError);
    ResultReturnScope resultScope(currentReturnResult, callSetup.scopedResult);
    pushFileScope();
    std::vector<Expr> callParams;
    std::vector<const Expr *> orderedArgs;
    std::vector<const Expr *> packedArgs;
    size_t packedParamIndex = 0;
    if (!ir_lowerer::buildInlineCallOrderedArguments(
            callExpr,
            callee,
            structNames,
            callerLocals,
            callParams,
            orderedArgs,
            packedArgs,
            packedParamIndex,
            error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    if (structDef) {
      if (!ir_lowerer::emitInlineStructDefinitionArguments(
              callee.fullPath,
              orderedArgs,
              callerLocals,
              requireValue,
              nextLocal,
              [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                return resolveStructSlotLayout(structPath, layoutOut);
              },
              [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                return inferExprKind(candidateExpr, candidateLocals);
              },
              [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                return inferStructExprPath(candidateExpr, candidateLocals);
              },
              [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                return emitExpr(candidateExpr, candidateLocals);
              },
              [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
                return emitStructCopySlots(destBaseLocal, srcPtrLocal, slotCount);
              },
              [&]() { return allocTempLocal(); },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              error)) {
        inlineStack.erase(callee.fullPath);
        return false;
      }
      inlineStack.erase(callee.fullPath);
      return true;
    }

    LocalMap calleeLocals;
    if (!ir_lowerer::emitInlineDefinitionCallParameters(
            callParams,
            orderedArgs,
            packedArgs,
            packedParamIndex,
            callerLocals,
            nextLocal,
            calleeLocals,
            [&](const Expr &param, LocalInfo &infoOut, std::string &errorOut) {
              return ir_lowerer::inferCallParameterLocalInfo(param,
                                                             callerLocals,
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
                                                             infoOut,
                                                             errorOut);
            },
            [&](const Expr &param) { return isStringBinding(param); },
            [&](const Expr &argExpr,
                const LocalMap &locals,
                LocalInfo::StringSource &sourceOut,
                int32_t &indexOut,
                bool &argvCheckedOut) {
              return emitStringValueForCall(argExpr, locals, sourceOut, indexOut, argvCheckedOut);
            },
            [&](const Expr &argExpr, const LocalMap &locals) { return inferStructExprPath(argExpr, locals); },
            [&](const Expr &argExpr, const LocalMap &locals) { return inferExprKind(argExpr, locals); },
            [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(structPath, layoutOut);
            },
            [&](const Expr &argExpr, const LocalMap &locals) { return emitExpr(argExpr, locals); },
            [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
              return emitStructCopySlots(destBaseLocal, srcPtrLocal, slotCount);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](int32_t localIndex) { fileScopeStack.back().push_back(localIndex); },
            error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    if (!ir_lowerer::runLowerInlineCallGpuLocalsStep(
            {
                .callerLocals = &callerLocals,
                .calleeLocals = &calleeLocals,
            },
            error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    InlineContext context;
    context.defPath = callee.fullPath;
    ir_lowerer::LowerInlineCallContextSetupStepOutput contextSetup;
    if (!ir_lowerer::runLowerInlineCallContextSetupStep(
            {
                .function = &function,
                .returnInfo = &returnInfo,
                .allocTempLocal = [&]() { return allocTempLocal(); },
            },
            contextSetup,
            error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }
    context.returnsVoid = contextSetup.returnsVoid;
    context.returnsArray = contextSetup.returnsArray;
    context.returnKind = contextSetup.returnKind;
    context.returnLocal = contextSetup.returnLocal;

    InlineContext *prevContext = activeInlineContext;
    if (!ir_lowerer::runLowerInlineCallActiveContextStep(
            {
                .callee = &callee,
                .structDefinition = structDef,
                .activateInlineContext = [&]() { activeInlineContext = &context; },
                .restoreInlineContext = [&]() { activeInlineContext = prevContext; },
                .emitInlineStatement = [&](const Expr &stmt) {
                  return ir_lowerer::runLowerInlineCallStatementStep(
                      {
                          .function = &function,
                          .emitStatement = [&](const Expr &inlineStmt) { return emitStatement(inlineStmt, calleeLocals); },
                          .appendInstructionSourceRange = [&](const std::string &functionName,
                                                              const Expr &inlineStmt,
                                                              size_t beginIndex,
                                                              size_t endIndex) {
                            appendInstructionSourceRange(functionName, inlineStmt, beginIndex, endIndex);
                          },
                      },
                      stmt,
                      error);
                },
                .runInlineCleanup = [&]() {
                  return ir_lowerer::runLowerInlineCallCleanupStep(
                      {
                          .function = &function,
                          .returnJumps = &context.returnJumps,
                          .emitCurrentFileScopeCleanup = [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
                          .popFileScope = [&]() { popFileScope(); },
                      },
                      error);
                },
            },
            error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    if (!ir_lowerer::runLowerInlineCallReturnValueStep(
            {
                .function = &function,
                .returnsVoid = context.returnsVoid,
                .returnLocal = context.returnLocal,
                .structDefinition = structDef,
                .requireValue = requireValue,
            },
            error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    inlineStack.erase(callee.fullPath);
    return true;
  };

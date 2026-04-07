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
              callParams,
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
              [&](const Expr &fieldParam,
                  const LocalMap &fieldLocals,
                  LocalInfo &infoOut,
                  std::string &errorOut) {
                return ir_lowerer::inferCallParameterLocalInfo(fieldParam,
                                                               fieldLocals,
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
                                                               errorOut,
                                                               [&](const Expr &callExpr,
                                                                   const LocalMap &callLocals) {
                                                                 return resolveMethodCallDefinition(callExpr, callLocals);
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
            callee.fullPath,
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
                                                             errorOut,
                                                             [&](const Expr &callExpr,
                                                                 const LocalMap &callLocals) {
                                                               return resolveMethodCallDefinition(callExpr, callLocals);
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
            [&](const Expr &argExpr) { return resolveDefinitionCall(argExpr); },
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

    if (callee.fullPath == "/std/collections/map/insert_builtin_pending" ||
        callee.fullPath.rfind("/std/collections/map/insert_builtin_pending__", 0) == 0) {
      auto valuesIt = calleeLocals.find("values");
      auto keyIt = calleeLocals.find("key");
      auto valueIt = calleeLocals.find("value");
      if (valuesIt == calleeLocals.end() || keyIt == calleeLocals.end() || valueIt == calleeLocals.end()) {
        error = "builtin canonical map insert pending lowering requires values/key/value locals";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      if (valuesIt->second.mapKeyKind == LocalInfo::ValueKind::Unknown) {
        error = "builtin canonical map insert pending lowering requires typed map bindings";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      int32_t valuesLocal = -1;
      int32_t valuesWrapperLocal = -1;
      int32_t ptrLocal = valuesIt->second.index;
      if (!orderedArgs.empty() && orderedArgs.front() != nullptr) {
        const Expr *originalValuesArg = orderedArgs.front();
        if (originalValuesArg->kind == Expr::Kind::Call &&
            isSimpleCallName(*originalValuesArg, "dereference") &&
            originalValuesArg->args.size() == 1 &&
            originalValuesArg->args.front().kind == Expr::Kind::Name) {
          originalValuesArg = &originalValuesArg->args.front();
        }
        if (originalValuesArg->kind == Expr::Kind::Name) {
          auto callerValuesIt = callerLocals.find(originalValuesArg->name);
          if (callerValuesIt != callerLocals.end()) {
            if (callerValuesIt->second.kind == LocalInfo::Kind::Map) {
              valuesLocal = callerValuesIt->second.index;
            } else if ((callerValuesIt->second.kind == LocalInfo::Kind::Reference &&
                        callerValuesIt->second.referenceToMap) ||
                       (callerValuesIt->second.kind == LocalInfo::Kind::Pointer &&
                        callerValuesIt->second.pointerToMap)) {
              valuesWrapperLocal = callerValuesIt->second.index;
            }
          }
        }
      }
      if (valuesIt->second.kind == LocalInfo::Kind::Reference ||
          valuesIt->second.kind == LocalInfo::Kind::Pointer) {
        valuesWrapperLocal = valuesIt->second.index;
        ptrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesIt->second.index)});
        function.instructions.push_back({IrOpcode::LoadIndirect, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
      }
      if (!ir_lowerer::emitBuiltinCanonicalMapInsertOverwriteOrPending(
              valuesLocal,
              valuesWrapperLocal,
              ptrLocal,
              keyIt->second.index,
              valueIt->second.index,
              valuesIt->second.mapKeyKind,
              [&]() { return allocTempLocal(); },
              [&]() { emitBuiltinCanonicalMapInsertPending(); },
              [&]() { return function.instructions.size(); },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              [&](size_t indexToPatch, uint64_t target) { function.instructions[indexToPatch].imm = target; })) {
        error = "failed to lower builtin canonical map insert pending helper";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();
      inlineStack.erase(callee.fullPath);
      return true;
    }

    if (callee.fullPath == "/std/collections/experimental_soa_storage/soaArbitraryWidthPending" ||
        callee.fullPath.rfind("/std/collections/experimental_soa_storage/soaArbitraryWidthPending__", 0) == 0) {
      emitSoaArbitraryWidthPending();
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();
      inlineStack.erase(callee.fullPath);
      return true;
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
                .definitionReturnsVoid = context.returnsVoid,
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

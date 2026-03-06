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
    if (!ir_lowerer::buildInlineCallOrderedArguments(
            callExpr, callee, structNames, callParams, orderedArgs, error)) {
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

    auto inheritGpuLocal = [&](const char *name) {
      auto it = callerLocals.find(name);
      if (it != callerLocals.end()) {
        calleeLocals.emplace(name, it->second);
      }
    };
    inheritGpuLocal(kGpuGlobalIdXName);
    inheritGpuLocal(kGpuGlobalIdYName);
    inheritGpuLocal(kGpuGlobalIdZName);

    InlineContext context;
    context.defPath = callee.fullPath;
    context.returnsVoid = returnInfo.returnsVoid;
    context.returnsArray = returnInfo.returnsArray;
    context.returnKind = returnInfo.kind;
    if (!context.returnsVoid) {
      context.returnLocal = allocTempLocal();
      IrOpcode zeroOp = IrOpcode::PushI32;
      if (context.returnsArray || context.returnKind == LocalInfo::ValueKind::Int64 ||
          context.returnKind == LocalInfo::ValueKind::UInt64 || context.returnKind == LocalInfo::ValueKind::String) {
        zeroOp = IrOpcode::PushI64;
      } else if (context.returnKind == LocalInfo::ValueKind::Float64) {
        zeroOp = IrOpcode::PushF64;
      } else if (context.returnKind == LocalInfo::ValueKind::Float32) {
        zeroOp = IrOpcode::PushF32;
      }
      function.instructions.push_back({zeroOp, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
    }

    InlineContext *prevContext = activeInlineContext;
    activeInlineContext = &context;
    if (!structDef) {
      for (const auto &stmt : callee.statements) {
        if (!ir_lowerer::runLowerInlineCallStatementStep(
                {
                    .function = &function,
                    .emitStatement = [&](const Expr &inlineStmt) { return emitStatement(inlineStmt, calleeLocals); },
                    .appendInstructionSourceRange =
                        [&](const std::string &functionName, const Expr &inlineStmt, size_t beginIndex, size_t endIndex) {
                          appendInstructionSourceRange(functionName, inlineStmt, beginIndex, endIndex);
                        },
                },
                stmt,
                error)) {
          activeInlineContext = prevContext;
          inlineStack.erase(callee.fullPath);
          return false;
        }
      }
    }
    size_t cleanupIndex = function.instructions.size();
    emitFileScopeCleanup(fileScopeStack.back());
    popFileScope();
    for (size_t jumpIndex : context.returnJumps) {
      function.instructions[jumpIndex].imm = static_cast<int32_t>(cleanupIndex);
    }
    activeInlineContext = prevContext;

    if (!context.returnsVoid) {
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(context.returnLocal)});
    }
    if (structDef && requireValue && context.returnsVoid) {
      // VM/native treat struct constructor calls as void; synthesize a value for expression contexts.
      function.instructions.push_back({IrOpcode::PushI32, 0});
    }

    inlineStack.erase(callee.fullPath);
    return true;
  };

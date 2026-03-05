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
    if (stmt.kind == Expr::Kind::Call) {
      if (stmt.isMethodCall && !isArrayCountCall(stmt, localsIn) && !isStringCountCall(stmt, localsIn) &&
          !isVectorCapacityCall(stmt, localsIn)) {
        const Definition *callee = resolveMethodCallDefinition(stmt, localsIn);
        if (!callee) {
          return false;
        }
        if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
          error = "native backend does not support block arguments on calls";
          return false;
        }
        return emitInlineDefinitionCall(stmt, *callee, localsIn, false);
      }
      if (const Definition *callee = resolveDefinitionCall(stmt)) {
        if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
          error = "native backend does not support block arguments on calls";
          return false;
        }
        ReturnInfo info;
        if (!getReturnInfo(callee->fullPath, info)) {
          return false;
        }
        if (!emitInlineDefinitionCall(stmt, *callee, localsIn, false)) {
          return false;
        }
        if (!info.returnsVoid) {
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
        return true;
      }
    }
    if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
      if (!emitExpr(stmt, localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::Pop, 0});
      return true;
    }
    if (!emitExpr(stmt, localsIn)) {
      return false;
    }
    function.instructions.push_back({IrOpcode::Pop, 0});
    return true;
  };

  std::optional<OnErrorHandler> entryOnError;
  auto entryOnErrorIt = onErrorByDef.find(entryDef->fullPath);
  if (entryOnErrorIt != onErrorByDef.end()) {
    entryOnError = entryOnErrorIt->second;
  }
  OnErrorScope entryOnErrorScope(currentOnError, entryOnError);
  std::optional<ResultReturnInfo> entryResult;
  if (entryHasResultInfo) {
    entryResult = entryResultInfo;
  }
  ResultReturnScope entryResultScope(currentReturnResult, entryResult);
  pushFileScope();
  for (const auto &stmt : entryDef->statements) {
    if (!emitStatement(stmt, locals)) {
      return false;
    }
  }
  emitFileScopeCleanup(fileScopeStack.back());
  popFileScope();

  if (!sawReturn) {
    if (returnsVoid) {
      function.instructions.push_back({IrOpcode::ReturnVoid, 0});
    } else {
      error = "native backend requires an explicit return statement";
      return false;
    }
  }

  auto appendReturnForDefinition = [&](const std::string &defPath, const ReturnInfo &returnInfo) -> bool {
    return ir_lowerer::emitReturnForDefinition(function.instructions, defPath, returnInfo, error);
  };
  out.functions.push_back(std::move(function));
  out.entryIndex = 0;

  for (const auto &def : program.definitions) {
    if (def.fullPath == entryDef->fullPath || isStructDefinition(def) ||
        loweredCallTargets.find(def.fullPath) == loweredCallTargets.end()) {
      continue;
    }
    ReturnInfo returnInfo;
    if (!getReturnInfo(def.fullPath, returnInfo)) {
      return false;
    }
    function = IrFunction{};
    function.name = def.fullPath;
    if (!resolveEffectMask(
            def.transforms, false, defaultEffects, entryDefaultEffects, function.metadata.effectMask, error)) {
      return false;
    }
    if (!resolveCapabilityMask(def.transforms,
                               resolveActiveEffects(def.transforms, false, defaultEffects, entryDefaultEffects),
                               def.fullPath,
                               function.metadata.capabilityMask,
                               error)) {
      return false;
    }
    function.metadata.schedulingScope = IrSchedulingScope::Default;
    function.metadata.instrumentationFlags = 0;
    if (ir_lowerer::hasTailExecutionCandidate(def.statements, returnInfo.returnsVoid, isTailCallCandidate)) {
      function.metadata.instrumentationFlags |= InstrumentationTailExecution;
    }

    nextLocal = 0;
    onErrorTempCounter = 0;
    sawReturn = false;
    activeInlineContext = nullptr;
    inlineStack.clear();
    fileScopeStack.clear();
    currentOnError.reset();
    currentReturnResult.reset();

    LocalMap definitionLocals;
    Expr callExpr;
    callExpr.kind = Expr::Kind::Call;
    callExpr.name = def.fullPath;
    callExpr.namespacePrefix = def.namespacePrefix;
    bool isComputeDefinition = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "compute") {
        isComputeDefinition = true;
        break;
      }
    }
    if (isComputeDefinition) {
      auto addGpuLocal = [&](const char *name) {
        LocalInfo gpuInfo;
        gpuInfo.index = nextLocal++;
        gpuInfo.kind = LocalInfo::Kind::Value;
        gpuInfo.valueKind = LocalInfo::ValueKind::Int32;
        definitionLocals.emplace(name, gpuInfo);
      };
      addGpuLocal(kGpuGlobalIdXName);
      addGpuLocal(kGpuGlobalIdYName);
      addGpuLocal(kGpuGlobalIdZName);
    }
    for (const auto &param : def.parameters) {
      LocalInfo info;
      info.index = nextLocal++;
      if (!ir_lowerer::inferCallParameterLocalInfo(param,
                                                   definitionLocals,
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
                                                   error)) {
        return false;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown && info.structTypeName.empty()) {
        error = "native backend requires typed parameters on " + def.fullPath;
        return false;
      }
      definitionLocals.emplace(param.name, info);

      Expr argExpr;
      argExpr.kind = Expr::Kind::Name;
      argExpr.name = param.name;
      callExpr.args.push_back(std::move(argExpr));
      callExpr.argNames.push_back(std::nullopt);
    }

    if (!emitInlineDefinitionCall(callExpr, def, definitionLocals, !returnInfo.returnsVoid)) {
      return false;
    }
    if (!appendReturnForDefinition(def.fullPath, returnInfo)) {
      return false;
    }
    out.functions.push_back(std::move(function));
  }

  out.stringTable = std::move(stringTable);
  return true;
}

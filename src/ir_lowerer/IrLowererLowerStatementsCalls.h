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
    if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "dispatch")) {
      if (stmt.args.size() < 4) {
        error = "dispatch requires kernel and three dimension arguments";
        return false;
      }
      if (stmt.args.front().kind != Expr::Kind::Name) {
        error = "dispatch requires kernel name as first argument";
        return false;
      }
      const Expr &kernelExpr = stmt.args.front();
      const std::string kernelPath = resolveExprPath(kernelExpr);
      auto defIt = defMap.find(kernelPath);
      if (defIt == defMap.end() || !defIt->second) {
        error = "dispatch requires known kernel: " + kernelPath;
        return false;
      }
      bool isCompute = false;
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name == "compute") {
          isCompute = true;
          break;
        }
      }
      if (!isCompute) {
        error = "dispatch requires compute definition: " + kernelPath;
        return false;
      }
      for (size_t i = 1; i <= 3; ++i) {
        LocalInfo::ValueKind dimKind = inferExprKind(stmt.args[i], localsIn);
        if (dimKind != LocalInfo::ValueKind::Int32) {
          error = "dispatch requires i32 dimensions";
          return false;
        }
      }
      const auto &kernelParams = defIt->second->parameters;
      if (stmt.args.size() != kernelParams.size() + 4) {
        error = "dispatch argument count mismatch for " + kernelPath;
        return false;
      }
      const int32_t gxLocal = allocTempLocal();
      if (!emitExpr(stmt.args[1], localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gxLocal)});
      const int32_t gyLocal = allocTempLocal();
      if (!emitExpr(stmt.args[2], localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gyLocal)});
      const int32_t gzLocal = allocTempLocal();
      if (!emitExpr(stmt.args[3], localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gzLocal)});

      const int32_t xLocal = allocTempLocal();
      const int32_t yLocal = allocTempLocal();
      const int32_t zLocal = allocTempLocal();
      const int32_t gidXLocal = allocTempLocal();
      const int32_t gidYLocal = allocTempLocal();
      const int32_t gidZLocal = allocTempLocal();

      LocalMap dispatchLocals = localsIn;
      LocalInfo gidInfo;
      gidInfo.index = gidXLocal;
      gidInfo.kind = LocalInfo::Kind::Value;
      gidInfo.valueKind = LocalInfo::ValueKind::Int32;
      dispatchLocals.emplace(kGpuGlobalIdXName, gidInfo);
      gidInfo.index = gidYLocal;
      dispatchLocals.emplace(kGpuGlobalIdYName, gidInfo);
      gidInfo.index = gidZLocal;
      dispatchLocals.emplace(kGpuGlobalIdZName, gidInfo);

      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
      const size_t zCheck = function.instructions.size();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gzLocal)});
      function.instructions.push_back({IrOpcode::CmpLtI32, 0});
      const size_t zJumpEnd = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
      const size_t yCheck = function.instructions.size();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gyLocal)});
      function.instructions.push_back({IrOpcode::CmpLtI32, 0});
      const size_t yJumpEnd = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
      const size_t xCheck = function.instructions.size();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gxLocal)});
      function.instructions.push_back({IrOpcode::CmpLtI32, 0});
      const size_t xJumpEnd = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidXLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidYLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidZLocal)});

      Expr kernelCall;
      kernelCall.kind = Expr::Kind::Call;
      kernelCall.name = kernelPath;
      kernelCall.namespacePrefix = kernelExpr.namespacePrefix;
      for (size_t i = 4; i < stmt.args.size(); ++i) {
        kernelCall.args.push_back(stmt.args[i]);
        kernelCall.argNames.push_back(std::nullopt);
      }
      if (!emitInlineDefinitionCall(kernelCall, *defIt->second, dispatchLocals, false)) {
        return false;
      }

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
      function.instructions.push_back({IrOpcode::PushI32, 1});
      function.instructions.push_back({IrOpcode::AddI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(xCheck)});
      const size_t xEnd = function.instructions.size();
      function.instructions[xJumpEnd].imm = static_cast<int32_t>(xEnd);

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
      function.instructions.push_back({IrOpcode::PushI32, 1});
      function.instructions.push_back({IrOpcode::AddI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(yCheck)});
      const size_t yEnd = function.instructions.size();
      function.instructions[yJumpEnd].imm = static_cast<int32_t>(yEnd);

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
      function.instructions.push_back({IrOpcode::PushI32, 1});
      function.instructions.push_back({IrOpcode::AddI32, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(zCheck)});
      const size_t zEnd = function.instructions.size();
      function.instructions[zJumpEnd].imm = static_cast<int32_t>(zEnd);
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

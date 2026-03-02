    if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "buffer_store")) {
      if (stmt.args.size() != 3) {
        error = "buffer_store requires buffer, index, and value";
        return false;
      }
      LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
      if (stmt.args[0].kind == Expr::Kind::Name) {
        auto it = localsIn.find(stmt.args[0].name);
        if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
          elemKind = it->second.valueKind;
        }
      } else if (stmt.args[0].kind == Expr::Kind::Call) {
        if (isSimpleCallName(stmt.args[0], "buffer") && stmt.args[0].templateArgs.size() == 1) {
          elemKind = valueKindFromTypeName(stmt.args[0].templateArgs.front());
        }
      }
      if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
        error = "buffer_store requires numeric/bool buffer";
        return false;
      }
      LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
      if (!isSupportedIndexKind(indexKind)) {
        error = "buffer_store requires integer index";
        return false;
      }
      const int32_t ptrLocal = allocTempLocal();
      if (!emitExpr(stmt.args[0], localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
      const int32_t indexLocal = allocTempLocal();
      if (!emitExpr(stmt.args[1], localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
      const int32_t valueLocal = allocTempLocal();
      if (!emitExpr(stmt.args[2], localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      function.instructions.push_back({pushOneForIndex(indexKind), 1});
      function.instructions.push_back({addForIndex(indexKind), 0});
      function.instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
      function.instructions.push_back({mulForIndex(indexKind), 0});
      function.instructions.push_back({IrOpcode::AddI64, 0});
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      function.instructions.push_back({IrOpcode::StoreIndirect, 0});
      function.instructions.push_back({IrOpcode::Pop, 0});
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
    if (returnInfo.returnsVoid) {
      function.instructions.push_back({IrOpcode::ReturnVoid, 0});
      return true;
    }
    if (returnInfo.returnsArray || returnInfo.kind == LocalInfo::ValueKind::Int64 ||
        returnInfo.kind == LocalInfo::ValueKind::UInt64 || returnInfo.kind == LocalInfo::ValueKind::String) {
      function.instructions.push_back({IrOpcode::ReturnI64, 0});
      return true;
    }
    if (returnInfo.kind == LocalInfo::ValueKind::Int32 || returnInfo.kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::ReturnI32, 0});
      return true;
    }
    if (returnInfo.kind == LocalInfo::ValueKind::Float32) {
      function.instructions.push_back({IrOpcode::ReturnF32, 0});
      return true;
    }
    if (returnInfo.kind == LocalInfo::ValueKind::Float64) {
      function.instructions.push_back({IrOpcode::ReturnF64, 0});
      return true;
    }
    error = "native backend does not support return type on " + defPath;
    return false;
  };
  auto hasTailExecutionCandidate = [&](const Definition &def, bool definitionReturnsVoid) {
    if (def.statements.empty()) {
      return false;
    }
    const Expr &lastStmt = def.statements.back();
    if (isReturnCall(lastStmt) && lastStmt.args.size() == 1) {
      return isTailCallCandidate(lastStmt.args.front());
    }
    return definitionReturnsVoid && isTailCallCandidate(lastStmt);
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
    if (!resolveEffectMask(def.transforms, false, function.metadata.effectMask)) {
      return false;
    }
    if (!resolveCapabilityMask(def.transforms, resolveActiveEffects(def.transforms, false),
                               function.metadata.capabilityMask)) {
      return false;
    }
    function.metadata.schedulingScope = IrSchedulingScope::Default;
    function.metadata.instrumentationFlags = 0;
    if (hasTailExecutionCandidate(def, returnInfo.returnsVoid)) {
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
      info.isMutable = isBindingMutable(param);
      info.kind = bindingKind(param);
      if (hasExplicitBindingTypeTransform(param)) {
        info.valueKind = bindingValueKind(param, info.kind);
      } else if (param.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
        info.valueKind = inferExprKind(param.args.front(), definitionLocals);
        if (info.valueKind == LocalInfo::ValueKind::Unknown) {
          info.valueKind = LocalInfo::ValueKind::Int32;
        }
      } else {
        info.valueKind = bindingValueKind(param, info.kind);
      }
      if (info.kind == LocalInfo::Kind::Map) {
        for (const auto &transform : param.transforms) {
          if (transform.name == "map" && transform.templateArgs.size() == 2) {
            info.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
            info.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
            break;
          }
        }
      }
      for (const auto &transform : param.transforms) {
        if (transform.name == "File") {
          info.isFileHandle = true;
          info.valueKind = LocalInfo::ValueKind::Int64;
        } else if (transform.name == "Result") {
          info.isResult = true;
          info.resultHasValue = (transform.templateArgs.size() == 2);
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          if (!transform.templateArgs.empty()) {
            info.resultErrorType = transform.templateArgs.back();
          }
        }
      }
      info.isFileError = isFileErrorBinding(param);
      setReferenceArrayInfo(param, info);
      applyStructArrayInfo(param, info);
      applyStructValueInfo(param, info);
      if (isStringBinding(param)) {
        if (info.kind != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          return false;
        }
        info.valueKind = LocalInfo::ValueKind::String;
        info.stringSource = LocalInfo::StringSource::RuntimeIndex;
        info.stringIndex = -1;
        info.argvChecked = true;
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

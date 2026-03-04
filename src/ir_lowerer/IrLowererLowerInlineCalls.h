  auto emitInlineDefinitionCall = [&](const Expr &callExpr,
                                      const Definition &callee,
                                      const LocalMap &callerLocals,
                                      bool requireValue) -> bool {
    ReturnInfo returnInfo;
    if (!getReturnInfo(callee.fullPath, returnInfo)) {
      return false;
    }
    const bool structDef = isStructDefinition(callee);
    if (returnInfo.returnsVoid && requireValue && !structDef) {
      error = "void call not allowed in expression context: " + callee.fullPath;
      return false;
    }
    if (!inlineStack.insert(callee.fullPath).second) {
      error = "native backend does not support recursive calls: " + callee.fullPath;
      return false;
    }
    loweredCallTargets.insert(callee.fullPath);
    std::optional<OnErrorHandler> scopedOnError;
    auto onErrorIt = onErrorByDef.find(callee.fullPath);
    if (onErrorIt != onErrorByDef.end()) {
      scopedOnError = onErrorIt->second;
    }
    OnErrorScope onErrorScope(currentOnError, scopedOnError);
    std::optional<ResultReturnInfo> scopedResult;
    if (returnInfo.isResult) {
      scopedResult = ResultReturnInfo{true, returnInfo.resultHasValue};
    }
    ResultReturnScope resultScope(currentReturnResult, scopedResult);
    pushFileScope();
    std::vector<Expr> callParams;
    std::vector<const Expr *> orderedArgs;
    if (!ir_lowerer::buildInlineCallOrderedArguments(
            callExpr, callee, structNames, callParams, orderedArgs, error)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    if (structDef) {
      StructSlotLayout layout;
      if (!resolveStructSlotLayout(callee.fullPath, layout)) {
        inlineStack.erase(callee.fullPath);
        return false;
      }
      if (static_cast<int32_t>(orderedArgs.size()) != static_cast<int32_t>(layout.fields.size())) {
        error = "argument count mismatch";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      const int32_t baseLocal = nextLocal;
      if (requireValue) {
        nextLocal += layout.totalSlots;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
      }
      for (size_t i = 0; i < layout.fields.size(); ++i) {
        const StructSlotFieldInfo &field = layout.fields[i];
        const Expr *arg = orderedArgs[i];
        if (!arg) {
          error = "argument count mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        if (field.structPath.empty()) {
          LocalInfo::ValueKind argKind = inferExprKind(*arg, callerLocals);
          if (argKind == LocalInfo::ValueKind::Unknown && field.valueKind != LocalInfo::ValueKind::Unknown) {
            argKind = field.valueKind;
          }
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
            error = "native backend requires struct field values to be numeric/bool on " + callee.fullPath;
            inlineStack.erase(callee.fullPath);
            return false;
          }
          if (argKind != field.valueKind) {
            error = "struct field type mismatch";
            inlineStack.erase(callee.fullPath);
            return false;
          }
          if (!emitExpr(*arg, callerLocals)) {
            inlineStack.erase(callee.fullPath);
            return false;
          }
          if (requireValue) {
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + field.slotOffset)});
          } else {
            function.instructions.push_back({IrOpcode::Pop, 0});
          }
          continue;
        }

        std::string argStruct = inferStructExprPath(*arg, callerLocals);
        if (argStruct.empty() || argStruct != field.structPath) {
          error = "struct field type mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        if (!emitExpr(*arg, callerLocals)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        if (requireValue) {
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (!emitStructCopySlots(baseLocal + field.slotOffset, srcPtrLocal, field.slotCount)) {
            inlineStack.erase(callee.fullPath);
            return false;
          }
        } else {
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
      }
      if (requireValue) {
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
      }
      inlineStack.erase(callee.fullPath);
      return true;
    }

    LocalMap calleeLocals;
    for (size_t i = 0; i < callParams.size(); ++i) {
      const Expr &param = callParams[i];
      LocalInfo paramInfo;
      paramInfo.index = nextLocal++;
      paramInfo.isMutable = isBindingMutable(param);
      paramInfo.kind = bindingKind(param);
      if (hasExplicitBindingTypeTransform(param)) {
        paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
      } else if (param.args.size() == 1 && paramInfo.kind == LocalInfo::Kind::Value) {
        paramInfo.valueKind = inferExprKind(param.args.front(), callerLocals);
        if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
          paramInfo.valueKind = LocalInfo::ValueKind::Int32;
        }
      } else {
        paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
      }
      if (paramInfo.kind == LocalInfo::Kind::Map) {
        for (const auto &transform : param.transforms) {
          if (transform.name == "map" && transform.templateArgs.size() == 2) {
            paramInfo.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
            paramInfo.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
            break;
          }
        }
      }
      for (const auto &transform : param.transforms) {
        if (transform.name == "File") {
          paramInfo.isFileHandle = true;
          paramInfo.valueKind = LocalInfo::ValueKind::Int64;
        } else if (transform.name == "Result") {
          paramInfo.isResult = true;
          paramInfo.resultHasValue = (transform.templateArgs.size() == 2);
          paramInfo.valueKind = paramInfo.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          if (!transform.templateArgs.empty()) {
            paramInfo.resultErrorType = transform.templateArgs.back();
          }
        }
      }
      paramInfo.isFileError = isFileErrorBinding(param);
      setReferenceArrayInfo(param, paramInfo);
      applyStructArrayInfo(param, paramInfo);
      applyStructValueInfo(param, paramInfo);

      if (isStringBinding(param)) {
        if (paramInfo.kind != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        if (!orderedArgs[i]) {
          error = "argument count mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        LocalInfo::StringSource source = LocalInfo::StringSource::None;
        int32_t index = -1;
        bool argvChecked = true;
        if (!emitStringValueForCall(*orderedArgs[i], callerLocals, source, index, argvChecked)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        paramInfo.valueKind = LocalInfo::ValueKind::String;
        paramInfo.stringSource = source;
        paramInfo.stringIndex = index;
        paramInfo.argvChecked = argvChecked;
        calleeLocals.emplace(param.name, paramInfo);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index)});
        continue;
      }

      if (paramInfo.kind == LocalInfo::Kind::Value && !paramInfo.structTypeName.empty()) {
        if (!orderedArgs[i]) {
          error = "argument count mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        std::string argStruct = inferStructExprPath(*orderedArgs[i], callerLocals);
        if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
          error = "struct parameter type mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        StructSlotLayout layout;
        if (!resolveStructSlotLayout(paramInfo.structTypeName, layout)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        const int32_t baseLocal = nextLocal;
        nextLocal += layout.totalSlots;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index)});
        if (!emitExpr(*orderedArgs[i], callerLocals)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        calleeLocals.emplace(param.name, paramInfo);
        continue;
      }

      if (paramInfo.kind == LocalInfo::Kind::Reference && !paramInfo.structTypeName.empty()) {
        if (!orderedArgs[i]) {
          error = "argument count mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        std::string argStruct = inferStructExprPath(*orderedArgs[i], callerLocals);
        if (argStruct.empty() || argStruct != paramInfo.structTypeName) {
          error = "struct parameter type mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        const Expr &argExpr = *orderedArgs[i];
        auto emitStructReference = [&](const Expr &arg) -> bool {
          if (arg.kind == Expr::Kind::Name) {
            auto it = callerLocals.find(arg.name);
            if (it != callerLocals.end()) {
              if (it->second.kind == LocalInfo::Kind::Reference) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
                return true;
              }
              if (it->second.kind == LocalInfo::Kind::Value && !it->second.structTypeName.empty()) {
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
                return true;
              }
            }
          }
          if (!emitExpr(arg, callerLocals)) {
            return false;
          }
          const int32_t tempLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempLocal)});
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(tempLocal)});
          return true;
        };
        if (!emitStructReference(argExpr)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        calleeLocals.emplace(param.name, paramInfo);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index)});
        continue;
      }

      if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown || paramInfo.valueKind == LocalInfo::ValueKind::String) {
        error = "native backend only supports numeric/bool, string, or struct parameters";
        inlineStack.erase(callee.fullPath);
        return false;
      }

      if (!orderedArgs[i]) {
        error = "argument count mismatch";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      if (!emitExpr(*orderedArgs[i], callerLocals)) {
        inlineStack.erase(callee.fullPath);
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index)});
      if (paramInfo.isFileHandle) {
        fileScopeStack.back().push_back(paramInfo.index);
      }
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
        if (!emitStatement(stmt, calleeLocals)) {
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

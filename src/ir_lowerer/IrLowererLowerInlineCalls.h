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
    std::vector<Expr> structParams;
    auto isStaticField = [](const Expr &stmt) -> bool {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    if (structDef) {
      structParams.reserve(callee.statements.size());
      for (const auto &param : callee.statements) {
        if (!param.isBinding) {
          error = "struct definitions may only contain field bindings: " + callee.fullPath;
          inlineStack.erase(callee.fullPath);
          return false;
        }
        if (isStaticField(param)) {
          continue;
        }
        structParams.push_back(param);
      }
    }
    const std::vector<Expr> &callParams = structDef ? structParams : callee.parameters;

    std::vector<const Expr *> orderedArgs;
    if (!buildOrderedCallArguments(callExpr, callParams, orderedArgs)) {
      inlineStack.erase(callee.fullPath);
      return false;
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

      if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown || paramInfo.valueKind == LocalInfo::ValueKind::String) {
        error = "native backend only supports numeric/bool or string parameters";
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
    }

    InlineContext context;
    context.defPath = callee.fullPath;
    context.returnsVoid = returnInfo.returnsVoid;
    context.returnsArray = returnInfo.returnsArray;
    context.returnKind = returnInfo.kind;
    if (!context.returnsVoid) {
      context.returnLocal = allocTempLocal();
      IrOpcode zeroOp = IrOpcode::PushI32;
      if (context.returnsArray || context.returnKind == LocalInfo::ValueKind::Int64 ||
          context.returnKind == LocalInfo::ValueKind::UInt64) {
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
    size_t endIndex = function.instructions.size();
    for (size_t jumpIndex : context.returnJumps) {
      function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
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

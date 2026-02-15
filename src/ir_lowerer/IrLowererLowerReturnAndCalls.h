    bool hasReturnTransformLocal = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
          transform.name == "gpu_lane" || transform.name == "no_padding" ||
          transform.name == "platform_independent_padding") {
        info.returnsVoid = true;
        hasReturnTransformLocal = true;
        break;
      }
      if (transform.name != "return") {
        continue;
      }
      hasReturnTransformLocal = true;
      if (transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &typeName = transform.templateArgs.front();
      if (typeName == "void") {
        info.returnsVoid = true;
        break;
      }
      info.kind = valueKindFromTypeName(typeName);
      info.returnsVoid = false;
      break;
    }

    if (hasReturnTransformLocal) {
      if (!info.returnsVoid) {
        if (info.kind == LocalInfo::ValueKind::Unknown) {
          error = "native backend does not support return type on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
        if (info.kind == LocalInfo::ValueKind::String) {
          error = "native backend does not support string return types on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
      }
    } else {
      if (!def.hasReturnStatement) {
        info.returnsVoid = true;
      } else {
        LocalMap localsForInference;
        for (const auto &param : def.parameters) {
          LocalInfo paramInfo;
          paramInfo.index = 0;
          paramInfo.isMutable = isBindingMutable(param);
          paramInfo.kind = bindingKind(param);
          if (hasExplicitBindingTypeTransform(param)) {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          } else if (param.args.size() == 1 && paramInfo.kind == LocalInfo::Kind::Value) {
            paramInfo.valueKind = inferExprKind(param.args.front(), localsForInference);
            if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              paramInfo.valueKind = LocalInfo::ValueKind::Int32;
            }
          } else {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          }
          if (isStringBinding(param)) {
            if (paramInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              returnInferenceStack.erase(path);
              return false;
            }
          }
          if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
            error = "native backend requires typed parameters on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          localsForInference.emplace(param.name, paramInfo);
        }

        LocalInfo::ValueKind inferred = LocalInfo::ValueKind::Unknown;
        bool sawReturnLocal = false;
        bool inferredVoid = false;
        std::function<bool(const Expr &, LocalMap &)> inferStatement;
        inferStatement = [&](const Expr &stmt, LocalMap &activeLocals) -> bool {
          if (stmt.isBinding) {
            LocalInfo bindingInfo;
            bindingInfo.index = 0;
            bindingInfo.isMutable = isBindingMutable(stmt);
            bindingInfo.kind = bindingKind(stmt);
            if (hasExplicitBindingTypeTransform(stmt)) {
              bindingInfo.valueKind = bindingValueKind(stmt, bindingInfo.kind);
            } else if (stmt.args.size() == 1 && bindingInfo.kind == LocalInfo::Kind::Value) {
              bindingInfo.valueKind = inferExprKind(stmt.args.front(), activeLocals);
              if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
                bindingInfo.valueKind = LocalInfo::ValueKind::Int32;
              }
            } else {
              bindingInfo.valueKind = LocalInfo::ValueKind::Unknown;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::String && bindingInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              return false;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend requires typed bindings on " + def.fullPath;
              return false;
            }
            activeLocals.emplace(stmt.name, bindingInfo);
            return true;
          }
          if (isReturnCall(stmt)) {
            sawReturnLocal = true;
            if (stmt.args.empty()) {
              inferredVoid = true;
              return true;
            }
            LocalInfo::ValueKind kind = inferExprKind(stmt.args.front(), activeLocals);
            if (kind == LocalInfo::ValueKind::Unknown) {
              error = "unable to infer return type on " + def.fullPath;
              return false;
            }
            if (kind == LocalInfo::ValueKind::String) {
              error = "native backend does not support string return types on " + def.fullPath;
              return false;
            }
            if (inferred == LocalInfo::ValueKind::Unknown) {
              inferred = kind;
              return true;
            }
            if (inferred != kind) {
              error = "conflicting return types on " + def.fullPath;
              return false;
            }
            return true;
          }
          if (isIfCall(stmt) && stmt.args.size() == 3) {
            const Expr &thenBlock = stmt.args[1];
            const Expr &elseBlock = stmt.args[2];
            auto walkBlock = [&](const Expr &block) -> bool {
              LocalMap blockLocals = activeLocals;
              for (const auto &bodyStmt : block.bodyArguments) {
                if (!inferStatement(bodyStmt, blockLocals)) {
                  return false;
                }
              }
              return true;
            };
            if (!walkBlock(thenBlock)) {
              return false;
            }
            if (!walkBlock(elseBlock)) {
              return false;
            }
          }
          if (isRepeatCall(stmt)) {
            LocalMap blockLocals = activeLocals;
            for (const auto &bodyStmt : stmt.bodyArguments) {
              if (!inferStatement(bodyStmt, blockLocals)) {
                return false;
              }
            }
          }
          return true;
        };

        LocalMap locals = localsForInference;
        for (const auto &stmt : def.statements) {
          if (!inferStatement(stmt, locals)) {
            returnInferenceStack.erase(path);
            return false;
          }
        }
        if (!sawReturnLocal || inferredVoid) {
          if (sawReturnLocal && inferred != LocalInfo::ValueKind::Unknown) {
            error = "conflicting return types on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          info.returnsVoid = true;
        } else {
          info.returnsVoid = false;
          info.kind = inferred;
          if (info.kind == LocalInfo::ValueKind::Unknown) {
            error = "unable to infer return type on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
        }
      }
    }

    returnInferenceStack.erase(path);
    returnInfoCache.emplace(path, info);
    outInfo = info;
    return true;
  };

  struct InlineContext {
    std::string defPath;
    bool returnsVoid = false;
    LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
    int32_t returnLocal = -1;
    std::vector<size_t> returnJumps;
  };

  InlineContext *activeInlineContext = nullptr;
  std::unordered_set<std::string> inlineStack;

  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<bool(const Expr &, LocalMap &)> emitStatement;
  auto allocTempLocal = [&]() -> int32_t {
    return nextLocal++;
  };

  auto emitBlock = [&](const Expr &blockExpr, LocalMap &blockLocals) -> bool {
    if (blockExpr.kind != Expr::Kind::Call) {
      error = "native backend expects if branch blocks to be calls";
      return false;
    }
    if (!blockExpr.args.empty()) {
      error = "native backend does not support arguments on if branch blocks";
      return false;
    }
    for (const auto &stmt : blockExpr.bodyArguments) {
      if (!emitStatement(stmt, blockLocals)) {
        return false;
      }
    }
    return true;
  };

  auto emitFloatLiteral = [&](const Expr &expr) -> bool {
    char *end = nullptr;
    const char *text = expr.floatValue.c_str();
    double value = std::strtod(text, &end);
    if (end == text || (end && *end != '\0')) {
      error = "invalid float literal";
      return false;
    }
    if (expr.floatWidth == 64) {
      uint64_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      function.instructions.push_back({IrOpcode::PushF64, bits});
      return true;
    }
    float f32 = static_cast<float>(value);
    uint32_t bits = 0;
    std::memcpy(&bits, &f32, sizeof(bits));
    function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
    return true;
  };

  auto emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PushI64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Float64) {
      function.instructions.push_back({IrOpcode::PushF64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqF64 : IrOpcode::CmpNeF64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Float32) {
      function.instructions.push_back({IrOpcode::PushF32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqF32 : IrOpcode::CmpNeF32, 0});
      return true;
    }
    error = "boolean conversion requires numeric operand";
    return false;
  };

  auto resolveDefinitionCall = [&](const Expr &callExpr) -> const Definition * {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall) {
      return nullptr;
    }
    const std::string resolved = resolveExprPath(callExpr);
    auto it = defMap.find(resolved);
    if (it == defMap.end()) {
      return nullptr;
    }
    return it->second;
  };
  auto buildOrderedCallArguments = [&](const Expr &callExpr,
                                       const std::vector<Expr> &params,
                                       std::vector<const Expr *> &ordered) -> bool {
    ordered.assign(params.size(), nullptr);
    size_t positionalIndex = 0;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
        const std::string &name = *callExpr.argNames[i];
        size_t index = params.size();
        for (size_t p = 0; p < params.size(); ++p) {
          if (params[p].name == name) {
            index = p;
            break;
          }
        }
        if (index >= params.size()) {
          error = "unknown named argument: " + name;
          return false;
        }
        if (ordered[index] != nullptr) {
          error = "named argument duplicates parameter: " + name;
          return false;
        }
        ordered[index] = &callExpr.args[i];
        continue;
      }
      while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
        ++positionalIndex;
      }
      if (positionalIndex >= ordered.size()) {
        error = "argument count mismatch";
        return false;
      }
      ordered[positionalIndex] = &callExpr.args[i];
      ++positionalIndex;
    }
    for (size_t i = 0; i < ordered.size(); ++i) {
      if (ordered[i] != nullptr) {
        continue;
      }
      if (!params[i].args.empty()) {
        ordered[i] = &params[i].args.front();
        continue;
      }
      error = "argument count mismatch";
      return false;
    }
    return true;
  };

  auto emitStringValueForCall = [&](const Expr &arg,
                                    const LocalMap &callerLocals,
                                    LocalInfo::StringSource &sourceOut,
                                    int32_t &stringIndexOut,
                                    bool &argvCheckedOut) -> bool {
    sourceOut = LocalInfo::StringSource::None;
    stringIndexOut = -1;
    argvCheckedOut = true;
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!parseStringLiteral(arg.stringValue, decoded)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
      sourceOut = LocalInfo::StringSource::TableIndex;
      stringIndexOut = index;
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = callerLocals.find(arg.name);
      if (it == callerLocals.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      if (it->second.valueKind != LocalInfo::ValueKind::String || it->second.stringSource == LocalInfo::StringSource::None) {
        error = "native backend requires string arguments to use string literals, bindings, or entry args";
        return false;
      }
      sourceOut = it->second.stringSource;
      stringIndexOut = it->second.stringIndex;
      argvCheckedOut = it->second.argvChecked;
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
      return true;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (!getBuiltinArrayAccessName(arg, accessName)) {
        error = "native backend requires string arguments to use string literals, bindings, or entry args";
        return false;
      }
      if (arg.args.size() != 2) {
        error = accessName + " requires exactly two arguments";
        return false;
      }
      if (!isEntryArgsName(arg.args.front(), callerLocals)) {
        error = "native backend only supports entry argument indexing";
        return false;
      }
      LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(arg.args[1], callerLocals));
      if (!isSupportedIndexKind(indexKind)) {
        error = "native backend requires integer indices for " + accessName;
        return false;
      }

      const int32_t argvIndexLocal = allocTempLocal();
      if (!emitExpr(arg.args[1], callerLocals)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

      if (accessName == "at") {
        if (indexKind != LocalInfo::ValueKind::UInt64) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
          function.instructions.push_back({pushZeroForIndex(indexKind), 0});
          function.instructions.push_back({cmpLtForIndex(indexKind), 0});
          size_t jumpNonNegative = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitArrayIndexOutOfBounds();
          size_t nonNegativeIndex = function.instructions.size();
          function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
        }

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
        function.instructions.push_back({IrOpcode::PushArgc, 0});
        function.instructions.push_back({cmpGeForIndex(indexKind), 0});
        size_t jumpInRange = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        size_t inRangeIndex = function.instructions.size();
        function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
      }

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
      sourceOut = LocalInfo::StringSource::ArgvIndex;
      argvCheckedOut = (accessName == "at");
      return true;
    }
    error = "native backend requires string arguments to use string literals, bindings, or entry args";
    return false;
  };

  auto emitInlineDefinitionCall = [&](const Expr &callExpr,
                                      const Definition &callee,
                                      const LocalMap &callerLocals,
                                      bool requireValue) -> bool {
    ReturnInfo returnInfo;
    if (!getReturnInfo(callee.fullPath, returnInfo)) {
      return false;
    }
    if (returnInfo.returnsVoid && requireValue) {
      error = "void call not allowed in expression context: " + callee.fullPath;
      return false;
    }
    if (!inlineStack.insert(callee.fullPath).second) {
      error = "native backend does not support recursive calls: " + callee.fullPath;
      return false;
    }

    const bool structDef = isStructDefinition(callee);
    const std::vector<Expr> &callParams = structDef ? callee.statements : callee.parameters;
    if (structDef) {
      for (const auto &param : callParams) {
        if (!param.isBinding) {
          error = "struct definitions may only contain field bindings: " + callee.fullPath;
          inlineStack.erase(callee.fullPath);
          return false;
        }
      }
    }

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
    context.returnKind = returnInfo.kind;
    if (!context.returnsVoid) {
      context.returnLocal = allocTempLocal();
      IrOpcode zeroOp = IrOpcode::PushI32;
      if (context.returnKind == LocalInfo::ValueKind::Int64 || context.returnKind == LocalInfo::ValueKind::UInt64) {
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

    inlineStack.erase(callee.fullPath);
    return true;
  };

  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (expr.isLambda) {
      error = "native backend does not support lambdas";
      return false;
    }
    switch (expr.kind) {
      case Expr::Kind::Literal: {
        if (expr.intWidth == 64 || expr.isUnsigned) {
          IrInstruction inst;
          inst.op = IrOpcode::PushI64;
          inst.imm = expr.literalValue;
          function.instructions.push_back(inst);
          return true;
        }
        int64_t value = static_cast<int64_t>(expr.literalValue);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
          error = "i32 literal out of range for native backend";
          return false;
        }
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = static_cast<int32_t>(value);
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::FloatLiteral:
        return emitFloatLiteral(expr);
      case Expr::Kind::StringLiteral:
        error = "native backend does not support string literals";
        return false;
      case Expr::Kind::BoolLiteral: {
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = expr.boolValue ? 1 : 0;
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it != localsIn.end()) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          if (it->second.kind == LocalInfo::Kind::Reference) {
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        }
        if (hasEntryArgs && expr.name == entryArgsName) {
          error = "native backend only supports count() on entry arguments";
          return false;
        }
        std::string mathConst;
        if (getMathConstantName(expr.name, mathConst)) {
          double value = 0.0;
          if (mathConst == "pi") {
            value = 3.14159265358979323846;
          } else if (mathConst == "tau") {
            value = 6.28318530717958647692;
          } else if (mathConst == "e") {
            value = 2.71828182845904523536;
          }
          uint64_t bits = 0;
          std::memcpy(&bits, &value, sizeof(bits));
          function.instructions.push_back({IrOpcode::PushF64, bits});
          return true;
        }
        error = "native backend does not know identifier: " + expr.name;
        return false;
      }
      case Expr::Kind::Call: {
        if (!expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 &&
            !isArrayCountCall(expr, localsIn) && !isStringCountCall(expr, localsIn)) {
          Expr methodExpr = expr;
          methodExpr.isMethodCall = true;
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee) {
            if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
              error = "native backend does not support block arguments on calls";
              return false;
            }
            return emitInlineDefinitionCall(methodExpr, *callee, localsIn, true);
          }
        }
        if (expr.isMethodCall && !isArrayCountCall(expr, localsIn) && !isStringCountCall(expr, localsIn) &&
            !isVectorCapacityCall(expr, localsIn)) {
          const Definition *callee = resolveMethodCallDefinition(expr, localsIn);
          if (!callee) {
            return false;
          }
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error = "native backend does not support block arguments on calls";
            return false;
          }
          return emitInlineDefinitionCall(expr, *callee, localsIn, true);
        }
        if (const Definition *callee = resolveDefinitionCall(expr)) {
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error = "native backend does not support block arguments on calls";
            return false;
          }
          return emitInlineDefinitionCall(expr, *callee, localsIn, true);
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 &&
            !isArrayCountCall(expr, localsIn) && !isStringCountCall(expr, localsIn)) {
          Expr methodExpr = expr;
          methodExpr.isMethodCall = true;
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee) {
            if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
              error = "native backend does not support block arguments on calls";
              return false;
            }
            return emitInlineDefinitionCall(methodExpr, *callee, localsIn, true);
          }
        }
        std::string mathName;
        if (getMathBuiltinName(expr, mathName)) {
          if (mathName == "abs" || mathName == "sign" || mathName == "min" || mathName == "max" ||
              mathName == "clamp" || mathName == "saturate" || mathName == "lerp" || mathName == "fma" ||
              mathName == "hypot" || mathName == "copysign" || mathName == "radians" || mathName == "degrees" ||
              mathName == "sin" || mathName == "cos" || mathName == "tan" || mathName == "atan2" ||
              mathName == "asin" || mathName == "acos" || mathName == "atan" || mathName == "sinh" ||
              mathName == "cosh" || mathName == "tanh" || mathName == "asinh" || mathName == "acosh" ||
              mathName == "atanh" || mathName == "exp" || mathName == "exp2" || mathName == "log" ||
              mathName == "log2" || mathName == "log10" || mathName == "pow" || mathName == "is_nan" ||
              mathName == "is_inf" || mathName == "is_finite" || mathName == "floor" || mathName == "ceil" ||
              mathName == "round" || mathName == "trunc" || mathName == "fract" || mathName == "sqrt" ||
              mathName == "cbrt") {
            // Supported in native/VM lowering.
          } else {
            error = "native backend does not support math builtin: " + mathName;
            return false;
          }
        }
        if (isArrayCountCall(expr, localsIn)) {
          if (isEntryArgsName(expr.args.front(), localsIn)) {
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (isVectorCapacityCall(expr, localsIn)) {
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::PushI64, 16});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (isStringCountCall(expr, localsIn)) {
          if (expr.args.size() != 1) {
            error = "count requires exactly one argument";
            return false;
          }
          int32_t stringIndex = -1;
          size_t length = 0;
          if (!resolveStringTableTarget(expr.args.front(), localsIn, stringIndex, length)) {
            error = "native backend only supports count() on string literals or string bindings";
            return false;
          }
          if (length > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            error = "native backend string too large for count()";
            return false;
          }
          function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(length))});
          return true;
        }
        if (isSimpleCallName(expr, "count")) {
          error = "count requires array, vector, map, or string target";
          return false;
        }
        if (isSimpleCallName(expr, "capacity")) {
          error = "capacity requires vector target";
          return false;
        }
        std::string vectorHelper;
        if (isSimpleCallName(expr, "push")) {
          vectorHelper = "push";
        } else if (isSimpleCallName(expr, "pop")) {
          vectorHelper = "pop";
        } else if (isSimpleCallName(expr, "reserve")) {
          vectorHelper = "reserve";
        } else if (isSimpleCallName(expr, "clear")) {
          vectorHelper = "clear";
        } else if (isSimpleCallName(expr, "remove_at")) {
          vectorHelper = "remove_at";
        } else if (isSimpleCallName(expr, "remove_swap")) {
          vectorHelper = "remove_swap";
        }
        if (!vectorHelper.empty()) {
          error = "native backend does not support vector helper: " + vectorHelper;
          return false;
        }
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(expr, printBuiltin)) {
          error = printBuiltin.name + " is only supported as a statement in the native backend";
          return false;
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            error = accessName + " requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();

          int32_t stringIndex = -1;
          size_t stringLength = 0;
          if (resolveStringTableTarget(target, localsIn, stringIndex, stringLength)) {
            LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              error = "native backend requires integer indices for " + accessName;
              return false;
            }
            if (stringLength > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = "native backend string too large for indexing";
              return false;
            }

            const int32_t indexLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

            if (accessName == "at") {
              if (indexKind != LocalInfo::ValueKind::UInt64) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
                function.instructions.push_back({pushZeroForIndex(indexKind), 0});
                function.instructions.push_back({cmpLtForIndex(indexKind), 0});
                size_t jumpNonNegative = function.instructions.size();
                function.instructions.push_back({IrOpcode::JumpIfZero, 0});
                emitStringIndexOutOfBounds();
                size_t nonNegativeIndex = function.instructions.size();
                function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
              }

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              IrOpcode lengthOp =
                  (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
              function.instructions.push_back({lengthOp, static_cast<uint64_t>(stringLength)});
              function.instructions.push_back({cmpGeForIndex(indexKind), 0});
              size_t jumpInRange = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitStringIndexOutOfBounds();
              size_t inRangeIndex = function.instructions.size();
              function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::LoadStringByte, static_cast<uint64_t>(stringIndex)});
            return true;
          }
          if (target.kind == Expr::Kind::StringLiteral) {
            return false;
          }
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports indexing into string literals or string bindings";
              return false;
            }
          }
          if (isEntryArgsName(target, localsIn)) {
            error = "native backend only supports entry argument indexing in print calls or string bindings";
            return false;
          }

          bool isMapTarget = false;
          LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
          LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
              isMapTarget = true;
              mapKeyKind = it->second.mapKeyKind;
              mapValueKind = it->second.mapValueKind;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2) {
              isMapTarget = true;
              mapKeyKind = valueKindFromTypeName(target.templateArgs[0]);
              mapValueKind = valueKindFromTypeName(target.templateArgs[1]);
            }
          }
          if (isMapTarget) {
            if (mapKeyKind == LocalInfo::ValueKind::Unknown || mapValueKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend requires typed map bindings for " + accessName;
              return false;
            }
            if (mapValueKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool map values";
              return false;
            }

            const int32_t ptrLocal = allocTempLocal();
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

            const int32_t keyLocal = allocTempLocal();
            if (mapKeyKind == LocalInfo::ValueKind::String) {
              int32_t stringIndex = -1;
              size_t length = 0;
              if (!resolveStringTableTarget(expr.args[1], localsIn, stringIndex, length)) {
                error =
                    "native backend requires map lookup key to be string literal or binding backed by literals";
                return false;
              }
              function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(keyLocal)});
            } else {
              LocalInfo::ValueKind lookupKeyKind = inferExprKind(expr.args[1], localsIn);
              if (lookupKeyKind == LocalInfo::ValueKind::Unknown || lookupKeyKind == LocalInfo::ValueKind::String) {
                error = "native backend requires map lookup key to be numeric/bool";
                return false;
              }
              if (lookupKeyKind != mapKeyKind) {
                error = "native backend requires map lookup key type to match map key type";
                return false;
              }
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(keyLocal)});
            }

            const int32_t countLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

            const int32_t indexLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

            size_t loopStart = function.instructions.size();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpLoopEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 2});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 16});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal)});
            IrOpcode cmpKeyOp = IrOpcode::CmpEqI32;
            if (mapKeyKind == LocalInfo::ValueKind::Int64 || mapKeyKind == LocalInfo::ValueKind::UInt64) {
              cmpKeyOp = IrOpcode::CmpEqI64;
            } else if (mapKeyKind == LocalInfo::ValueKind::Float64) {
              cmpKeyOp = IrOpcode::CmpEqF64;
            } else if (mapKeyKind == LocalInfo::ValueKind::Float32) {
              cmpKeyOp = IrOpcode::CmpEqF32;
            }
            function.instructions.push_back({cmpKeyOp, 0});
            size_t jumpNotMatch = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            size_t jumpFound = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t notMatchIndex = function.instructions.size();
            function.instructions[jumpNotMatch].imm = static_cast<int32_t>(notMatchIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

            size_t loopEndIndex = function.instructions.size();
            function.instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEndIndex);
            function.instructions[jumpFound].imm = static_cast<int32_t>(loopEndIndex);

            if (accessName == "at") {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
              function.instructions.push_back({IrOpcode::CmpEqI32, 0});
              size_t jumpKeyFound = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitMapKeyNotFound();
              size_t keyFoundIndex = function.instructions.size();
              function.instructions[jumpKeyFound].imm = static_cast<int32_t>(keyFoundIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 2});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 2});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 16});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }

          LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
          bool isVectorTarget = false;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() &&
                (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
              elemKind = it->second.valueKind;
              isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
                target.templateArgs.size() == 1) {
              elemKind = valueKindFromTypeName(target.templateArgs.front());
              isVectorTarget = (collection == "vector");
            }
          }
          if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
            error = "native backend only supports at() on numeric/bool arrays or vectors";
            return false;
          }

          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
          if (!isSupportedIndexKind(indexKind)) {
            error = "native backend requires integer indices for " + accessName;
            return false;
          }

          const uint64_t headerSlots = isVectorTarget ? 2 : 1;
          const int32_t ptrLocal = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            const int32_t countLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

            if (indexKind != LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({pushZeroForIndex(indexKind), 0});
              function.instructions.push_back({cmpLtForIndex(indexKind), 0});
              size_t jumpNonNegative = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              size_t nonNegativeIndex = function.instructions.size();
              function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
            function.instructions.push_back({cmpGeForIndex(indexKind), 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          function.instructions.push_back({pushOneForIndex(indexKind), headerSlots});
          function.instructions.push_back({addForIndex(indexKind), 0});
          function.instructions.push_back({pushOneForIndex(indexKind), 16});
          function.instructions.push_back({mulForIndex(indexKind), 0});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});

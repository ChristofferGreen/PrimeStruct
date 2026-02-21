  auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto validateLoopBody = [&](const Expr &body, const std::unordered_map<std::string, BindingInfo> &baseLocals) -> bool {
    if (!isLoopBlockEnvelope(body)) {
      error_ = "loop body requires a block envelope";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = baseLocals;
    OnErrorScope onErrorScope(*this, std::nullopt);
    for (const auto &bodyExpr : body.bodyArguments) {
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix)) {
        return false;
      }
    }
    return true;
  };
  auto isNegativeIntegerLiteral = [&](const Expr &expr) -> bool {
    if (expr.kind != Expr::Kind::Literal || expr.isUnsigned) {
      return false;
    }
    if (expr.intWidth == 32) {
      return static_cast<int32_t>(expr.literalValue) < 0;
    }
    return static_cast<int64_t>(expr.literalValue) < 0;
  };
  for (const auto &transform : stmt.transforms) {
    if (transform.name == "on_error" && !isBuiltinBlockCall(stmt)) {
      error_ = "on_error is only valid on definitions and block scopes";
      return false;
    }
  }
  if (isLoopCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "loop does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "loop does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 2) {
      error_ = "loop requires count and body";
      return false;
    }
    const Expr &count = stmt.args[0];
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64) {
      error_ = "loop count requires integer";
      return false;
    }
    if (isNegativeIntegerLiteral(count)) {
      error_ = "loop count must be non-negative";
      return false;
    }
    return validateLoopBody(stmt.args[1], locals);
  }
  if (isWhileCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "while does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "while does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 2) {
      error_ = "while requires condition and body";
      return false;
    }
    const Expr &cond = stmt.args[0];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      error_ = "while condition requires bool";
      return false;
    }
    return validateLoopBody(stmt.args[1], locals);
  }
  if (isForCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "for does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "for does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 4) {
      error_ = "for requires init, condition, step, and body";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> loopLocals = locals;
    if (!validateStatement(params, loopLocals, stmt.args[0], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    const Expr &cond = stmt.args[1];
    auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
      if (binding.typeName == "Reference") {
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
          std::vector<std::string> args;
          if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
            return ReturnKind::Array;
          }
        }
        return returnKindForTypeName(binding.typeTemplateArg);
      }
      return returnKindForTypeName(binding.typeName);
    };
    if (cond.isBinding) {
      if (!validateStatement(params, loopLocals, cond, returnKind, false, allowBindings, nullptr, namespacePrefix)) {
        return false;
      }
      auto it = loopLocals.find(cond.name);
      ReturnKind condKind = it == loopLocals.end() ? ReturnKind::Unknown : returnKindForBinding(it->second);
      if (condKind != ReturnKind::Bool) {
        error_ = "for condition requires bool";
        return false;
      }
    } else {
      if (!validateExpr(params, loopLocals, cond)) {
        return false;
      }
      ReturnKind condKind = inferExprReturnKind(cond, params, loopLocals);
      if (condKind != ReturnKind::Bool) {
        error_ = "for condition requires bool";
        return false;
      }
    }
    if (!validateStatement(params, loopLocals, stmt.args[2], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    return validateLoopBody(stmt.args[3], loopLocals);
  }
  if (isRepeatCall(stmt)) {
    if (!stmt.hasBodyArguments) {
      error_ = "repeat requires block arguments";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "repeat does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = "repeat requires exactly one argument";
      return false;
    }
    const Expr &count = stmt.args.front();
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64 &&
        countKind != ReturnKind::Bool) {
      error_ = "repeat count requires integer or bool";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    OnErrorScope onErrorScope(*this, std::nullopt);
    for (const auto &bodyExpr : stmt.bodyArguments) {
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  if (isBuiltinBlockCall(stmt) && !stmt.hasBodyArguments) {
    error_ = "block requires block arguments";
    return false;
  }
  if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
    if (hasNamedArguments(stmt.argNames) || !stmt.args.empty() || !stmt.templateArgs.empty()) {
      error_ = "block does not accept arguments";
      return false;
    }
    std::optional<OnErrorHandler> blockOnError;
    if (!parseOnErrorTransform(stmt.transforms, stmt.namespacePrefix, "block", blockOnError)) {
      return false;
    }
    if (blockOnError.has_value()) {
      OnErrorScope onErrorArgsScope(*this, std::nullopt);
      for (const auto &arg : blockOnError->boundArgs) {
        if (!validateExpr(params, locals, arg)) {
          return false;
        }
      }
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    OnErrorScope onErrorScope(*this, blockOnError);
    for (const auto &bodyExpr : stmt.bodyArguments) {
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  PrintBuiltin printBuiltin;
  if (getPrintBuiltin(stmt, printBuiltin)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = printBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = printBuiltin.name + " requires exactly one argument";
      return false;
    }
    const std::string effectName = (printBuiltin.target == PrintTarget::Err) ? "io_err" : "io_out";
    if (activeEffects_.count(effectName) == 0) {
      error_ = printBuiltin.name + " requires " + effectName + " effect";
      return false;
    }
    {
      EntryArgStringScope entryArgScope(*this, true);
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
    }
    if (!isPrintableExpr(stmt.args.front(), params, locals)) {
      error_ = printBuiltin.name + " requires an integer/bool or string literal/binding argument";
      return false;
    }
    return true;
  }
  PathSpaceBuiltin pathSpaceBuiltin;
  if (getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && defMap_.find(resolveCalleePath(stmt)) == defMap_.end()) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
      error_ = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
               " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
      return false;
    }
    if (activeEffects_.count(pathSpaceBuiltin.requiredEffect) == 0) {
      error_ = pathSpaceBuiltin.name + " requires " + pathSpaceBuiltin.requiredEffect + " effect";
      return false;
    }
    auto isStringExpr = [&](const Expr &candidate) -> bool {
      if (candidate.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      auto isStringTypeName = [&](const std::string &typeName) -> bool {
        return normalizeBindingTypeName(typeName) == "string";
      };
      auto isStringArrayBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "array" && binding.typeName != "vector") {
          return false;
        }
        return isStringTypeName(binding.typeTemplateArg);
      };
      auto isStringMapBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(binding.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        return isStringTypeName(parts[1]);
      };
      auto isStringCollectionTarget = [&](const Expr &target) -> bool {
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            return isStringArrayBinding(*paramBinding) || isStringMapBinding(*paramBinding);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            return isStringArrayBinding(it->second) || isStringMapBinding(it->second);
          }
        }
        if (target.kind == Expr::Kind::Call) {
          std::string collection;
          if (!getBuiltinCollectionName(target, collection)) {
            return false;
          }
          if ((collection == "array" || collection == "vector") && target.templateArgs.size() == 1) {
            return isStringTypeName(target.templateArgs.front());
          }
          if (collection == "map" && target.templateArgs.size() == 2) {
            return isStringTypeName(target.templateArgs[1]);
          }
        }
        return false;
      };
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          return isStringTypeName(paramBinding->typeName);
        }
        auto it = locals.find(candidate.name);
        return it != locals.end() && isStringTypeName(it->second.typeName);
      }
      if (candidate.kind == Expr::Kind::Call) {
        std::string accessName;
        if (getBuiltinArrayAccessName(candidate, accessName) && candidate.args.size() == 2) {
          return isStringCollectionTarget(candidate.args.front());
        }
      }
      return false;
    };
    if (!isStringExpr(stmt.args.front())) {
      error_ = pathSpaceBuiltin.name + " requires string path argument";
      return false;
    }
    for (const auto &arg : stmt.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }
  std::string vectorHelper;
  if (getVectorStatementHelperName(stmt, vectorHelper)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = vectorHelper + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = vectorHelper + " does not accept block arguments";
      return false;
    }
    if (vectorHelper == "push") {
      if (stmt.args.size() != 2) {
        error_ = "push requires exactly two arguments";
        return false;
      }
      if (activeEffects_.count("heap_alloc") == 0) {
        error_ = "push requires heap_alloc effect";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), "push", binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front()) ||
          !validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      if (!validateVectorElementType(stmt.args[1], binding->typeTemplateArg)) {
        return false;
      }
      return true;
    }
    if (vectorHelper == "reserve") {
      if (stmt.args.size() != 2) {
        error_ = "reserve requires exactly two arguments";
        return false;
      }
      if (activeEffects_.count("heap_alloc") == 0) {
        error_ = "reserve requires heap_alloc effect";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), "reserve", binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front()) ||
          !validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      if (!isIntegerOrBoolExpr(stmt.args[1])) {
        error_ = "reserve requires integer capacity";
        return false;
      }
      return true;
    }
    if (vectorHelper == "remove_at" || vectorHelper == "remove_swap") {
      if (stmt.args.size() != 2) {
        error_ = vectorHelper + " requires exactly two arguments";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), vectorHelper.c_str(), binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front()) ||
          !validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      if (!isIntegerOrBoolExpr(stmt.args[1])) {
        error_ = vectorHelper + " requires integer index";
        return false;
      }
      return true;
    }
    if (vectorHelper == "pop" || vectorHelper == "clear") {
      if (stmt.args.size() != 1) {
        error_ = vectorHelper + " requires exactly one argument";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), vectorHelper.c_str(), binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      return true;
    }
  }
  auto resolveBodyArgumentTarget = [&](const Expr &callExpr, std::string &resolvedOut) {
    if (!callExpr.isMethodCall) {
      resolvedOut = resolveCalleePath(callExpr);
      return;
    }
    if (callExpr.args.empty()) {
      resolvedOut = resolveCalleePath(callExpr);
      return;
    }
    const Expr &receiver = callExpr.args.front();
    const std::string &methodName = callExpr.name;
    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
      const std::string resolvedType = resolveCalleePath(receiver);
      if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
        resolvedOut = resolvedType + "/" + methodName;
        return;
      }
    }
    std::string typeName;
    if (receiver.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        typeName = paramBinding->typeName;
      } else {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          typeName = it->second.typeName;
        }
      }
    }
    if (typeName.empty()) {
      ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
      std::string inferred;
      if (inferredKind == ReturnKind::Array) {
        inferred = inferStructReturnPath(receiver, params, locals);
        if (inferred.empty()) {
          inferred = typeNameForReturnKind(inferredKind);
        }
      } else {
        inferred = typeNameForReturnKind(inferredKind);
      }
      if (!inferred.empty()) {
        typeName = inferred;
      }
    }
    if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
      resolvedOut = resolveCalleePath(callExpr);
      return;
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + methodName;
      return;
    }
    std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    resolvedOut = resolvedType + "/" + methodName;
  };

  if ((stmt.hasBodyArguments || !stmt.bodyArguments.empty()) && !isBuiltinBlockCall(stmt)) {
    std::string collectionName;
    if (getBuiltinCollectionName(stmt, collectionName)) {
      error_ = collectionName + " literal does not accept block arguments";
      return false;
    }
    std::string resolved;
    resolveBodyArgumentTarget(stmt, resolved);
    if (defMap_.count(resolved) == 0) {
      error_ = "block arguments require a definition target: " + resolved;
      return false;
    }
    Expr call = stmt;
    call.bodyArguments.clear();
    call.hasBodyArguments = false;
    if (!validateExpr(params, locals, call)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    for (const auto &bodyExpr : stmt.bodyArguments) {
      if (!validateStatement(params, blockLocals, bodyExpr, returnKind, false, true, nullptr, namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  return validateExpr(params, locals, stmt);
}

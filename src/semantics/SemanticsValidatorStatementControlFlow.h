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
  auto validateLoopBody = [&](const Expr &body,
                              const std::unordered_map<std::string, BindingInfo> &baseLocals,
                              const std::vector<Expr> &boundaryStatements,
                              bool includeNextIterationBody) -> bool {
    if (!isLoopBlockEnvelope(body)) {
      error_ = "loop body requires a block envelope";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = baseLocals;
    std::vector<Expr> livenessStatements = body.bodyArguments;
    livenessStatements.insert(livenessStatements.end(), boundaryStatements.begin(), boundaryStatements.end());
    if (includeNextIterationBody) {
      livenessStatements.insert(livenessStatements.end(), body.bodyArguments.begin(), body.bodyArguments.end());
    }
    OnErrorScope onErrorScope(*this, std::nullopt);
    BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
    for (size_t bodyIndex = 0; bodyIndex < body.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = body.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             &body.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
    }
    return true;
  };
  auto isBoolLiteralFalse = [](const Expr &expr) -> bool { return expr.kind == Expr::Kind::BoolLiteral && !expr.boolValue; };
  auto isNegativeIntegerLiteral = [&](const Expr &expr) -> bool {
    if (expr.kind != Expr::Kind::Literal || expr.isUnsigned) {
      return false;
    }
    if (expr.intWidth == 32) {
      return static_cast<int32_t>(expr.literalValue) < 0;
    }
    return static_cast<int64_t>(expr.literalValue) < 0;
  };
  auto canIterateMoreThanOnce = [&](const Expr &countExpr, bool allowBoolCount) -> bool {
    return runSemanticsValidatorStatementCanIterateMoreThanOnceStep(countExpr, allowBoolCount);
  };
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
    const bool includeNextIterationBody = canIterateMoreThanOnce(count, false);
    return validateLoopBody(stmt.args[1], locals, {}, includeNextIterationBody);
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
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[1], locals, boundaryStatements, !conditionAlwaysFalse);
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
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = !cond.isBinding && isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(stmt.args[2]);
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[3], loopLocals, boundaryStatements, !conditionAlwaysFalse);
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
    const bool includeNextIterationBody = canIterateMoreThanOnce(count, true);
    Expr repeatBody;
    repeatBody.kind = Expr::Kind::Call;
    repeatBody.name = "repeat_body";
    repeatBody.bodyArguments = stmt.bodyArguments;
    repeatBody.hasBodyArguments = stmt.hasBodyArguments;
    return validateLoopBody(repeatBody, locals, {}, includeNextIterationBody);
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
    BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             &stmt.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, stmt.bodyArguments, bodyIndex + 1);
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
  auto isIntegerKind = [&](ReturnKind kind) -> bool {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64;
  };
  auto isNumericOrBoolKind = [&](ReturnKind kind) -> bool {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
           kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool;
  };
  auto resolveArrayElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "array" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "array" && !itLocal->second.typeTemplateArg.empty()) {
          elemType = itLocal->second.typeTemplateArg;
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(arg, collection) && collection == "array" && arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
    }
    return false;
  };
  auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "Buffer" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "Buffer" && !itLocal->second.typeTemplateArg.empty()) {
          elemType = itLocal->second.typeTemplateArg;
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (isSimpleCallName(arg, "buffer") && arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
      if (isSimpleCallName(arg, "upload") && arg.args.size() == 1) {
        return resolveArrayElemType(arg.args.front(), elemType);
      }
    }
    return false;
  };
  if (isSimpleCallName(stmt, "dispatch")) {
    if (currentDefinitionIsCompute_) {
      error_ = "dispatch is not allowed in compute definitions";
      return false;
    }
    if (activeEffects_.count("gpu_dispatch") == 0) {
      error_ = "dispatch requires gpu_dispatch effect";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "dispatch does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "dispatch does not accept block arguments";
      return false;
    }
    if (stmt.args.size() < 4) {
      error_ = "dispatch requires kernel and three dimension arguments";
      return false;
    }
    if (stmt.args.front().kind != Expr::Kind::Name) {
      error_ = "dispatch requires kernel name as first argument";
      return false;
    }
    const Expr &kernelExpr = stmt.args.front();
    const std::string kernelPath = resolveCalleePath(kernelExpr);
    auto defIt = defMap_.find(kernelPath);
    if (defIt == defMap_.end()) {
      error_ = "unknown kernel: " + kernelPath;
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
      error_ = "dispatch requires compute definition: " + kernelPath;
      return false;
    }
    for (size_t i = 1; i <= 3; ++i) {
      if (!validateExpr(params, locals, stmt.args[i])) {
        return false;
      }
      ReturnKind dimKind = inferExprReturnKind(stmt.args[i], params, locals);
      if (!isIntegerKind(dimKind)) {
        error_ = "dispatch dimensions require integer expressions";
        return false;
      }
    }
    const auto &kernelParams = paramsByDef_[kernelPath];
    if (stmt.args.size() != kernelParams.size() + 4) {
      error_ = "dispatch argument count mismatch for " + kernelPath;
      return false;
    }
    for (size_t i = 4; i < stmt.args.size(); ++i) {
      if (!validateExpr(params, locals, stmt.args[i])) {
        return false;
      }
    }
    return true;
  }
  if (isSimpleCallName(stmt, "buffer_store")) {
    if (!currentDefinitionIsCompute_) {
      error_ = "buffer_store requires a compute definition";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "buffer_store does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = "buffer_store requires buffer, index, and value arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "buffer_store does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args[0]) || !validateExpr(params, locals, stmt.args[1]) ||
        !validateExpr(params, locals, stmt.args[2])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(stmt.args[0], elemType)) {
      error_ = "buffer_store requires Buffer input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolKind(elemKind)) {
      error_ = "buffer_store requires numeric/bool element type";
      return false;
    }
    ReturnKind indexKind = inferExprReturnKind(stmt.args[1], params, locals);
    if (!isIntegerKind(indexKind)) {
      error_ = "buffer_store requires integer index";
      return false;
    }
    ReturnKind valueKind = inferExprReturnKind(stmt.args[2], params, locals);
    if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
      error_ = "buffer_store value type mismatch";
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

  if ((stmt.hasBodyArguments || !stmt.bodyArguments.empty()) && !isBuiltinBlockCall(stmt) && !stmt.isLambda) {
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
    std::vector<Expr> livenessStatements = stmt.bodyArguments;
    if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
      for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
        livenessStatements.push_back((*enclosingStatements)[idx]);
      }
    }
    BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             false,
                             true,
                             nullptr,
                             namespacePrefix,
                             &stmt.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
    }
    return true;
  }
  return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
}

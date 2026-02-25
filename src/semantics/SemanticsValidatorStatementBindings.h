  if (stmt.isBinding) {
    if (!allowBindings) {
      error_ = "binding not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "binding does not accept block arguments";
      return false;
    }
    if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
      error_ = "duplicate binding name: " + stmt.name;
      return false;
    }
    BindingInfo info;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(stmt, namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = "binding requires exactly one argument";
      return false;
    }
    const Expr &initializer = stmt.args.front();
    const bool entryArgInit = isEntryArgsAccess(initializer);
    const bool entryArgStringInit = isEntryArgStringBinding(locals, initializer);
    std::optional<EntryArgStringScope> entryArgScope;
    if (entryArgInit || entryArgStringInit) {
      entryArgScope.emplace(*this, true);
    }
    if (!validateExpr(params, locals, initializer)) {
      return false;
    }
    auto isStructConstructor = [&](const Expr &expr) -> bool {
      if (expr.kind != Expr::Kind::Call || expr.isBinding) {
        return false;
      }
      const std::string resolved = resolveCalleePath(expr);
      return structNames_.count(resolved) > 0;
    };
    auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return nullptr;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return nullptr;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return nullptr;
      }
      if (!allowAnyName && !isBuiltinBlockCall(candidate)) {
        return nullptr;
      }
      const Expr *valueExpr = nullptr;
      for (const auto &bodyExpr : candidate.bodyArguments) {
        if (bodyExpr.isBinding) {
          continue;
        }
        valueExpr = &bodyExpr;
      }
      return valueExpr;
    };
    std::function<bool(const Expr &)> isStructConstructorValueExpr;
    isStructConstructorValueExpr = [&](const Expr &candidate) -> bool {
      if (isStructConstructor(candidate)) {
        return true;
      }
      if (isIfCall(candidate) && candidate.args.size() == 3) {
        const Expr &thenArg = candidate.args[1];
        const Expr &elseArg = candidate.args[2];
        const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
        const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
        const Expr &thenExpr = thenValue ? *thenValue : thenArg;
        const Expr &elseExpr = elseValue ? *elseValue : elseArg;
        return isStructConstructorValueExpr(thenExpr) && isStructConstructorValueExpr(elseExpr);
      }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      return isStructConstructorValueExpr(*valueExpr);
    }
    return false;
  };
  auto isSimpleUninitializedName = [&](const Expr &expr, const char *name) -> bool {
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding) {
      return false;
    }
    if (!expr.templateArgs.empty() || hasNamedArguments(expr.argNames) ||
        expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return false;
    }
    return isSimpleCallName(expr, name);
  };
  ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
  if (initKind == ReturnKind::Void &&
      !isStructConstructorValueExpr(initializer)) {
    if (isSimpleUninitializedName(initializer, "init") ||
        isSimpleUninitializedName(initializer, "drop") ||
        isSimpleUninitializedName(initializer, "take") ||
        isSimpleUninitializedName(initializer, "borrow")) {
      error_ = "uninitialized helpers are only valid as statements";
      return false;
    }
    error_ = "binding initializer requires a value";
    return false;
  }
    if (!hasExplicitBindingTypeTransform(stmt)) {
      (void)inferBindingTypeFromInitializer(initializer, params, locals, info);
    }
    if (info.typeName == "uninitialized") {
      if (info.typeTemplateArg.empty()) {
        error_ = "uninitialized requires exactly one template argument";
        return false;
      }
      if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isBinding) {
        error_ = "uninitialized bindings require uninitialized<T>() initializer";
        return false;
      }
      if (initializer.name != "uninitialized" && initializer.name != "/uninitialized") {
        error_ = "uninitialized bindings require uninitialized<T>() initializer";
        return false;
      }
      if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() || !initializer.args.empty()) {
        error_ = "uninitialized does not accept arguments";
        return false;
      }
      if (initializer.templateArgs.size() != 1 ||
          !errorTypesMatch(info.typeTemplateArg, initializer.templateArgs.front(), namespacePrefix)) {
        error_ = "uninitialized initializer type mismatch";
        return false;
      }
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    if (entryArgInit || entryArgStringInit) {
      if (normalizeBindingTypeName(info.typeName) != "string") {
        error_ = "entry argument strings require string bindings";
        return false;
      }
      info.isEntryArgString = true;
    }
    if (info.typeName == "Reference") {
      const Expr &init = initializer;
      std::string pointerName;
      if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
          pointerName != "location" || init.args.size() != 1) {
        error_ = "Reference bindings require location(...)";
        return false;
      }
    }
    locals.emplace(stmt.name, info);
    return true;
  }
  std::optional<EffectScope> effectScope;
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.transforms.empty() && !isBuiltinBlockCall(stmt)) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(stmt, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  if (stmt.kind != Expr::Kind::Call) {
    if (!allowBindings) {
      error_ = "execution body arguments must be calls";
      return false;
    }
    return validateExpr(params, locals, stmt);
  }
  if (isReturnCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!allowReturn) {
      error_ = "return not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "return does not accept block arguments";
      return false;
    }
    if (returnKind == ReturnKind::Void) {
      if (!stmt.args.empty()) {
        error_ = "return value not allowed for void definition";
        return false;
      }
    } else {
      if (stmt.args.size() != 1) {
        error_ = "return requires exactly one argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (returnKind != ReturnKind::Unknown) {
        ReturnKind exprKind = inferExprReturnKind(stmt.args.front(), params, locals);
        if (returnKind == ReturnKind::Array) {
          auto structIt = returnStructs_.find(currentDefinitionPath_);
          if (structIt != returnStructs_.end()) {
            std::string actualStruct = inferStructReturnPath(stmt.args.front(), params, locals);
            if (actualStruct.empty() || actualStruct != structIt->second) {
              error_ = "return type mismatch: expected " + structIt->second;
              return false;
            }
          } else if (exprKind != ReturnKind::Array) {
            error_ = "return type mismatch: expected array";
            return false;
          }
        } else if (exprKind != returnKind) {
          error_ = "return type mismatch: expected " + typeNameForReturnKind(returnKind);
          return false;
        }
      }
    }
    if (sawReturn) {
      *sawReturn = true;
    }
    return true;
  }
  if (isIfCall(stmt)) {
    const std::string keyword = isSimpleCallName(stmt, "match") ? "match" : "if";
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = keyword + " does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = keyword + " requires condition, then, else";
      return false;
    }
    const Expr &cond = stmt.args[0];
    const Expr &thenArg = stmt.args[1];
    const Expr &elseArg = stmt.args[2];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      error_ = keyword + " condition requires bool";
      return false;
    }
    auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return true;
    };
    auto validateBranch = [&](const Expr &branch) -> bool {
      if (!isIfBlockEnvelope(branch)) {
        error_ = keyword + " branches require block envelopes";
        return false;
      }
      std::unordered_map<std::string, BindingInfo> branchLocals = locals;
      OnErrorScope onErrorScope(*this, std::nullopt);
      for (const auto &bodyExpr : branch.bodyArguments) {
        if (!validateStatement(params,
                               branchLocals,
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
    if (!validateBranch(thenArg)) {
      return false;
    }
    if (!validateBranch(elseArg)) {
      return false;
    }
    return true;
  }

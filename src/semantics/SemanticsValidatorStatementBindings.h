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
    ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
    if (initKind == ReturnKind::Void &&
        !isStructConstructorValueExpr(initializer)) {
      error_ = "binding initializer requires a value";
      return false;
    }
    if (!hasExplicitBindingTypeTransform(stmt)) {
      (void)inferBindingTypeFromInitializer(initializer, params, locals, info);
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
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.transforms.empty()) {
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
        if (exprKind != returnKind) {
          if (returnKind == ReturnKind::Array) {
            error_ = "return type mismatch: expected array";
          } else {
            error_ = "return type mismatch: expected " + typeNameForReturnKind(returnKind);
          }
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
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "if does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = "if requires condition, then, else";
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
      error_ = "if condition requires bool";
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
        error_ = "if branches require block envelopes";
        return false;
      }
      std::unordered_map<std::string, BindingInfo> branchLocals = locals;
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

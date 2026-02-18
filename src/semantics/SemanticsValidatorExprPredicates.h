bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr) {
  if (expr.isLambda) {
    auto splitTokens = [](const std::string &text) {
      std::vector<std::string> tokens;
      std::string token;
      bool inToken = false;
      for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
          if (inToken) {
            tokens.push_back(token);
            token.clear();
            inToken = false;
          }
          continue;
        }
        token.push_back(c);
        inToken = true;
      }
      if (inToken) {
        tokens.push_back(token);
      }
      return tokens;
    };
    auto addCapturedBinding = [&](std::unordered_map<std::string, BindingInfo> &lambdaLocals,
                                  const std::string &name) -> bool {
      if (lambdaLocals.count(name) > 0) {
        error_ = "duplicate lambda capture: " + name;
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        lambdaLocals.emplace(name, *paramBinding);
        return true;
      }
      auto it = locals.find(name);
      if (it != locals.end()) {
        lambdaLocals.emplace(name, it->second);
        return true;
      }
      error_ = "unknown capture: " + name;
      return false;
    };

    if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
      error_ = "lambda requires a body";
      return false;
    }

    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!expr.lambdaCaptures.empty()) {
      bool captureAll = false;
      std::string captureAllToken;
      std::vector<std::string> captureNames;
      std::unordered_set<std::string> explicitNames;
      captureNames.reserve(expr.lambdaCaptures.size());
      explicitNames.reserve(expr.lambdaCaptures.size());
      for (const auto &capture : expr.lambdaCaptures) {
        std::vector<std::string> tokens = splitTokens(capture);
        if (tokens.empty()) {
          error_ = "invalid lambda capture";
          return false;
        }
        if (tokens.size() == 1) {
          const std::string &token = tokens[0];
          if (token == "=" || token == "&") {
            if (!captureAllToken.empty()) {
              error_ = "invalid lambda capture";
              return false;
            }
            captureAllToken = token;
            captureAll = true;
            continue;
          }
          if (!explicitNames.insert(token).second) {
            error_ = "duplicate lambda capture: " + token;
            return false;
          }
          captureNames.push_back(token);
          continue;
        }
        if (tokens.size() == 2) {
          const std::string &qualifier = tokens[0];
          const std::string &name = tokens[1];
          if (qualifier != "value" && qualifier != "ref") {
            error_ = "invalid lambda capture";
            return false;
          }
          if (name == "=" || name == "&") {
            error_ = "invalid lambda capture";
            return false;
          }
          if (!explicitNames.insert(name).second) {
            error_ = "duplicate lambda capture: " + name;
            return false;
          }
          captureNames.push_back(name);
          continue;
        }
        error_ = "invalid lambda capture";
        return false;
      }
      if (captureAll) {
        for (const auto &param : params) {
          if (!addCapturedBinding(lambdaLocals, param.name)) {
            return false;
          }
        }
        for (const auto &entry : locals) {
          if (!addCapturedBinding(lambdaLocals, entry.first)) {
            return false;
          }
        }
        for (const auto &name : captureNames) {
          if (findParamBinding(params, name) || locals.count(name) > 0) {
            continue;
          }
          error_ = "unknown capture: " + name;
          return false;
        }
      } else {
        for (const auto &name : captureNames) {
          if (!addCapturedBinding(lambdaLocals, name)) {
            return false;
          }
        }
      }
    }

    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> lambdaParams;
    lambdaParams.reserve(expr.args.size());
    for (const auto &param : expr.args) {
      if (!param.isBinding) {
        error_ = "lambda parameters must use binding syntax";
        return false;
      }
      if (param.hasBodyArguments || !param.bodyArguments.empty()) {
        error_ = "lambda parameter does not accept block arguments: " + param.name;
        return false;
      }
      if (!seen.insert(param.name).second) {
        error_ = "duplicate parameter: " + param.name;
        return false;
      }
      if (lambdaLocals.count(param.name) > 0) {
        error_ = "duplicate binding name: " + param.name;
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(param, expr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (param.args.size() > 1) {
        error_ = "lambda parameter defaults accept at most one argument: " + param.name;
        return false;
      }
      if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front())) {
        if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
          error_ = "lambda parameter default does not accept named arguments: " + param.name;
        } else {
          error_ = "lambda parameter default must be a literal or pure expression: " + param.name;
        }
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      ParameterInfo info;
      info.name = param.name;
      info.binding = std::move(binding);
      if (param.args.size() == 1) {
        info.defaultExpr = &param.args.front();
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.binding.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType,
                                    info.binding.typeName,
                                    info.binding.typeTemplateArg,
                                    hasTemplate,
                                    expr.namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      lambdaLocals.emplace(info.name, info.binding);
      lambdaParams.push_back(std::move(info));
    }

    bool sawReturn = false;
    for (const auto &stmt : expr.bodyArguments) {
      if (!validateStatement(lambdaParams,
                             lambdaLocals,
                             stmt,
                             ReturnKind::Unknown,
                             true,
                             true,
                             &sawReturn,
                             expr.namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  if (!allowEntryArgStringUse_) {
    if (isEntryArgsAccess(expr) || isEntryArgStringBinding(locals, expr)) {
      error_ = "entry argument strings are only supported in print calls or string bindings";
      return false;
    }
  }
  std::optional<EffectScope> effectScope;
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.transforms.empty()) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(expr, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  auto isMutableBinding = [&](const std::vector<ParameterInfo> &paramsIn,
                              const std::unordered_map<std::string, BindingInfo> &localsIn,
                              const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(paramsIn, name)) {
      return paramBinding->isMutable;
    }
    auto it = localsIn.find(name);
    return it != localsIn.end() && it->second.isMutable;
  };
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };
  auto isIntegerOrBoolExpr = [&](const Expr &arg,
                                 const std::vector<ParameterInfo> &paramsIn,
                                 const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Void) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, paramsIn, localsIn)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Bool;
        }
        auto it = localsIn.find(arg.name);
        if (it != localsIn.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto isBoolExpr = [&](const Expr &arg,
                        const std::vector<ParameterInfo> &paramsIn,
                        const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Void || kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, paramsIn, localsIn)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Bool;
        }
        auto it = localsIn.find(arg.name);
        if (it != localsIn.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto isNumericExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Void) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
        }
      }
      return true;
    }
    return false;
  };
  auto isFloatExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
        kind == ReturnKind::UInt64) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral) {
        return true;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
        }
      }
      return true;
    }
    return false;
  };
  enum class NumericCategory { Unknown, Integer, Float };
  auto numericCategoryForExpr = [&](const Expr &arg) -> NumericCategory {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return NumericCategory::Float;
    }
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return NumericCategory::Integer;
    }
    if (kind == ReturnKind::Void) {
      return NumericCategory::Unknown;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral) {
        return NumericCategory::Float;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return NumericCategory::Unknown;
      }
      if (isPointerExpr(arg, params, locals)) {
        return NumericCategory::Unknown;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          const std::string &typeName = paramBinding->typeName;
          if (typeName == "float" || typeName == "f32" || typeName == "f64") {
            return NumericCategory::Float;
          }
          if (typeName == "int" || typeName == "i32" || typeName == "i64" || typeName == "u64" ||
              typeName == "bool") {
            return NumericCategory::Integer;
          }
          if (typeName == "Reference") {
            const std::string &refType = paramBinding->typeTemplateArg;
            if (refType == "float" || refType == "f32" || refType == "f64") {
              return NumericCategory::Float;
            }
            if (refType == "int" || refType == "i32" || refType == "i64" || refType == "u64" ||
                refType == "bool") {
              return NumericCategory::Integer;
            }
          }
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          const std::string &typeName = it->second.typeName;
          if (typeName == "float" || typeName == "f32" || typeName == "f64") {
            return NumericCategory::Float;
          }
          if (typeName == "int" || typeName == "i32" || typeName == "i64" || typeName == "u64" ||
              typeName == "bool") {
            return NumericCategory::Integer;
          }
          if (typeName == "Reference") {
            const std::string &refType = it->second.typeTemplateArg;
            if (refType == "float" || refType == "f32" || refType == "f64") {
              return NumericCategory::Float;
            }
            if (refType == "int" || refType == "i32" || refType == "i64" || refType == "u64" ||
                refType == "bool") {
              return NumericCategory::Integer;
            }
          }
        }
      }
      return NumericCategory::Unknown;
    }
    return NumericCategory::Unknown;
  };
  auto hasMixedNumericCategory = [&](const std::vector<Expr> &args) -> bool {
    bool sawFloat = false;
    bool sawInteger = false;
    for (const auto &arg : args) {
      NumericCategory category = numericCategoryForExpr(arg);
      if (category == NumericCategory::Float) {
        sawFloat = true;
      } else if (category == NumericCategory::Integer) {
        sawInteger = true;
      }
    }
    return sawFloat && sawInteger;
  };
  auto isComparisonOperand = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Bool || kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return true;
    }
    if (kind == ReturnKind::Void) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Bool || paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                 paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Bool || localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                 localKind == ReturnKind::UInt64 || localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
        }
      }
      return true;
    }
    return false;
  };
  auto isUnsignedExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    return kind == ReturnKind::UInt64;
  };
  auto hasMixedSignedness = [&](const std::vector<Expr> &args, bool boolCountsAsSigned) -> bool {
    bool sawUnsigned = false;
    bool sawSigned = false;
    for (const auto &arg : args) {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::UInt64) {
        sawUnsigned = true;
        continue;
      }
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || (boolCountsAsSigned && kind == ReturnKind::Bool)) {
        sawSigned = true;
      }
    }
    return sawUnsigned && sawSigned;
  };
  auto isStructConstructor = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return false;
    }
    const std::string resolved = resolveCalleePath(candidate);
    return structNames_.count(resolved) > 0;
  };
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
  auto getEnvelopeValueExpr = [&](const Expr &candidate) -> const Expr * {
    if (!isIfBlockEnvelope(candidate)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
      const Expr &bodyExpr = candidate.bodyArguments[i];
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
      const Expr *thenValue = getEnvelopeValueExpr(thenArg);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg);
      const Expr &thenExpr = thenValue ? *thenValue : thenArg;
      const Expr &elseExpr = elseValue ? *elseValue : elseArg;
      return isStructConstructorValueExpr(thenExpr) && isStructConstructorValueExpr(elseExpr);
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate)) {
      return isStructConstructorValueExpr(*valueExpr);
    }
    return false;
  };

  if (expr.kind == Expr::Kind::Literal) {
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    ParsedStringLiteral parsed;
    if (!parseStringLiteralToken(expr.stringValue, parsed, error_)) {
      return false;
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      error_ = "ascii string literal contains non-ASCII characters";
      return false;
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
      return true;
    }
    if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos &&
        isBuiltinMathConstant(expr.name, true)) {
      error_ = "math constant requires import /math/* or /math/<name>: " + expr.name;
      return false;
    }
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return true;
    }
    error_ = "unknown identifier: " + expr.name;
    return false;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isBinding) {
      error_ = "binding not allowed in expression context";
      return false;
    }
    std::optional<EffectScope> effectScope;
    if (!expr.transforms.empty()) {
      std::unordered_set<std::string> executionEffects;
      if (!resolveExecutionEffects(expr, executionEffects)) {
        return false;
      }
      effectScope.emplace(*this, std::move(executionEffects));
    }
    if (isIfCall(expr)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "if does not accept trailing block arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error_ = "if requires condition, then, else";
        return false;
      }
      const Expr &cond = expr.args[0];
      const Expr &thenArg = expr.args[1];
      const Expr &elseArg = expr.args[2];
      if (!validateExpr(params, locals, cond)) {
        return false;
      }
      ReturnKind condKind = inferExprReturnKind(cond, params, locals);
      if (condKind != ReturnKind::Bool) {
        error_ = "if condition requires bool";
        return false;
      }
      auto isStringValue = [&](const Expr &valueExpr,
                               const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
        if (valueExpr.kind == Expr::Kind::StringLiteral) {
          return true;
        }
        if (valueExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, valueExpr.name)) {
            return paramBinding->typeName == "string";
          }
          auto it = localsIn.find(valueExpr.name);
          return it != localsIn.end() && it->second.typeName == "string";
        }
        if (valueExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string accessName;
        if (!getBuiltinArrayAccessName(valueExpr, accessName) || valueExpr.args.size() != 2) {
          return false;
        }
        const Expr &target = valueExpr.args.front();
        auto isStringCollectionTarget = [&](const Expr &collectionExpr) -> bool {
          if (collectionExpr.kind == Expr::Kind::StringLiteral) {
            return true;
          }
          if (collectionExpr.kind == Expr::Kind::Name) {
            const BindingInfo *binding = findParamBinding(params, collectionExpr.name);
            if (!binding) {
              auto it = localsIn.find(collectionExpr.name);
              if (it != localsIn.end()) {
                binding = &it->second;
              }
            }
            if (!binding) {
              return false;
            }
            if (binding->typeName == "string") {
              return true;
            }
            if (binding->typeName == "array" || binding->typeName == "vector") {
              return normalizeBindingTypeName(binding->typeTemplateArg) == "string";
            }
            if (binding->typeName == "map") {
              std::vector<std::string> parts;
              if (!splitTopLevelTemplateArgs(binding->typeTemplateArg, parts) || parts.size() != 2) {
                return false;
              }
              return normalizeBindingTypeName(parts[1]) == "string";
            }
            return false;
          }
          if (collectionExpr.kind == Expr::Kind::Call) {
            std::string collection;
            if (!getBuiltinCollectionName(collectionExpr, collection)) {
              return false;
            }
            if ((collection == "array" || collection == "vector") && collectionExpr.templateArgs.size() == 1) {
              return normalizeBindingTypeName(collectionExpr.templateArgs.front()) == "string";
            }
            if (collection == "map" && collectionExpr.templateArgs.size() == 2) {
              return normalizeBindingTypeName(collectionExpr.templateArgs[1]) == "string";
            }
          }
          return false;
        };
        return isStringCollectionTarget(target);
      };
      auto validateBranchValueKind =
          [&](const Expr &branch, const char *label, ReturnKind &kindOut, bool &stringOut) -> bool {
        kindOut = ReturnKind::Unknown;
        stringOut = false;
        if (isIfBlockEnvelope(branch)) {
          if (branch.bodyArguments.empty()) {
            error_ = std::string(label) + " block must produce a value";
            return false;
          }
          std::unordered_map<std::string, BindingInfo> branchLocals = locals;
          const Expr *valueExpr = nullptr;
          bool sawReturn = false;
          for (const auto &bodyExpr : branch.bodyArguments) {
            if (bodyExpr.isBinding) {
              if (isParam(params, bodyExpr.name) || branchLocals.count(bodyExpr.name) > 0) {
                error_ = "duplicate binding name: " + bodyExpr.name;
                return false;
              }
              BindingInfo info;
              std::optional<std::string> restrictType;
              if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
                return false;
              }
              if (bodyExpr.args.size() != 1) {
                error_ = "binding requires exactly one argument";
                return false;
              }
              if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
                return false;
              }
              ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, branchLocals);
              if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
                error_ = "binding initializer requires a value";
                return false;
              }
              if (!hasExplicitBindingTypeTransform(bodyExpr)) {
                (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, branchLocals, info);
              }
              if (restrictType.has_value()) {
                const bool hasTemplate = !info.typeTemplateArg.empty();
                if (!restrictMatchesBinding(*restrictType,
                                            info.typeName,
                                            info.typeTemplateArg,
                                            hasTemplate,
                                            bodyExpr.namespacePrefix)) {
                  error_ = "restrict type does not match binding type";
                  return false;
                }
              }
              if (info.typeName == "Reference") {
                const Expr &init = bodyExpr.args.front();
                std::string pointerName;
                if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
                    pointerName != "location" || init.args.size() != 1) {
                  error_ = "Reference bindings require location(...)";
                  return false;
                }
              }
              branchLocals.emplace(bodyExpr.name, info);
              continue;
            }
            if (isReturnCall(bodyExpr)) {
              if (bodyExpr.args.size() != 1) {
                error_ = std::string("return requires a value in ") + label + " block";
                return false;
              }
              if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
                return false;
              }
              valueExpr = &bodyExpr.args.front();
              sawReturn = true;
              continue;
            }
            if (!validateExpr(params, branchLocals, bodyExpr)) {
              return false;
            }
            if (!sawReturn) {
              valueExpr = &bodyExpr;
            }
          }
          if (!valueExpr) {
            error_ = std::string(label) + " block must end with an expression";
            return false;
          }
          kindOut = inferExprReturnKind(*valueExpr, params, branchLocals);
          stringOut = isStringValue(*valueExpr, branchLocals);
          if (kindOut == ReturnKind::Void) {
            if (isStructConstructorValueExpr(*valueExpr)) {
              kindOut = ReturnKind::Unknown;
            } else {
              error_ = "if branches must produce a value";
              return false;
            }
          }
          return true;
        }
        error_ = "if branches require block envelopes";
        return false;
      };

      ReturnKind thenKind = ReturnKind::Unknown;
      ReturnKind elseKind = ReturnKind::Unknown;
      bool thenIsString = false;
      bool elseIsString = false;
      if (!validateBranchValueKind(thenArg, "then", thenKind, thenIsString)) {
        return false;
      }
      if (!validateBranchValueKind(elseArg, "else", elseKind, elseIsString)) {
        return false;
      }
      if (thenIsString != elseIsString) {
        error_ = "if branches must return compatible types";
        return false;
      }

      ReturnKind combined = inferExprReturnKind(expr, params, locals);
      if (thenKind != ReturnKind::Unknown && elseKind != ReturnKind::Unknown && combined == ReturnKind::Unknown) {
        error_ = "if branches must return compatible types";
        return false;
      }
      return true;
    }
    if (isBlockCall(expr) && expr.hasBodyArguments) {
      const std::string resolved = resolveCalleePath(expr);
      if (defMap_.find(resolved) != defMap_.end()) {
        error_ = "block arguments require a definition target: " + resolved;
        return false;
      }
      if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
        error_ = "block expression does not accept arguments";
        return false;
      }
      if (expr.bodyArguments.empty()) {
        error_ = "block expression requires a value";
        return false;
      }
      std::unordered_map<std::string, BindingInfo> blockLocals = locals;
      bool sawReturn = false;
      for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
        const Expr &bodyExpr = expr.bodyArguments[i];
        const bool isLast = (i + 1 == expr.bodyArguments.size());
        if (bodyExpr.isBinding) {
          if (isLast && !sawReturn) {
            error_ = "block expression must end with an expression";
            return false;
          }
          if (isParam(params, bodyExpr.name) || blockLocals.count(bodyExpr.name) > 0) {
            error_ = "duplicate binding name: " + bodyExpr.name;
            return false;
          }
          BindingInfo info;
          std::optional<std::string> restrictType;
          if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
            return false;
          }
          if (bodyExpr.args.size() != 1) {
            error_ = "binding requires exactly one argument";
            return false;
          }
          if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
            return false;
          }
          ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
          if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
            error_ = "binding initializer requires a value";
            return false;
          }
          if (!hasExplicitBindingTypeTransform(bodyExpr)) {
            (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, blockLocals, info);
          }
          if (restrictType.has_value()) {
            const bool hasTemplate = !info.typeTemplateArg.empty();
            if (!restrictMatchesBinding(*restrictType,
                                        info.typeName,
                                        info.typeTemplateArg,
                                        hasTemplate,
                                        bodyExpr.namespacePrefix)) {
              error_ = "restrict type does not match binding type";
              return false;
            }
          }
          if (info.typeName == "Reference") {
            const Expr &init = bodyExpr.args.front();
            std::string pointerName;
            if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
                pointerName != "location" || init.args.size() != 1) {
              error_ = "Reference bindings require location(...)";
              return false;
            }
          }
          blockLocals.emplace(bodyExpr.name, info);
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            error_ = "return requires a value in block expression";
            return false;
          }
          if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
            return false;
          }
          ReturnKind kind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
          if (kind == ReturnKind::Void) {
            if (!isStructConstructorValueExpr(bodyExpr.args.front())) {
              error_ = "block expression requires a value";
              return false;
            }
          }
          sawReturn = true;
          continue;
        }
        if (!validateExpr(params, blockLocals, bodyExpr)) {
          return false;
        }
        if (isLast && !sawReturn) {
          ReturnKind kind = inferExprReturnKind(bodyExpr, params, blockLocals);
          if (kind == ReturnKind::Void) {
            if (!isStructConstructorValueExpr(bodyExpr)) {
              error_ = "block expression requires a value";
              return false;
            }
          }
        }
      }
      return true;
    }
    if (isBlockCall(expr)) {
      error_ = "block requires block arguments";
      return false;
    }
    if (isLoopCall(expr)) {
      error_ = "loop is only supported as a statement";
      return false;
    }
    if (isWhileCall(expr)) {
      error_ = "while is only supported as a statement";
      return false;
    }
    if (isForCall(expr)) {
      error_ = "for is only supported as a statement";
      return false;
    }
    if (isRepeatCall(expr)) {
      error_ = "repeat is only supported as a statement";
      return false;
    }
    auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      auto matchesHelper = [&](const char *helper) -> bool {
        if (isSimpleCallName(candidate, helper)) {
          return true;
        }
        if (candidate.name.empty()) {
          return false;
        }
        std::string normalized = candidate.name;
        if (!normalized.empty() && normalized[0] == '/') {
          normalized.erase(0, 1);
        }
        return normalized == std::string("vector/") + helper;
      };
      if (matchesHelper("push")) {
        nameOut = "push";
        return true;
      }
      if (matchesHelper("pop")) {
        nameOut = "pop";
        return true;
      }
      if (matchesHelper("reserve")) {
        nameOut = "reserve";
        return true;
      }
      if (matchesHelper("clear")) {
        nameOut = "clear";
        return true;
      }
      if (matchesHelper("remove_at")) {
        nameOut = "remove_at";
        return true;
      }
      if (matchesHelper("remove_swap")) {
        nameOut = "remove_swap";
        return true;
      }
      return false;
    };
    std::string vectorHelper;
    if (getVectorStatementHelperName(expr, vectorHelper)) {
      const std::string resolved = resolveCalleePath(expr);
      if (defMap_.find(resolved) == defMap_.end()) {
        error_ = vectorHelper + " is only supported as a statement";
        return false;
      }
    }
    if ((expr.hasBodyArguments || !expr.bodyArguments.empty()) && !isBlockCall(expr)) {
      const std::string resolved = resolveCalleePath(expr);
      if (defMap_.find(resolved) == defMap_.end()) {
        error_ = "block arguments require a definition target: " + resolved;
        return false;
      }
      std::unordered_map<std::string, BindingInfo> blockLocals = locals;
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (!validateStatement(params, blockLocals, bodyExpr, ReturnKind::Unknown, false, true, nullptr,
                               expr.namespacePrefix)) {
          return false;
        }
      }
    }
    if (isReturnCall(expr)) {
      error_ = "return not allowed in expression context";
      return false;
    }
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
              it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
            target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "vector" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName != "vector" || it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && collection == "vector" && target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveStringTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }

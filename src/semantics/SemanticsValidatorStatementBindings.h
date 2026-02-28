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
    if (stmt.args.empty()) {
      if (structNames_.count(currentDefinitionPath_) > 0) {
        if (restrictType.has_value()) {
          const bool hasTemplate = !info.typeTemplateArg.empty();
          if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
            error_ = "restrict type does not match binding type";
            return false;
          }
        }
        locals.emplace(stmt.name, info);
        return true;
      }
      if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
        return false;
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      locals.emplace(stmt.name, info);
      return true;
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
      if (isMatchCall(candidate)) {
        Expr expanded;
        std::string error;
        if (!lowerMatchToIf(candidate, expanded, error)) {
          return false;
        }
        return isStructConstructorValueExpr(expanded);
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
    auto referenceRootForBorrowBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
      if (binding.typeName == "Reference") {
        if (!binding.referenceRoot.empty()) {
          return binding.referenceRoot;
        }
        return bindingName;
      }
      return "";
    };
    auto pointerAliasRootForBinding = [&](const std::string &bindingName, const BindingInfo &binding) -> std::string {
      std::string referenceRoot = referenceRootForBorrowBinding(bindingName, binding);
      if (!referenceRoot.empty()) {
        return referenceRoot;
      }
      if (binding.typeName == "Pointer" && !binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return "";
    };
    auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        return paramBinding;
      }
      auto it = locals.find(name);
      if (it == locals.end()) {
        return nullptr;
      }
      return &it->second;
    };
    std::function<bool(const Expr &, std::string &)> resolvePointerRoot;
    resolvePointerRoot = [&](const Expr &expr, std::string &rootOut) -> bool {
      if (expr.kind == Expr::Kind::Name) {
        const BindingInfo *binding = resolveNamedBinding(expr.name);
        if (binding == nullptr) {
          return false;
        }
        rootOut = pointerAliasRootForBinding(expr.name, *binding);
        return !rootOut.empty();
      }
      if (expr.kind != Expr::Kind::Call) {
        return false;
      }
      std::string builtinName;
      if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name) {
          return false;
        }
        const BindingInfo *binding = resolveNamedBinding(target.name);
        if (binding != nullptr) {
          std::string root = pointerAliasRootForBinding(target.name, *binding);
          if (!root.empty()) {
            rootOut = std::move(root);
          } else {
            rootOut = target.name;
          }
          return true;
        }
        return false;
      }
      std::string opName;
      if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
        if (isPointerLikeExpr(expr.args[1], params, locals)) {
          return false;
        }
        return resolvePointerRoot(expr.args[0], rootOut);
      }
      return false;
    };
    if (info.typeName == "Pointer") {
      std::string pointerRoot;
      if (resolvePointerRoot(initializer, pointerRoot)) {
        info.referenceRoot = std::move(pointerRoot);
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (info.typeName == "Reference") {
      const Expr &init = initializer;
      auto formatBindingType = [](const BindingInfo &binding) -> std::string {
        if (binding.typeTemplateArg.empty()) {
          return binding.typeName;
        }
        return binding.typeName + "<" + binding.typeTemplateArg + ">";
      };
      std::function<bool(const Expr &, std::string &)> resolvePointerTargetType;
      resolvePointerTargetType = [&](const Expr &expr, std::string &targetOut) -> bool {
        if (expr.kind == Expr::Kind::Name) {
          const BindingInfo *binding = resolveNamedBinding(expr.name);
          if (binding == nullptr) {
            return false;
          }
          if ((binding->typeName == "Pointer" || binding->typeName == "Reference") &&
              !binding->typeTemplateArg.empty()) {
            targetOut = binding->typeTemplateArg;
            return true;
          }
          return false;
        }
        if (expr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string builtinName;
        if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
          const Expr &target = expr.args.front();
          if (target.kind != Expr::Kind::Name) {
            return false;
          }
          const BindingInfo *binding = resolveNamedBinding(target.name);
          if (binding == nullptr) {
            return false;
          }
          if (binding->typeName == "Reference" && !binding->typeTemplateArg.empty()) {
            targetOut = binding->typeTemplateArg;
          } else {
            targetOut = formatBindingType(*binding);
          }
          return true;
        }
        std::string opName;
        if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
          if (isPointerLikeExpr(expr.args[1], params, locals)) {
            return false;
          }
          return resolvePointerTargetType(expr.args[0], targetOut);
        }
        return false;
      };
      std::string pointerName;
      const bool initIsLocation =
          init.kind == Expr::Kind::Call && getBuiltinPointerName(init, pointerName) && pointerName == "location" &&
          init.args.size() == 1 && init.args.front().kind == Expr::Kind::Name;
      if (!initIsLocation && !currentDefinitionIsUnsafe_) {
        error_ = "Reference bindings require location(...)";
        return false;
      }
      if (initIsLocation) {
        std::string safeTargetType;
        if (!resolvePointerTargetType(init, safeTargetType) ||
            !errorTypesMatch(safeTargetType, info.typeTemplateArg, namespacePrefix)) {
          error_ = "Reference binding type mismatch";
          return false;
        }
      }
      if (!initIsLocation && currentDefinitionIsUnsafe_) {
        std::string pointerTargetType;
        if (!resolvePointerTargetType(init, pointerTargetType)) {
          error_ = "unsafe Reference bindings require pointer-like initializer";
          return false;
        }
        if (!errorTypesMatch(pointerTargetType, info.typeTemplateArg, namespacePrefix)) {
          error_ = "unsafe Reference binding type mismatch";
          return false;
        }
        std::string borrowRoot;
        if (resolvePointerRoot(init, borrowRoot)) {
          info.referenceRoot = std::move(borrowRoot);
        }
        info.isUnsafeReference = true;
        locals.emplace(stmt.name, info);
        return true;
      }
      const Expr &target = init.args.front();
      auto resolveBorrowRoot = [&](const std::string &targetName, std::string &rootOut) -> bool {
        if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
          if (paramBinding->typeName == "Reference") {
            rootOut = referenceRootForBorrowBinding(targetName, *paramBinding);
          } else {
            rootOut = targetName;
          }
          return true;
        }
        auto it = locals.find(targetName);
        if (it == locals.end()) {
          return false;
        }
        if (it->second.typeName == "Reference") {
          rootOut = referenceRootForBorrowBinding(it->first, it->second);
        } else {
          rootOut = targetName;
        }
        return true;
      };
      std::string borrowRoot;
      if (!resolveBorrowRoot(target.name, borrowRoot) || borrowRoot.empty()) {
        error_ = "Reference bindings require location(...)";
        return false;
      }
      bool sawMutableBorrow = false;
      bool sawImmutableBorrow = false;
      auto observeBorrow = [&](const std::string &bindingName, const BindingInfo &binding) {
        if (endedReferenceBorrows_.count(bindingName) > 0) {
          return;
        }
        const std::string root = referenceRootForBorrowBinding(bindingName, binding);
        if (root.empty() || root != borrowRoot) {
          return;
        }
        if (binding.isMutable) {
          sawMutableBorrow = true;
        } else {
          sawImmutableBorrow = true;
        }
      };
      for (const auto &param : params) {
        observeBorrow(param.name, param.binding);
      }
      for (const auto &entry : locals) {
        observeBorrow(entry.first, entry.second);
      }
      const bool conflict = info.isMutable ? (sawMutableBorrow || sawImmutableBorrow) : sawMutableBorrow;
      if (conflict && !currentDefinitionIsUnsafe_) {
        error_ = "borrow conflict: " + borrowRoot + " (root: " + borrowRoot + ", sink: " + stmt.name + ")";
        return false;
      }
      info.referenceRoot = std::move(borrowRoot);
      info.isUnsafeReference = currentDefinitionIsUnsafe_;
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
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      (isSimpleCallName(stmt, "init") || isSimpleCallName(stmt, "drop"))) {
    const std::string name = stmt.name;
    auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
      if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
        return "";
      }
      if (!typeName.empty() && typeName[0] == '/') {
        return structNames_.count(typeName) > 0 ? typeName : "";
      }
      std::string current = namespacePrefix;
      while (true) {
        if (!current.empty()) {
          std::string scoped = current + "/" + typeName;
          if (structNames_.count(scoped) > 0) {
            return scoped;
          }
          if (current.size() > typeName.size()) {
            const size_t start = current.size() - typeName.size();
            if (start > 0 && current[start - 1] == '/' &&
                current.compare(start, typeName.size(), typeName) == 0 &&
                structNames_.count(current) > 0) {
              return current;
            }
          }
        } else {
          std::string root = "/" + typeName;
          if (structNames_.count(root) > 0) {
            return root;
          }
        }
        if (current.empty()) {
          break;
        }
        const size_t slash = current.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
          current.clear();
        } else {
          current.erase(slash);
        }
      }
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
        return importIt->second;
      }
      return "";
    };
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = name + " does not accept block arguments";
      return false;
    }
    const size_t expectedArgs = (name == "init") ? 2 : 1;
    if (stmt.args.size() != expectedArgs) {
      error_ = name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
               (expectedArgs == 1 ? "" : "s");
      return false;
    }
    const Expr &storageArg = stmt.args.front();
    if (!validateExpr(params, locals, storageArg)) {
      return false;
    }
    BindingInfo storageBinding;
    bool resolved = false;
    if (!resolveUninitializedStorageBinding(params, locals, storageArg, storageBinding, resolved)) {
      return false;
    }
    if (!resolved || storageBinding.typeName != "uninitialized" || storageBinding.typeTemplateArg.empty()) {
      error_ = name + " requires uninitialized<T> storage";
      return false;
    }
    if (name == "init") {
      if (!validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(stmt.args[1], params, locals);
      if (valueKind == ReturnKind::Void) {
        error_ = "init requires a value";
        return false;
      }
      auto trimType = [](const std::string &text) -> std::string {
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
          ++start;
        }
        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
          --end;
        }
        return text.substr(start, end - start);
      };
      auto isBuiltinTemplateBase = [](const std::string &base) -> bool {
        return base == "array" || base == "vector" || base == "map" || base == "Result" || base == "Pointer" ||
               base == "Reference" || base == "Buffer" || base == "uninitialized";
      };
      std::function<bool(const std::string &, const std::string &)> typesMatch;
      typesMatch = [&](const std::string &expected, const std::string &actual) -> bool {
        std::string expectedTrim = trimType(expected);
        std::string actualTrim = trimType(actual);
        std::string expectedBase;
        std::string expectedArg;
        std::string actualBase;
        std::string actualArg;
        const bool expectedHasTemplate = splitTemplateTypeName(expectedTrim, expectedBase, expectedArg);
        const bool actualHasTemplate = splitTemplateTypeName(actualTrim, actualBase, actualArg);
        if (expectedHasTemplate != actualHasTemplate) {
          return false;
        }
        auto normalizeBase = [&](std::string base) -> std::string {
          base = trimType(base);
          if (!base.empty() && base[0] == '/') {
            base.erase(0, 1);
          }
          return base;
        };
        if (expectedHasTemplate) {
          std::string expectedNorm = normalizeBase(expectedBase);
          std::string actualNorm = normalizeBase(actualBase);
          if (isBuiltinTemplateBase(expectedNorm) || isBuiltinTemplateBase(actualNorm)) {
            if (expectedNorm != actualNorm) {
              return false;
            }
          } else if (!errorTypesMatch(expectedBase, actualBase, namespacePrefix)) {
            return false;
          }
          std::vector<std::string> expectedArgs;
          std::vector<std::string> actualArgs;
          if (!splitTopLevelTemplateArgs(expectedArg, expectedArgs) ||
              !splitTopLevelTemplateArgs(actualArg, actualArgs)) {
            return false;
          }
          if (expectedArgs.size() != actualArgs.size()) {
            return false;
          }
          for (size_t i = 0; i < expectedArgs.size(); ++i) {
            if (!typesMatch(expectedArgs[i], actualArgs[i])) {
              return false;
            }
          }
          return true;
        }
        return errorTypesMatch(expectedTrim, actualTrim, namespacePrefix);
      };
      auto inferValueTypeString = [&](const Expr &value, std::string &typeOut) -> bool {
        BindingInfo inferred;
        if (inferBindingTypeFromInitializer(value, params, locals, inferred)) {
          if (!inferred.typeTemplateArg.empty()) {
            typeOut = inferred.typeName + "<" + inferred.typeTemplateArg + ">";
            return true;
          }
          if (!inferred.typeName.empty() && inferred.typeName != "array") {
            typeOut = inferred.typeName;
            return true;
          }
        }
        if (inferExprReturnKind(value, params, locals) == ReturnKind::Array) {
          std::string structPath = inferStructReturnPath(value, params, locals);
          if (!structPath.empty()) {
            typeOut = structPath;
            return true;
          }
        }
        return false;
      };
      const std::string expectedType = storageBinding.typeTemplateArg;
      std::string actualType;
      if (inferValueTypeString(stmt.args[1], actualType)) {
        if (!typesMatch(expectedType, actualType)) {
          error_ = "init value type mismatch";
          return false;
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind == ReturnKind::Array) {
        if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
          error_ = "init value type mismatch";
          return false;
        }
      } else if (expectedKind != ReturnKind::Unknown) {
        if (valueKind != ReturnKind::Unknown && valueKind != expectedKind) {
          error_ = "init value type mismatch";
          return false;
        }
      } else {
        std::string expectedStruct = resolveStructTypePath(expectedType, namespacePrefix);
        if (!expectedStruct.empty()) {
          if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
            error_ = "init value type mismatch";
            return false;
          }
          if (valueKind == ReturnKind::Array) {
            std::string actualStruct = inferStructReturnPath(stmt.args[1], params, locals);
            if (!actualStruct.empty() && actualStruct != expectedStruct) {
              error_ = "init value type mismatch";
              return false;
            }
          }
        }
      }
    }
    return true;
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      (isSimpleCallName(stmt, "take") || isSimpleCallName(stmt, "borrow"))) {
    const std::string name = stmt.name;
    auto isUninitializedStorage = [&](const Expr &arg) -> bool {
      if (arg.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = findBinding(params, locals, arg.name);
      return binding && binding->typeName == "uninitialized" && !binding->typeTemplateArg.empty();
    };
    const bool treatAsUninitializedHelper =
        (name != "take") || (!stmt.args.empty() && isUninitializedStorage(stmt.args.front()));
    if (treatAsUninitializedHelper) {
      if (hasNamedArguments(stmt.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error_ = name + " does not accept template arguments";
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error_ = name + " does not accept block arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error_ = name + " requires exactly 1 argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (!isUninitializedStorage(stmt.args.front())) {
        error_ = name + " requires uninitialized<T> storage";
        return false;
      }
      return true;
    }
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
      auto declaredReferenceReturnTarget = [&]() -> std::optional<std::string> {
        auto defIt = defMap_.find(currentDefinitionPath_);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          return std::nullopt;
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "Reference") {
            continue;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return std::nullopt;
          }
          return args.front();
        }
        return std::nullopt;
      };
      if (std::optional<std::string> expectedReferenceTarget = declaredReferenceReturnTarget();
          expectedReferenceTarget.has_value()) {
        const Expr &returnExpr = stmt.args.front();
        if (returnExpr.kind != Expr::Kind::Name) {
          error_ = "reference return requires direct parameter reference";
          return false;
        }
        const BindingInfo *paramBinding = findParamBinding(params, returnExpr.name);
        if (paramBinding == nullptr || paramBinding->typeName != "Reference") {
          error_ = "reference return requires direct parameter reference";
          return false;
        }
        auto normalizeReferenceTarget = [&](const std::string &target) -> std::string {
          std::string trimmed = target;
          size_t start = 0;
          while (start < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[start])) != 0) {
            ++start;
          }
          size_t end = trimmed.size();
          while (end > start && std::isspace(static_cast<unsigned char>(trimmed[end - 1])) != 0) {
            --end;
          }
          trimmed = trimmed.substr(start, end - start);
          return normalizeBindingTypeName(trimmed);
        };
        if (normalizeReferenceTarget(paramBinding->typeTemplateArg) !=
            normalizeReferenceTarget(*expectedReferenceTarget)) {
          error_ = "reference return type mismatch";
          return false;
        }
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
  if (isMatchCall(stmt)) {
    Expr expanded;
    if (!lowerMatchToIf(stmt, expanded, error_)) {
      return false;
    }
    return validateStatement(params,
                             locals,
                             expanded,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             enclosingStatements,
                             statementIndex);
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
    std::vector<Expr> postMergeStatements;
    if (enclosingStatements != nullptr) {
      size_t start = statementIndex + 1;
      if (start > enclosingStatements->size()) {
        start = enclosingStatements->size();
      }
      postMergeStatements.insert(postMergeStatements.end(), enclosingStatements->begin() + start, enclosingStatements->end());
    }
    auto validateBranch = [&](const Expr &branch) -> bool {
      if (!isIfBlockEnvelope(branch)) {
        error_ = "if branches require block envelopes";
        return false;
      }
      std::unordered_map<std::string, BindingInfo> branchLocals = locals;
      std::vector<Expr> livenessStatements = branch.bodyArguments;
      livenessStatements.insert(livenessStatements.end(), postMergeStatements.begin(), postMergeStatements.end());
      OnErrorScope onErrorScope(*this, std::nullopt);
      BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
      for (size_t bodyIndex = 0; bodyIndex < branch.bodyArguments.size(); ++bodyIndex) {
        const Expr &bodyExpr = branch.bodyArguments[bodyIndex];
        if (!validateStatement(params,
                               branchLocals,
                               bodyExpr,
                               returnKind,
                               allowReturn,
                               allowBindings,
                               sawReturn,
                               namespacePrefix,
                               &branch.bodyArguments,
                               bodyIndex)) {
          return false;
        }
        expireReferenceBorrowsForRemainder(params, branchLocals, livenessStatements, bodyIndex + 1);
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

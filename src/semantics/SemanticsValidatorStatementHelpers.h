bool SemanticsValidator::validateStatement(const std::vector<ParameterInfo> &params,
                                           std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &stmt,
                                           ReturnKind returnKind,
                                           bool allowReturn,
                                           bool allowBindings,
                                           bool *sawReturn,
                                           const std::string &namespacePrefix) {
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
  auto isStringExpr = [&](const Expr &arg,
                          const std::vector<ParameterInfo> &paramsIn,
                          const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        auto resolveReference = [&](const BindingInfo &binding) -> bool {
          if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
            return false;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
            return false;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        };
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          if (resolveReference(*paramBinding)) {
            return true;
          }
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemTypeOut = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end()) {
          return false;
        }
        if (resolveReference(it->second)) {
          return true;
        }
        if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
            it->second.typeTemplateArg.empty()) {
          return false;
        }
        elemTypeOut = it->second.typeTemplateArg;
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
            target.templateArgs.size() == 1) {
          elemTypeOut = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          valueTypeOut = parts[1];
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        valueTypeOut = parts[1];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
          return false;
        }
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      return false;
    };
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = localsIn.find(arg.name);
      return it != localsIn.end() && it->second.typeName == "string";
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName) && arg.args.size() == 2) {
        std::string elemType;
        if (resolveArrayTarget(arg.args.front(), elemType) && normalizeBindingTypeName(elemType) == "string") {
          return true;
        }
        std::string mapValueType;
        if (resolveMapValueType(arg.args.front(), mapValueType) &&
            normalizeBindingTypeName(mapValueType) == "string") {
          return true;
        }
      }
    }
    return false;
  };
  auto isPrintableExpr = [&](const Expr &arg,
                             const std::vector<ParameterInfo> &paramsIn,
                             const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    if (isStringExpr(arg, paramsIn, localsIn)) {
      return true;
    }
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
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
    if (isPointerLikeExpr(arg, paramsIn, localsIn)) {
      return false;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string builtinName;
      if (getBuiltinCollectionName(arg, builtinName)) {
        return false;
      }
    }
    return false;
  };
  auto isIntegerOrBoolExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Void ||
        kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Bool;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto resolveVectorBinding = [&](const Expr &target, const BindingInfo *&bindingOut) -> bool {
    bindingOut = nullptr;
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      bindingOut = paramBinding;
      return true;
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      bindingOut = &it->second;
      return true;
    }
    return false;
  };
  auto validateVectorElementType = [&](const Expr &arg, const std::string &typeName) -> bool {
    if (typeName.empty()) {
      error_ = "push requires vector element type";
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (normalizedType == "string") {
      if (!isStringExpr(arg, params, locals)) {
        error_ = "push requires element type " + typeName;
        return false;
      }
      return true;
    }
    ReturnKind expectedKind = returnKindForTypeName(normalizedType);
    if (expectedKind == ReturnKind::Unknown) {
      return true;
    }
    if (isStringExpr(arg, params, locals)) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    ReturnKind argKind = inferExprReturnKind(arg, params, locals);
    if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    return true;
  };
  auto validateVectorHelperTarget = [&](const Expr &target, const char *helperName, const BindingInfo *&bindingOut) -> bool {
    if (!resolveVectorBinding(target, bindingOut) || bindingOut == nullptr || bindingOut->typeName != "vector") {
      error_ = std::string(helperName) + " requires vector binding";
      return false;
    }
    if (!bindingOut->isMutable) {
      error_ = std::string(helperName) + " requires mutable vector binding";
      return false;
    }
    return true;
  };
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

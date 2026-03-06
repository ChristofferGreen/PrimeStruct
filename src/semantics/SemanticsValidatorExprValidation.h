      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string builtinName;
        if (defMap_.find(resolveCalleePath(target)) == defMap_.end() && getBuiltinArrayAccessName(target, builtinName) &&
            target.args.size() == 2) {
          std::string elemType;
          if (resolveArrayTarget(target.args.front(), elemType) && elemType == "string") {
            return true;
          }
        }
      }
      return false;
    };
    auto resolveMapTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "map";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "map";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
          return false;
        }
        return getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2;
      }
      return false;
    };
    auto resolveMapKeyType = [&](const Expr &target, std::string &keyTypeOut) -> bool {
      keyTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          keyTypeOut = parts[0];
          return true;
        }
        auto it = locals.find(target.name);
        if (it == locals.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        keyTypeOut = parts[0];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
          return false;
        }
        if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
          return false;
        }
        keyTypeOut = target.templateArgs[0];
        return true;
      }
      return false;
    };
    auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
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
        auto it = locals.find(target.name);
        if (it == locals.end() || it->second.typeName != "map") {
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
        if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
          return false;
        }
        if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
          return false;
        }
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      return false;
    };
    auto isStringExpr = [&](const Expr &arg) -> bool {
      if (resolveStringTarget(arg)) {
        return true;
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string accessName;
        if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinArrayAccessName(arg, accessName) &&
            arg.args.size() == 2) {
          std::string mapValueType;
          if (resolveMapValueType(arg.args.front(), mapValueType) && normalizeBindingTypeName(mapValueType) == "string") {
            return true;
          }
        }
      }
      return false;
    };
    auto validateCollectionElementType = [&](const Expr &arg, const std::string &typeName,
                                              const std::string &errorPrefix) -> bool {
      const std::string normalizedType = normalizeBindingTypeName(typeName);
      if (normalizedType == "string") {
        if (!isStringExpr(arg)) {
          error_ = errorPrefix + typeName;
          return false;
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(normalizedType);
      if (expectedKind == ReturnKind::Unknown) {
        return true;
      }
      if (isStringExpr(arg)) {
        error_ = errorPrefix + typeName;
        return false;
      }
      ReturnKind argKind = inferExprReturnKind(arg, params, locals);
      if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
        error_ = errorPrefix + typeName;
        return false;
      }
      return true;
    };
    auto isStaticHelperDefinition = [&](const Definition &def) -> bool {
      for (const auto &transform : def.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto resolveMethodTarget =
        [&](const Expr &receiver, const std::string &methodName, std::string &resolvedOut, bool &isBuiltinOut) -> bool {
      isBuiltinOut = false;
      auto resolveStructTypePath = [&](const std::string &typeName,
                                       const std::string &namespacePrefix) -> std::string {
        if (typeName.empty()) {
          return "";
        }
        if (!typeName.empty() && typeName[0] == '/') {
          return typeName;
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
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        return "";
      };
      if (methodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/ok";
        isBuiltinOut = true;
        return true;
      }
      if (methodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/error";
        isBuiltinOut = true;
        return true;
      }
      if (methodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/why";
        isBuiltinOut = true;
        return true;
      }
      if ((methodName == "map" || methodName == "and_then" || methodName == "map2") &&
          receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/" + methodName;
        isBuiltinOut = true;
        return true;
      }
      std::string elemType;
      if (methodName == "count") {
        if (resolveVectorTarget(receiver, elemType)) {
          resolvedOut = "/vector/count";
          isBuiltinOut = true;
          return true;
        }
        if (resolveArrayTarget(receiver, elemType)) {
          resolvedOut = "/array/count";
          isBuiltinOut = true;
          return true;
        }
        if (resolveStringTarget(receiver)) {
          resolvedOut = "/string/count";
          isBuiltinOut = true;
          return true;
        }
        if (resolveMapTarget(receiver)) {
          resolvedOut = "/map/count";
          isBuiltinOut = true;
          return true;
        }
      }
      if (methodName == "capacity") {
        if (resolveVectorTarget(receiver, elemType)) {
          resolvedOut = "/vector/capacity";
          isBuiltinOut = true;
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        const std::string resolvedType = resolveCalleePath(receiver);
        if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
          resolvedOut = resolvedType + "/" + methodName;
          return true;
        }
      }
      std::string typeName;
      std::string typeTemplateArg;
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
          typeTemplateArg = paramBinding->typeTemplateArg;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
            typeTemplateArg = it->second.typeTemplateArg;
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
      if (typeName == "File") {
        if (methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
            methodName == "write_bytes" || methodName == "flush" || methodName == "close") {
          resolvedOut = "/file/" + methodName;
          isBuiltinOut = true;
          return true;
        }
      }
      if (typeName == "FileError" && methodName == "why") {
        resolvedOut = "/file_error/why";
        isBuiltinOut = true;
        return true;
      }
      if (typeName.empty()) {
        error_ = "unknown method target for " + methodName;
        return false;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        error_ = "unknown method target for " + methodName;
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + methodName;
        return true;
      }
      std::string resolvedType = resolveStructTypePath(typeName, receiver.namespacePrefix);
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
      }
      resolvedOut = resolvedType + "/" + methodName;
      return true;
    };
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
    auto isConvertibleExpr = [&](const Expr &arg) -> bool {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
          kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool ||
          kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
        return true;
      }
      if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
        return false;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(arg, collection)) {
          return false;
        }
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 || paramKind == ReturnKind::Bool ||
                 paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal || paramKind == ReturnKind::Complex;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 || localKind == ReturnKind::Bool ||
                 localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal || localKind == ReturnKind::Complex;
        }
      }
      return true;
    };

    auto inferStructFieldBinding = [&](const Definition &def,
                                       const std::string &fieldName,
                                       BindingInfo &bindingOut,
                                       bool allowStatic,
                                       bool allowPrivate) -> bool {
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      auto isPrivateField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "private") {
            return true;
          }
        }
        return false;
      };
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + def.fullPath;
          return false;
        }
        if (stmt.name != fieldName) {
          continue;
        }
        if (isStaticField(stmt) && !allowStatic) {
          return false;
        }
        if (isPrivateField(stmt) && !allowPrivate) {
          error_ = "private field is not accessible: " + def.fullPath + "/" + fieldName;
          return false;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(def, stmt, fieldBinding)) {
          return false;
        }
        bindingOut = std::move(fieldBinding);
        return true;
      }
      return false;
    };

    auto resolveStructFieldBinding =
        [&](const Expr &receiver, const std::string &fieldName, BindingInfo &bindingOut) -> bool {
      auto resolveStructPathFromType = [&](const std::string &typeName,
                                           const std::string &namespacePrefix,
                                           std::string &structPathOut) -> bool {
        if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
          return false;
        }
        if (!typeName.empty() && typeName[0] == '/') {
          if (structNames_.count(typeName) > 0) {
            structPathOut = typeName;
            return true;
          }
          return false;
        }
        std::string current = namespacePrefix;
        while (true) {
          if (!current.empty()) {
            std::string scoped = current + "/" + typeName;
            if (structNames_.count(scoped) > 0) {
              structPathOut = scoped;
              return true;
            }
            if (current.size() > typeName.size()) {
              const size_t start = current.size() - typeName.size();
              if (start > 0 && current[start - 1] == '/' &&
                  current.compare(start, typeName.size(), typeName) == 0 &&
                  structNames_.count(current) > 0) {
                structPathOut = current;
                return true;
              }
            }
          } else {
            std::string root = "/" + typeName;
            if (structNames_.count(root) > 0) {
              structPathOut = root;
              return true;
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
          structPathOut = importIt->second;
          return true;
        }
        return false;
      };
      std::string structPath;
      if (receiver.kind == Expr::Kind::Name) {
        const BindingInfo *binding = findParamBinding(params, receiver.name);
        if (!binding) {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            binding = &it->second;
          }
        }
        if (binding) {
          std::string typeName = binding->typeName;
          if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
            typeName = binding->typeTemplateArg;
          }
          (void)resolveStructPathFromType(typeName, receiver.namespacePrefix, structPath);
        }
      } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        std::string resolvedType = resolveCalleePath(receiver);
        if (structNames_.count(resolvedType) > 0) {
          structPath = resolvedType;
        }
      }
      if (structPath.empty()) {
        return false;
      }
      auto defIt = defMap_.find(structPath);
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferred;
      if (!inferStructFieldBinding(*defIt->second, fieldName, inferred, false, true)) {
        return false;
      }
      bindingOut = std::move(inferred);
      return true;
    };

    std::string resolved = resolveCalleePath(expr);
    bool resolvedMethod = false;
    bool usedMethodTarget = false;
    if (expr.isFieldAccess) {
      if (expr.args.size() != 1) {
        error_ = "field access requires a receiver";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "field access does not accept template arguments";
        return false;
      }
      if (hasNamedArguments(expr.argNames)) {
        error_ = "field access does not accept named arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "field access does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(expr.args.front(), expr.name, fieldBinding)) {
        if (error_.empty()) {
          error_ = "unknown field: " + expr.name;
        }
        return false;
      }
      return true;
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        error_ = "method call missing receiver";
        return false;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      if (!resolveMethodTarget(expr.args.front(), expr.name, resolved, isBuiltinMethod)) {
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end()) {
        error_ = "unknown method: " + resolved;
        return false;
      }
      resolvedMethod = isBuiltinMethod;
    } else if (expr.name == "count" && expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(expr.args.front(), "count", methodResolved, isBuiltinMethod)) {
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    }
    if (usedMethodTarget && !resolvedMethod) {
      auto defIt = defMap_.find(resolved);
      if (defIt != defMap_.end() && isStaticHelperDefinition(*defIt->second)) {
        error_ = "static helper does not accept method-call syntax: " + resolved;
        return false;
      }
    }
      if ((expr.hasBodyArguments || !expr.bodyArguments.empty()) && !isBuiltinBlockCall(expr)) {
        if (resolvedMethod || defMap_.find(resolved) == defMap_.end()) {
          error_ = "block arguments require a definition target: " + resolved;
          return false;
        }
        std::unordered_map<std::string, BindingInfo> blockLocals = locals;
        std::vector<Expr> livenessStatements = expr.bodyArguments;
        if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
          for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
            livenessStatements.push_back((*enclosingStatements)[idx]);
          }
        }
        OnErrorScope onErrorScope(*this, std::nullopt);
        BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
        for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size(); ++bodyIndex) {
          const Expr &bodyExpr = expr.bodyArguments[bodyIndex];
          if (!validateStatement(params, blockLocals, bodyExpr, ReturnKind::Unknown, false, true, nullptr,
                                 expr.namespacePrefix, &expr.bodyArguments, bodyIndex)) {
            return false;
          }
          expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
        }
      }
    std::string gpuBuiltin;
    if (getBuiltinGpuName(expr, gpuBuiltin)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "gpu builtins do not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "gpu builtins do not accept block arguments";
        return false;
      }
      if (!expr.args.empty()) {
        error_ = "gpu builtins do not accept arguments";
        return false;
      }
      if (!currentDefinitionIsCompute_) {
        error_ = "gpu builtins require a compute definition";
        return false;
      }
      return true;
    }
    PathSpaceBuiltin pathSpaceBuiltin;
    if (getPathSpaceBuiltin(expr, pathSpaceBuiltin) && defMap_.find(resolved) == defMap_.end()) {
      error_ = pathSpaceBuiltin.name + " is statement-only";
      return false;
    }

    if (!validateNamedArguments(expr.args, expr.argNames, resolved, error_)) {
      return false;
    }
    std::function<bool(const Expr &)> isUnsafeReferenceExpr;
    isUnsafeReferenceExpr = [&](const Expr &argExpr) -> bool {
        if (argExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, argExpr.name)) {
            return paramBinding->typeName == "Reference" && paramBinding->isUnsafeReference;
        }
        auto itLocal = locals.find(argExpr.name);
        return itLocal != locals.end() && itLocal->second.typeName == "Reference" && itLocal->second.isUnsafeReference;
        }
        if (argExpr.kind != Expr::Kind::Call || argExpr.isBinding) {
          return false;
        }
        auto hasUnsafeChildExpr = [&](const Expr &callExpr) -> bool {
          for (const auto &nestedArg : callExpr.args) {
            if (isUnsafeReferenceExpr(nestedArg)) {
              return true;
            }
          }
          for (const auto &bodyExpr : callExpr.bodyArguments) {
            if (isUnsafeReferenceExpr(bodyExpr)) {
              return true;
            }
          }
          return false;
        };
        if (isIfCall(argExpr) || isMatchCall(argExpr) || isBlockCall(argExpr) || isReturnCall(argExpr) ||
            isSimpleCallName(argExpr, "then") || isSimpleCallName(argExpr, "else") ||
            isSimpleCallName(argExpr, "case")) {
          return hasUnsafeChildExpr(argExpr);
        }
        const std::string nestedResolved = resolveCalleePath(argExpr);
        if (nestedResolved.empty()) {
          return false;
        }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      bool returnsReference = false;
      for (const auto &transform : nestedIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) && base == "Reference") {
          returnsReference = true;
          break;
        }
      }
      if (!returnsReference) {
        return false;
      }
      const auto &nestedParams = paramsByDef_[nestedResolved];
      if (nestedParams.empty()) {
        return false;
      }
      std::string nestedArgError;
      if (!validateNamedArgumentsAgainstParams(nestedParams, argExpr.argNames, nestedArgError)) {
        return false;
      }
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, argExpr.args, argExpr.argNames, nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0; nestedIndex < nestedOrderedArgs.size() && nestedIndex < nestedParams.size();
           ++nestedIndex) {
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParams[nestedIndex].binding.typeName != "Reference") {
          continue;
        }
        if (isUnsafeReferenceExpr(*nestedArg)) {
          return true;
        }
      }
      return false;
    };
    std::function<bool(const Expr &, std::string &)> resolveEscapingReferenceRoot;
    resolveEscapingReferenceRoot = [&](const Expr &argExpr, std::string &rootOut) -> bool {
      rootOut.clear();
      if (argExpr.kind == Expr::Kind::Name) {
        if (findParamBinding(params, argExpr.name) != nullptr) {
          return false;
        }
        auto itLocal = locals.find(argExpr.name);
        if (itLocal == locals.end() || itLocal->second.typeName != "Reference") {
          return false;
        }
        std::string sourceRoot = itLocal->second.referenceRoot.empty() ? argExpr.name : itLocal->second.referenceRoot;
        if (const BindingInfo *rootParam = findParamBinding(params, sourceRoot)) {
          if (rootParam->typeName == "Reference") {
            return false;
          }
        }
        rootOut = sourceRoot;
        return true;
      }
      if (argExpr.kind != Expr::Kind::Call || argExpr.isBinding) {
        return false;
      }
      auto resolveChildRoot = [&](const Expr &callExpr) -> bool {
        for (const auto &nestedArg : callExpr.args) {
          if (resolveEscapingReferenceRoot(nestedArg, rootOut)) {
            return true;
          }
        }
        for (const auto &bodyExpr : callExpr.bodyArguments) {
          if (resolveEscapingReferenceRoot(bodyExpr, rootOut)) {
            return true;
          }
        }
        return false;
      };
      if (isIfCall(argExpr) || isMatchCall(argExpr) || isBlockCall(argExpr) || isReturnCall(argExpr) ||
          isSimpleCallName(argExpr, "then") || isSimpleCallName(argExpr, "else") ||
          isSimpleCallName(argExpr, "case")) {
        return resolveChildRoot(argExpr);
      }
      const std::string nestedResolved = resolveCalleePath(argExpr);
      if (nestedResolved.empty()) {
        return false;
      }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      bool returnsReference = false;
      for (const auto &transform : nestedIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) && base == "Reference") {
          returnsReference = true;
          break;
        }
      }
      if (!returnsReference) {
        return false;
      }
      const auto &nestedParams = paramsByDef_[nestedResolved];
      if (nestedParams.empty()) {
        return false;
      }
      std::string nestedArgError;
      if (!validateNamedArgumentsAgainstParams(nestedParams, argExpr.argNames, nestedArgError)) {
        return false;
      }
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, argExpr.args, argExpr.argNames, nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0; nestedIndex < nestedOrderedArgs.size() && nestedIndex < nestedParams.size();
           ++nestedIndex) {
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParams[nestedIndex].binding.typeName != "Reference") {
          continue;
        }
        if (resolveEscapingReferenceRoot(*nestedArg, rootOut)) {
          return true;
        }
      }
      return false;
    };
    auto reportReferenceAssignmentEscape = [&](const std::string &sinkName, const Expr &rhsExpr) -> bool {
      std::string sourceRoot;
      if (!resolveEscapingReferenceRoot(rhsExpr, sourceRoot)) {
        return false;
      }
      if (sourceRoot.empty()) {
        sourceRoot = "<unknown>";
      }
      const std::string sink = sinkName.empty() ? "<unknown>" : sinkName;
      if (currentDefinitionIsUnsafe_ && isUnsafeReferenceExpr(rhsExpr)) {
        error_ = "unsafe reference escapes via assignment to " + sink +
                 " (root: " + sourceRoot + ", sink: " + sink + ")";
      } else {
        error_ = "reference escapes via assignment to " + sink +
                 " (root: " + sourceRoot + ", sink: " + sink + ")";
      }
      return true;
    };
    auto it = defMap_.find(resolved);
    if (it == defMap_.end() || resolvedMethod) {
      if (!expr.isMethodCall && isSimpleCallName(expr, "try")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "try does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "try does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "try requires exactly one argument";
          return false;
        }
        if (!currentOnError_.has_value()) {
          error_ = "missing on_error for ? usage";
          return false;
        }
        if (!currentResultType_.has_value() || !currentResultType_->isResult) {
          error_ = "try requires Result return type";
          return false;
        }
        if (!errorTypesMatch(currentResultType_->errorType, currentOnError_->errorType, expr.namespacePrefix)) {
          error_ = "on_error error type mismatch";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) || !argResult.isResult) {
          error_ = "try requires Result argument";
          return false;
        }
        if (!errorTypesMatch(argResult.errorType, currentOnError_->errorType, expr.namespacePrefix)) {
          error_ = "try error type mismatch";
          return false;
        }
        return true;
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "File")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (expr.templateArgs.size() != 1) {
          error_ = "File requires exactly one template argument";
          return false;
        }
        const std::string &mode = expr.templateArgs.front();
        if (mode != "Read" && mode != "Write" && mode != "Append") {
          error_ = "File requires Read, Write, or Append mode";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "File requires exactly one path argument";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "File does not accept block arguments";
          return false;
        }
        if (activeEffects_.count("file_write") == 0) {
          error_ = "File requires file_write effect";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        if (!isStringExpr(expr.args.front())) {
          error_ = "File requires string path argument";
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/ok") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.ok does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.ok does not accept block arguments";
          return false;
        }
        if (expr.args.size() > 2) {
          error_ = "Result.ok accepts at most one value argument";
          return false;
        }
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (!validateExpr(params, locals, expr.args[i])) {
            return false;
          }
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/error") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.error does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.error does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "Result.error requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
          error_ = "Result.error requires Result argument";
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/why") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.why does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.why does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "Result.why requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
          error_ = "Result.why requires Result argument";
          return false;
        }
        return true;
      }
      if (resolvedMethod && (resolved == "/result/map" || resolved == "/result/and_then")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result." + expr.name + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result." + expr.name + " does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 3) {
          error_ = "Result." + expr.name + " requires exactly two arguments";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
          error_ = "Result." + expr.name + " requires Result argument";
          return false;
        }
        if (!argResult.hasValue) {
          error_ = "Result." + expr.name + " requires value Result";
          return false;
        }
        if (!expr.args[2].isLambda) {
          error_ = "Result." + expr.name + " requires a lambda argument";
          return false;
        }
        if (expr.args[2].args.size() != 1) {
          error_ = "Result." + expr.name + " requires a single-parameter lambda";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[2])) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/map2") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.map2 does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.map2 does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 4) {
          error_ = "Result.map2 requires exactly three arguments";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1]) || !validateExpr(params, locals, expr.args[2])) {
          return false;
        }
        ResultTypeInfo leftResult;
        ResultTypeInfo rightResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, leftResult) || !leftResult.isResult ||
            !resolveResultTypeForExpr(expr.args[2], params, locals, rightResult) || !rightResult.isResult) {
          error_ = "Result.map2 requires Result arguments";
          return false;
        }
        if (!leftResult.hasValue || !rightResult.hasValue) {
          error_ = "Result.map2 requires value Results";
          return false;
        }
        if (!errorTypesMatch(leftResult.errorType, rightResult.errorType, expr.namespacePrefix)) {
          error_ = "Result.map2 requires matching error types";
          return false;
        }
        if (!expr.args[3].isLambda) {
          error_ = "Result.map2 requires a lambda argument";
          return false;
        }
        if (expr.args[3].args.size() != 2) {
          error_ = "Result.map2 requires a two-parameter lambda";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[3])) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/file_error/why") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "why does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "why does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "why does not accept arguments";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved.rfind("/file/", 0) == 0) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "file methods do not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "file methods do not accept block arguments";
          return false;
        }
        if (expr.args.empty()) {
          error_ = "file method missing receiver";
          return false;
        }
        if (activeEffects_.count("file_write") == 0) {
          error_ = "file operations require file_write effect";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        auto isFilePrintableExpr = [&](const Expr &arg) -> bool {
          if (isStringExpr(arg)) {
            return true;
          }
          ReturnKind kind = inferExprReturnKind(arg, params, locals);
          if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
              kind == ReturnKind::Bool) {
            return true;
          }
          if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
            return false;
          }
          if (arg.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
              ReturnKind paramKind = returnKindForBinding(*paramBinding);
              return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                     paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Bool;
            }
            auto itLocal = locals.find(arg.name);
            if (itLocal != locals.end()) {
              ReturnKind localKind = returnKindForBinding(itLocal->second);
              return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                     localKind == ReturnKind::UInt64 || localKind == ReturnKind::Bool;
            }
          }
          if (isPointerLikeExpr(arg, params, locals)) {
            return false;
          }
          if (arg.kind == Expr::Kind::Call) {
            std::string builtinName;
            if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinCollectionName(arg, builtinName)) {
              return false;
            }
          }
          return false;
        };
        auto isIntegerOrBoolExpr = [&](const Expr &arg) -> bool {
          ReturnKind kind = inferExprReturnKind(arg, params, locals);
          if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
              kind == ReturnKind::Bool) {
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
                return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                       paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Bool;
              }
              auto itLocal = locals.find(arg.name);
              if (itLocal != locals.end()) {
                ReturnKind localKind = returnKindForBinding(itLocal->second);
                return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                       localKind == ReturnKind::UInt64 || localKind == ReturnKind::Bool;
              }
            }
          }
          return false;
        };
        if (expr.name == "write" || expr.name == "write_line") {
          for (size_t i = 1; i < expr.args.size(); ++i) {
            if (!validateExpr(params, locals, expr.args[i])) {
              return false;
            }
            if (!isFilePrintableExpr(expr.args[i])) {
              error_ = "file write requires integer/bool or string arguments";
              return false;
            }
          }
          return true;
        }
        if (expr.name == "write_byte") {
          if (expr.args.size() != 2) {
            error_ = "write_byte requires exactly one argument";
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          if (!isIntegerOrBoolExpr(expr.args[1])) {
            error_ = "write_byte requires integer argument";
            return false;
          }
          return true;
        }
        if (expr.name == "write_bytes") {
          if (expr.args.size() != 2) {
            error_ = "write_bytes requires exactly one argument";
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          bool ok = false;
          if (expr.args[1].kind == Expr::Kind::Call) {
            std::string collection;
            if (defMap_.find(resolveCalleePath(expr.args[1])) == defMap_.end() &&
                getBuiltinCollectionName(expr.args[1], collection) && collection == "array") {
              ok = true;
            }
          }
          if (expr.args[1].kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, expr.args[1].name)) {
              ok = (paramBinding->typeName == "array");
            } else {
              auto itLocal = locals.find(expr.args[1].name);
              ok = (itLocal != locals.end() && itLocal->second.typeName == "array");
            }
          }
          if (!ok) {
            error_ = "write_bytes requires array argument";
            return false;
          }
          return true;
        }
        if (expr.name == "flush" || expr.name == "close") {
          if (expr.args.size() != 1) {
            error_ = expr.name + " does not accept arguments";
            return false;
          }
          return true;
        }
      }
      if (hasNamedArguments(expr.argNames)) {
        std::string builtinName;
        auto isLegacyCollectionBuiltinCall = [&]() {
          std::string collectionName;
          if (!getBuiltinCollectionName(expr, collectionName)) {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyArrayAccessBuiltinCall = [&]() {
          std::string arrayAccessName;
          if (!getBuiltinArrayAccessName(expr, arrayAccessName)) {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyCountBuiltinCall = [&]() {
          if (expr.name != "count") {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyCapacityBuiltinCall = [&]() {
          if (expr.name != "capacity") {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyVectorHelperBuiltinCall = [&]() {
          if (!(isSimpleCallName(expr, "push") || isSimpleCallName(expr, "pop") ||
                isSimpleCallName(expr, "reserve") || isSimpleCallName(expr, "clear") ||
                isSimpleCallName(expr, "remove_at") || isSimpleCallName(expr, "remove_swap"))) {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        bool isBuiltin = false;
        if (getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
            getBuiltinMutationName(expr, builtinName) ||
            getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinGpuName(expr, builtinName) ||
            getBuiltinPointerName(expr, builtinName) || getBuiltinConvertName(expr, builtinName) ||
            isLegacyCollectionBuiltinCall() || isLegacyArrayAccessBuiltinCall() ||
            isAssignCall(expr) || isIfCall(expr) || isMatchCall(expr) || isLoopCall(expr) || isWhileCall(expr) ||
            isForCall(expr) ||
            isRepeatCall(expr) || isLegacyCountBuiltinCall() || expr.name == "File" || expr.name == "try" ||
            isLegacyCapacityBuiltinCall() || isLegacyVectorHelperBuiltinCall() ||
            isSimpleCallName(expr, "dispatch") || isSimpleCallName(expr, "buffer") ||
            isSimpleCallName(expr, "upload") || isSimpleCallName(expr, "readback") ||
            isSimpleCallName(expr, "buffer_load") || isSimpleCallName(expr, "buffer_store")) {
          isBuiltin = true;
        }
      if (isBuiltin) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
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
    if (isSimpleCallName(expr, "dispatch")) {
      if (currentDefinitionIsCompute_) {
        error_ = "dispatch is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "dispatch requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "dispatch does not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "dispatch does not accept block arguments";
        return false;
      }
      if (expr.args.size() < 4) {
        error_ = "dispatch requires kernel and three dimension arguments";
        return false;
      }
      if (expr.args.front().kind != Expr::Kind::Name) {
        error_ = "dispatch requires kernel name as first argument";
        return false;
      }
      const Expr &kernelExpr = expr.args.front();
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
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
        ReturnKind dimKind = inferExprReturnKind(expr.args[i], params, locals);
        if (!isIntegerKind(dimKind)) {
          error_ = "dispatch dimensions require integer expressions";
          return false;
        }
      }
      const auto &kernelParams = paramsByDef_[kernelPath];
      if (expr.args.size() != kernelParams.size() + 4) {
        error_ = "dispatch argument count mismatch for " + kernelPath;
        return false;
      }
      for (size_t i = 4; i < expr.args.size(); ++i) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer")) {
      if (currentDefinitionIsCompute_) {
        error_ = "buffer is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "buffer requires gpu_dispatch effect";
        return false;
      }
      if (expr.templateArgs.size() != 1) {
        error_ = "buffer requires exactly one template argument";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "buffer requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      ReturnKind countKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (!isIntegerKind(countKind)) {
        error_ = "buffer size requires integer expression";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(expr.templateArgs.front());
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "upload")) {
      if (currentDefinitionIsCompute_) {
        error_ = "upload is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "upload requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "upload does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "upload requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "upload does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      std::string elemType;
      if (!resolveArrayElemType(expr.args.front(), elemType)) {
        error_ = "upload requires array input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "upload requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "readback")) {
      if (currentDefinitionIsCompute_) {
        error_ = "readback is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "readback requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "readback does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "readback requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "readback does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args.front(), elemType)) {
        error_ = "readback requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "readback requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer_load")) {
      if (!currentDefinitionIsCompute_) {
        error_ = "buffer_load requires a compute definition";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "buffer_load does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error_ = "buffer_load requires buffer and index arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer_load does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[0]) || !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args[0], elemType)) {
        error_ = "buffer_load requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer_load requires numeric/bool element type";
        return false;
      }
      ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
      if (!isIntegerKind(indexKind)) {
        error_ = "buffer_load requires integer index";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      if (!currentDefinitionIsCompute_) {
        error_ = "buffer_store requires a compute definition";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "buffer_store does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error_ = "buffer_store requires buffer, index, and value arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer_store does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[0]) || !validateExpr(params, locals, expr.args[1]) ||
          !validateExpr(params, locals, expr.args[2])) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args[0], elemType)) {
        error_ = "buffer_store requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer_store requires numeric/bool element type";
        return false;
      }
      ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
      if (!isIntegerKind(indexKind)) {
        error_ = "buffer_store requires integer index";
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(expr.args[2], params, locals);
      if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
        error_ = "buffer_store value type mismatch";
        return false;
      }
      return true;
    }
      if (resolvedMethod && (resolved == "/array/count" || resolved == "/vector/count" || resolved == "/string/count" ||
                             resolved == "/map/count")) {
        if (!expr.templateArgs.empty()) {
          error_ = "count does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && expr.name == "count" && it == defMap_.end()) {
        if (!expr.templateArgs.empty()) {
          error_ = "count does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/vector/capacity") {
        if (!expr.templateArgs.empty()) {
          error_ = "capacity does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "capacity does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin capacity";
          return false;
        }
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "capacity requires vector target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && expr.name == "capacity" && it == defMap_.end()) {
        if (!expr.templateArgs.empty()) {
          error_ = "capacity does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "capacity does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin capacity";
          return false;
        }
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "capacity requires vector target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      std::string builtinName;
      if (getBuiltinOperatorName(expr, builtinName)) {
        size_t expectedArgs = builtinName == "negate" ? 1 : 2;
        if (expr.args.size() != expectedArgs) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        const bool isPlusMinus = builtinName == "plus" || builtinName == "minus";
        bool leftPointer = false;
        bool rightPointer = false;
        if (expr.args.size() == 2) {
          leftPointer = isPointerExpr(expr.args[0], params, locals);
          rightPointer = isPointerExpr(expr.args[1], params, locals);
        }
        if (isPlusMinus && expr.args.size() == 2 && (leftPointer || rightPointer)) {
          if (leftPointer && rightPointer) {
            error_ = "pointer arithmetic does not support pointer + pointer";
            return false;
          }
          if (rightPointer) {
            error_ = "pointer arithmetic requires pointer on the left";
            return false;
          }
          const Expr &offsetExpr = expr.args[1];
          ReturnKind offsetKind = inferExprReturnKind(offsetExpr, params, locals);
          if (offsetKind != ReturnKind::Unknown && offsetKind != ReturnKind::Int &&
              offsetKind != ReturnKind::Int64 && offsetKind != ReturnKind::UInt64) {
            error_ = "pointer arithmetic requires integer offset";
            return false;
          }
        } else if (leftPointer || rightPointer) {
          error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
          return false;
        }
        if (builtinName == "negate") {
          if (!isNumericExpr(expr.args.front())) {
            error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
            return false;
          }
          if (isUnsignedExpr(expr.args.front())) {
            error_ = "negate does not support unsigned operands";
            return false;
          }
        } else {
          if (!leftPointer && !rightPointer) {
            if (!isNumericExpr(expr.args[0]) || !isNumericExpr(expr.args[1])) {
              error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
              return false;
            }
            if (hasMixedNumericDomain(expr.args)) {
              error_ = "arithmetic operators do not support mixed software/fixed numeric operands";
              return false;
            }
            if (hasMixedComplexNumeric(expr.args)) {
              error_ = "arithmetic operators do not support mixed complex/real operands";
              return false;
            }
            if (hasMixedSignedness(expr.args, false)) {
              error_ = "arithmetic operators do not support mixed signed/unsigned operands";
              return false;
            }
            if (hasMixedNumericCategory(expr.args)) {
              error_ = "arithmetic operators do not support mixed int/float operands";
              return false;
            }
          } else if (leftPointer) {
            if (!isNumericExpr(expr.args[1])) {
              error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
              return false;
            }
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinComparisonName(expr, builtinName)) {
        size_t expectedArgs = builtinName == "not" ? 1 : 2;
        if (expr.args.size() != expectedArgs) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        const bool isBooleanOp = builtinName == "and" || builtinName == "or" || builtinName == "not";
        if (isBooleanOp) {
          for (const auto &arg : expr.args) {
            if (!isBoolExpr(arg, params, locals)) {
              error_ = "boolean operators require bool operands";
              return false;
            }
          }
        } else {
          bool sawString = false;
          bool sawNonString = false;
          for (const auto &arg : expr.args) {
            if (isStringExpr(arg)) {
              sawString = true;
            } else {
              sawNonString = true;
            }
          }
          if (sawString) {
            if (sawNonString) {
              error_ = "comparisons do not support mixed string/numeric operands";
              return false;
            }
          } else {
            for (const auto &arg : expr.args) {
              if (!isComparisonOperand(arg)) {
                error_ = "comparisons require numeric, bool, or string operands";
                return false;
              }
            }
            if (hasMixedNumericDomain(expr.args)) {
              error_ = "comparisons do not support mixed software/fixed numeric operands";
              return false;
            }
            if (hasMixedComplexNumeric(expr.args)) {
              error_ = "comparisons do not support mixed complex/real operands";
              return false;
            }
            if (builtinName != "equal" && builtinName != "not_equal") {
              if (hasComplexNumeric(expr.args)) {
                error_ = "comparisons do not support ordered complex operands";
                return false;
              }
            }
            if (hasMixedSignedness(expr.args, true)) {
              error_ = "comparisons do not support mixed signed/unsigned operands";
              return false;
            }
            if (hasMixedNumericCategory(expr.args)) {
              error_ = "comparisons do not support mixed int/float operands";
              return false;
            }
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 3) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!isNumericExpr(arg)) {
            error_ = "clamp requires numeric operands";
            return false;
          }
        }
        if (hasMixedNumericDomain(expr.args)) {
          error_ = "clamp does not support mixed software/fixed numeric operands";
          return false;
        }
        if (hasComplexNumeric(expr.args)) {
          error_ = "clamp does not support complex operands";
          return false;
        }
        if (hasMixedSignedness(expr.args, false)) {
          error_ = "clamp does not support mixed signed/unsigned operands";
          return false;
        }
        if (hasMixedNumericCategory(expr.args)) {
          error_ = "clamp does not support mixed int/float operands";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!isNumericExpr(arg)) {
            error_ = builtinName + " requires numeric operands";
            return false;
          }
        }
        if (hasMixedNumericDomain(expr.args)) {
          error_ = builtinName + " does not support mixed software/fixed numeric operands";
          return false;
        }
        if (hasComplexNumeric(expr.args)) {
          error_ = builtinName + " does not support complex operands";
          return false;
        }
        if (hasMixedSignedness(expr.args, false)) {
          error_ = builtinName + " does not support mixed signed/unsigned operands";
          return false;
        }
        if (hasMixedNumericCategory(expr.args)) {
          error_ = builtinName + " does not support mixed int/float operands";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!isNumericExpr(expr.args.front())) {
          error_ = builtinName + " requires numeric operand";
          return false;
        }
        if (builtinName == "sign" && hasComplexNumeric(expr.args)) {
          error_ = "sign does not support complex operands";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!isNumericExpr(expr.args.front())) {
          error_ = builtinName + " requires numeric operand";
          return false;
        }
        if (hasComplexNumeric(expr.args)) {
          error_ = builtinName + " does not support complex operands";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
        if (!expr.templateArgs.empty()) {
          error_ = builtinName + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = builtinName + " does not accept block arguments";
          return false;
        }
        size_t expectedArgs = 1;
        if (builtinName == "lerp" || builtinName == "fma") {
          expectedArgs = 3;
        } else if (builtinName == "pow" || builtinName == "atan2" || builtinName == "hypot" ||
                   builtinName == "copysign") {
          expectedArgs = 2;
        }
        if (expr.args.size() != expectedArgs) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        auto validateFloatArgs = [&](size_t count) -> bool {
          for (size_t i = 0; i < count; ++i) {
            if (!isFloatExpr(expr.args[i])) {
              error_ = builtinName + " requires float operands";
              return false;
            }
          }
          return true;
        };
        if (builtinName == "lerp") {
          for (const auto &arg : expr.args) {
            if (!isNumericExpr(arg)) {
              error_ = builtinName + " requires numeric operands";
              return false;
            }
          }
          if (hasMixedNumericDomain(expr.args)) {
            error_ = builtinName + " does not support mixed software/fixed numeric operands";
            return false;
          }
          if (hasMixedComplexNumeric(expr.args)) {
            error_ = builtinName + " does not support mixed complex/real operands";
            return false;
          }
          if (hasMixedSignedness(expr.args, false)) {
            error_ = builtinName + " does not support mixed signed/unsigned operands";
            return false;
          }
          if (hasMixedNumericCategory(expr.args)) {
            error_ = builtinName + " does not support mixed int/float operands";
            return false;
          }
        } else if (builtinName == "pow") {
          for (const auto &arg : expr.args) {
            if (!isNumericExpr(arg)) {
              error_ = builtinName + " requires numeric operands";
              return false;
            }
          }
          if (hasMixedNumericDomain(expr.args)) {
            error_ = builtinName + " does not support mixed software/fixed numeric operands";
            return false;
          }
          if (hasMixedComplexNumeric(expr.args)) {
            error_ = builtinName + " does not support mixed complex/real operands";
            return false;
          }
          if (hasMixedSignedness(expr.args, false)) {
            error_ = builtinName + " does not support mixed signed/unsigned operands";
            return false;
          }
          if (hasMixedNumericCategory(expr.args)) {
            error_ = builtinName + " does not support mixed int/float operands";
            return false;
          }
        } else if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
          if (!validateFloatArgs(1)) {
            return false;
          }
        } else if (builtinName == "fma") {
          if (!validateFloatArgs(3)) {
            return false;
          }
        } else if (builtinName == "atan2" || builtinName == "hypot" || builtinName == "copysign") {
          if (!validateFloatArgs(2)) {
            return false;
          }
        } else {
          if (!validateFloatArgs(1)) {
            return false;
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinArrayAccessName(expr, builtinName)) {
        if (!expr.templateArgs.empty()) {
          error_ = builtinName + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = builtinName + " does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        std::string elemType;
        bool isArrayOrString = resolveArrayTarget(expr.args.front(), elemType) || resolveStringTarget(expr.args.front());
        std::string mapKeyType;
        bool isMap = resolveMapKeyType(expr.args.front(), mapKeyType);
        if (!isArrayOrString && !isMap) {
          error_ = builtinName + " requires array, vector, map, or string target";
          return false;
        }
        if (!isMap) {
          if (!isIntegerOrBoolExpr(expr.args[1], params, locals)) {
            error_ = builtinName + " requires integer index";
            return false;
          }
        } else if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!isStringExpr(expr.args[1])) {
              error_ = builtinName + " requires string map key";
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(expr.args[1])) {
                error_ = builtinName + " requires map key type " + mapKeyType;
                return false;
              }
              ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
              if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                error_ = builtinName + " requires map key type " + mapKeyType;
                return false;
              }
            }
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinConvertName(expr, builtinName)) {
        if (expr.templateArgs.size() != 1) {
          error_ = "convert requires exactly one template argument";
          return false;
        }
        const std::string &typeName = expr.templateArgs[0];
        if (typeName != "int" && typeName != "i32" && typeName != "i64" && typeName != "u64" &&
            typeName != "bool" && typeName != "float" && typeName != "f32" && typeName != "f64") {
          error_ = "unsupported convert target type: " + typeName;
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        if (!isConvertibleExpr(expr.args.front())) {
          error_ = "convert requires numeric or bool operand";
          return false;
        }
        return true;
      }
      PrintBuiltin printBuiltin;
      if (getPrintBuiltin(expr, printBuiltin)) {
        error_ = printBuiltin.name + " is only supported as a statement";
        return false;
      }
      if (getBuiltinPointerName(expr, builtinName)) {
        if (!expr.templateArgs.empty()) {
          error_ = "pointer helpers do not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "pointer helpers do not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (builtinName == "location") {
          const Expr &target = expr.args.front();
          if (target.kind != Expr::Kind::Name) {
            error_ = "location requires a local binding";
            return false;
          }
          if (isEntryArgsName(target.name)) {
            error_ = "location requires a local binding";
            return false;
          }
          if (locals.count(target.name) == 0 && !isParam(params, target.name)) {
            error_ = "location requires a local binding";
            return false;
          }
        }
        if (builtinName == "dereference") {
          if (!isPointerLikeExpr(expr.args.front(), params, locals)) {
            error_ = "dereference requires a pointer or reference";
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinCollectionName(expr, builtinName)) {
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = builtinName + " literal does not accept block arguments";
          return false;
        }
        if (builtinName == "vector" && !expr.args.empty()) {
          if (activeEffects_.count("heap_alloc") == 0) {
            error_ = "vector literal requires heap_alloc effect";
            return false;
          }
        }
        if (builtinName == "array" || builtinName == "vector") {
          if (expr.templateArgs.size() != 1) {
            error_ = builtinName + " literal requires exactly one template argument";
            return false;
          }
        } else {
          if (expr.templateArgs.size() != 2) {
            error_ = "map literal requires exactly two template arguments";
            return false;
          }
          if (expr.args.size() % 2 != 0) {
            error_ = "map literal requires an even number of arguments";
            return false;
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        if ((builtinName == "array" || builtinName == "vector") && !expr.templateArgs.empty()) {
          const std::string &elemType = expr.templateArgs.front();
          for (const auto &arg : expr.args) {
            if (!validateCollectionElementType(arg, elemType, builtinName + " literal requires element type ")) {
              return false;
            }
          }
        }
        if (builtinName == "map" && expr.templateArgs.size() == 2) {
          const std::string &keyType = expr.templateArgs[0];
          const std::string &valueType = expr.templateArgs[1];
          for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
            if (!validateCollectionElementType(expr.args[i], keyType, "map literal requires key type ")) {
              return false;
            }
            if (!validateCollectionElementType(expr.args[i + 1], valueType, "map literal requires value type ")) {
              return false;
            }
          }
        }
        return true;
      }
      auto hasActiveBorrowForBinding = [&](const std::string &name,
                                           const std::string &ignoreBorrowName = std::string()) -> bool {
        if (currentDefinitionIsUnsafe_) {
          return false;
        }
        auto referenceRootForBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
          if (binding.typeName != "Reference") {
            return "";
          }
          if (!binding.referenceRoot.empty()) {
            return binding.referenceRoot;
          }
          return bindingName;
        };
        auto hasBorrowFrom = [&](const std::string &borrowName, const BindingInfo &binding) -> bool {
          if (borrowName == name) {
            return false;
          }
          if (!ignoreBorrowName.empty() && borrowName == ignoreBorrowName) {
            return false;
          }
          if (endedReferenceBorrows_.count(borrowName) > 0) {
            return false;
          }
          const std::string root = referenceRootForBinding(borrowName, binding);
          return !root.empty() && root == name;
        };
        for (const auto &param : params) {
          if (hasBorrowFrom(param.name, param.binding)) {
            return true;
          }
        }
        for (const auto &entry : locals) {
          if (hasBorrowFrom(entry.first, entry.second)) {
            return true;
          }
        }
        return false;
      };
      auto formatBorrowedBindingError = [&](const std::string &borrowRoot, const std::string &sinkName) {
        const std::string sink = sinkName.empty() ? borrowRoot : sinkName;
        error_ = "borrowed binding: " + borrowRoot + " (root: " + borrowRoot + ", sink: " + sink + ")";
      };
      auto findNamedBinding = [&](const std::string &name) -> const BindingInfo * {
        if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
          return paramBinding;
        }
        auto itBinding = locals.find(name);
        if (itBinding != locals.end()) {
          return &itBinding->second;
        }
        return nullptr;
      };
      auto resolveReferenceEscapeSink = [&](const std::string &targetName, std::string &sinkOut) -> bool {
        sinkOut.clear();
        if (const BindingInfo *targetParam = findParamBinding(params, targetName)) {
          if (targetParam->typeName == "Reference") {
            sinkOut = targetName;
            return true;
          }
          return false;
        }
        auto targetIt = locals.find(targetName);
        if (targetIt == locals.end() || targetIt->second.typeName != "Reference" ||
            targetIt->second.referenceRoot.empty()) {
          return false;
        }
        if (const BindingInfo *rootParam = findParamBinding(params, targetIt->second.referenceRoot)) {
          if (rootParam->typeName == "Reference") {
            sinkOut = targetIt->second.referenceRoot;
            return true;
          }
        }
        return false;
      };
      std::function<bool(const Expr &, std::string &)> resolveLocationRootBindingName;
      resolveLocationRootBindingName = [&](const Expr &pointerExpr, std::string &rootNameOut) -> bool {
        if (pointerExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string pointerBuiltinName;
        if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) && pointerBuiltinName == "location" &&
            pointerExpr.args.size() == 1) {
          if (pointerExpr.args.front().kind != Expr::Kind::Name) {
            return false;
          }
          rootNameOut = pointerExpr.args.front().name;
          return true;
        }
        std::string opName;
        if (!getBuiltinOperatorName(pointerExpr, opName) || (opName != "plus" && opName != "minus") ||
            pointerExpr.args.size() != 2) {
          return false;
        }
        if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
          return false;
        }
        return resolveLocationRootBindingName(pointerExpr.args.front(), rootNameOut);
      };
      std::function<bool(const Expr &, std::string &, std::string &)> resolveMutablePointerWriteTarget;
      resolveMutablePointerWriteTarget =
          [&](const Expr &pointerExpr, std::string &borrowRootOut, std::string &ignoreBorrowNameOut) -> bool {
        if (pointerExpr.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, pointerExpr.name)) {
            return false;
          }
          const BindingInfo *pointerBinding = findNamedBinding(pointerExpr.name);
          if (!pointerBinding || !isPointerLikeExpr(pointerExpr, params, locals)) {
            return false;
          }
          ignoreBorrowNameOut = pointerExpr.name;
          if (!pointerBinding->referenceRoot.empty()) {
            borrowRootOut = pointerBinding->referenceRoot;
          }
          return true;
        }
        if (pointerExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string pointerBuiltinName;
        if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) && pointerBuiltinName == "location" &&
            pointerExpr.args.size() == 1) {
          const Expr &locationTarget = pointerExpr.args.front();
          if (locationTarget.kind != Expr::Kind::Name || !isMutableBinding(params, locals, locationTarget.name)) {
            return false;
          }
          const BindingInfo *targetBinding = findNamedBinding(locationTarget.name);
          if (targetBinding == nullptr) {
            return false;
          }
          if (targetBinding->typeName == "Reference") {
            ignoreBorrowNameOut = locationTarget.name;
            if (!targetBinding->referenceRoot.empty()) {
              borrowRootOut = targetBinding->referenceRoot;
            } else {
              borrowRootOut = locationTarget.name;
            }
          } else {
            borrowRootOut = locationTarget.name;
          }
          return true;
        }
        std::string opName;
        if (!getBuiltinOperatorName(pointerExpr, opName) || (opName != "plus" && opName != "minus") ||
            pointerExpr.args.size() != 2) {
          return false;
        }
        if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
          return false;
        }
        return resolveMutablePointerWriteTarget(pointerExpr.args.front(), borrowRootOut, ignoreBorrowNameOut);
      };
      if (isSimpleCallName(expr, "move")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (expr.isMethodCall) {
          error_ = "move does not support method-call syntax";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "move does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "move does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "move requires exactly one argument";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name) {
          error_ = "move requires a binding name";
          return false;
        }
        const BindingInfo *binding = findParamBinding(params, target.name);
        if (!binding) {
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            binding = &it->second;
          }
        }
        if (!binding) {
          error_ = "move requires a local binding or parameter: " + target.name;
          return false;
        }
        if (binding->typeName == "Reference") {
          error_ = "move does not support Reference bindings: " + target.name;
          return false;
        }
        if (hasActiveBorrowForBinding(target.name)) {
          formatBorrowedBindingError(target.name, target.name);
          return false;
        }
        if (movedBindings_.count(target.name) > 0) {
          error_ = "use-after-move: " + target.name;
          return false;
        }
        movedBindings_.insert(target.name);
        return true;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          error_ = "assign requires exactly two arguments";
          return false;
        }
        const Expr &target = expr.args.front();
        const bool targetIsName = target.kind == Expr::Kind::Name;
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = "assign target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          std::string pointerBorrowRoot;
          std::string ignoreBorrowName;
          if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot, ignoreBorrowName)) {
            if (pointerExpr.kind == Expr::Kind::Name && !isMutableBinding(params, locals, pointerExpr.name)) {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            std::string locationRootName;
            if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
                !isMutableBinding(params, locals, locationRootName)) {
              error_ = "assign target must be a mutable binding: " + locationRootName;
              return false;
            }
            error_ = "assign target must be a mutable pointer binding";
            return false;
          }
          if (!pointerBorrowRoot.empty() && hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
            const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
            formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
            return false;
          }
          if (currentDefinitionIsUnsafe_ && isUnsafeReferenceExpr(expr.args[1])) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink) {
              if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
                return false;
              }
            }
          }
          if (!currentDefinitionIsUnsafe_) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink && reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (currentDefinitionIsUnsafe_ && targetIsName && isUnsafeReferenceExpr(expr.args[1])) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink)) {
            if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
        }
        if (!currentDefinitionIsUnsafe_ && targetIsName) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink) &&
              reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        if (targetIsName) {
          movedBindings_.erase(target.name);
        }
        return true;
      }
      std::string mutateName;
      if (getBuiltinMutationName(expr, mutateName)) {
        if (expr.args.size() != 1) {
          error_ = mutateName + " requires exactly one argument";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = mutateName + " target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = mutateName + " target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          std::string pointerBorrowRoot;
          std::string ignoreBorrowName;
          if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot, ignoreBorrowName)) {
            if (pointerExpr.kind == Expr::Kind::Name && !isMutableBinding(params, locals, pointerExpr.name)) {
              error_ = mutateName + " target must be a mutable binding";
              return false;
            }
            std::string locationRootName;
            if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
                !isMutableBinding(params, locals, locationRootName)) {
              error_ = mutateName + " target must be a mutable binding: " + locationRootName;
              return false;
            }
            error_ = mutateName + " target must be a mutable pointer binding";
            return false;
          }
          if (!pointerBorrowRoot.empty() && hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
            const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
            formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
            return false;
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
        } else {
          error_ = mutateName + " target must be a mutable binding";
          return false;
        }
        if (!isNumericExpr(target)) {
          error_ = mutateName + " requires numeric operand";
          return false;
        }
        return true;
      }
      if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos) {
        std::string builtinName;
        if (getBuiltinClampName(expr, builtinName, true) || getBuiltinMinMaxName(expr, builtinName, true) ||
            getBuiltinAbsSignName(expr, builtinName, true) || getBuiltinSaturateName(expr, builtinName, true) ||
            getBuiltinMathName(expr, builtinName, true)) {
          error_ = "math builtin requires import /std/math/* or /std/math/<name>: " + expr.name;
          return false;
        }
      }
      error_ = "unknown call target: " + expr.name;
      return false;
    }
    const auto &calleeParams = paramsByDef_[resolved];
    if (calleeParams.empty() && structNames_.count(resolved) > 0) {
      std::vector<ParameterInfo> fieldParams;
      fieldParams.reserve(it->second->statements.size());
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      bool hasMissingDefaults = false;
      for (const auto &stmt : it->second->statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + resolved;
          return false;
        }
        if (isStaticField(stmt)) {
          continue;
        }
        ParameterInfo field;
        field.name = stmt.name;
        if (stmt.args.size() == 1) {
          field.defaultExpr = &stmt.args.front();
        } else {
          hasMissingDefaults = true;
        }
        fieldParams.push_back(field);
      }
      if (hasMissingDefaults && expr.args.empty() && !hasNamedArguments(expr.argNames)) {
        if (hasStructZeroArgConstructor(resolved)) {
          return true;
        }
      }
      if (!validateNamedArgumentsAgainstParams(fieldParams, expr.argNames, error_)) {
        return false;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(fieldParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          error_ = "argument count mismatch for " + resolved;
        } else {
          error_ = orderError;
        }
        return false;
      }
      std::unordered_set<const Expr *> explicitArgs;
      explicitArgs.reserve(expr.args.size());
      for (const auto &arg : expr.args) {
        explicitArgs.insert(&arg);
      }
      for (const auto *arg : orderedArgs) {
        if (!arg || explicitArgs.count(arg) == 0) {
          continue;
        }
        if (!validateExpr(params, locals, *arg)) {
          return false;
        }
      }
      return true;
    }
    if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, error_)) {
      return false;
    }
    std::vector<const Expr *> orderedArgs;
    std::string orderError;
    if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + resolved;
      } else {
        error_ = orderError;
      }
      return false;
    }
    for (const auto *arg : orderedArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr(params, locals, *arg)) {
        return false;
      }
    }
    bool calleeIsUnsafe = false;
    for (const auto &transform : it->second->transforms) {
      if (transform.name == "unsafe") {
        calleeIsUnsafe = true;
        break;
      }
    }
    if (currentDefinitionIsUnsafe_ && !calleeIsUnsafe) {
      for (size_t i = 0; i < orderedArgs.size() && i < calleeParams.size(); ++i) {
        const Expr *arg = orderedArgs[i];
        if (arg == nullptr) {
          continue;
        }
        if (calleeParams[i].binding.typeName != "Reference") {
          continue;
        }
        if (!isUnsafeReferenceExpr(*arg)) {
          continue;
        }
        error_ = "unsafe reference escapes across safe boundary to " + resolved;
        return false;
      }
    }
    return true;
  }
  return false;
}

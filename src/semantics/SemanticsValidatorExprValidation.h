      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string builtinName;
        if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
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
        return getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2;
      }
      return false;
    };
    auto resolveMethodTarget =
        [&](const Expr &receiver, const std::string &methodName, std::string &resolvedOut, bool &isBuiltinOut) -> bool {
      isBuiltinOut = false;
      std::string elemType;
      if (methodName == "count") {
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
        std::string inferred = typeNameForReturnKind(inferExprReturnKind(receiver, params, locals));
        if (!inferred.empty()) {
          typeName = inferred;
        }
      }
      if (typeName.empty()) {
        error_ = "unknown method target for " + methodName;
        return false;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        error_ = "unknown method target for " + methodName;
        return false;
      }
      if (typeName == "array" || typeName == "map") {
        error_ = "unknown method: " + methodName;
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + methodName;
        return true;
      }
      resolvedOut = resolveTypePath(typeName, receiver.namespacePrefix) + "/" + methodName;
      return true;
    };

    std::string resolved = resolveCalleePath(expr);
    bool resolvedMethod = false;
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        error_ = "method call missing receiver";
        return false;
      }
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
    PathSpaceBuiltin pathSpaceBuiltin;
    if (getPathSpaceBuiltin(expr, pathSpaceBuiltin) && defMap_.find(resolved) == defMap_.end()) {
      error_ = pathSpaceBuiltin.name + " is statement-only";
      return false;
    }

    if (!validateNamedArguments(expr.args, expr.argNames, resolved, error_)) {
      return false;
    }
    auto it = defMap_.find(resolved);
    if (it == defMap_.end() || resolvedMethod) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (resolvedMethod && (resolved == "/array/count" || resolved == "/string/count" || resolved == "/map/count")) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
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
          error_ = "arithmetic operators require numeric operands";
          return false;
        }
        if (builtinName == "negate") {
          if (!isNumericExpr(expr.args.front())) {
            error_ = "arithmetic operators require numeric operands";
            return false;
          }
          if (isUnsignedExpr(expr.args.front())) {
            error_ = "negate does not support unsigned operands";
            return false;
          }
        } else {
          if (!leftPointer && !rightPointer) {
            if (!isNumericExpr(expr.args[0]) || !isNumericExpr(expr.args[1])) {
              error_ = "arithmetic operators require numeric operands";
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
              error_ = "arithmetic operators require numeric operands";
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
        for (const auto &arg : expr.args) {
          if (isBooleanOp) {
            if (!isIntegerOrBoolExpr(arg, params, locals)) {
              error_ = "boolean operators require integer or bool operands";
              return false;
            }
          } else {
            if (!isComparisonOperand(arg)) {
              error_ = "comparisons require numeric or bool operands";
              return false;
            }
          }
        }
        if (!isBooleanOp) {
          if (hasMixedSignedness(expr.args, true)) {
            error_ = "comparisons do not support mixed signed/unsigned operands";
            return false;
          }
          if (hasMixedNumericCategory(expr.args)) {
            error_ = "comparisons do not support mixed int/float operands";
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
      if (getBuiltinClampName(expr, builtinName)) {
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
      if (getBuiltinMinMaxName(expr, builtinName)) {
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
      if (getBuiltinAbsSignName(expr, builtinName)) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!isNumericExpr(expr.args.front())) {
          error_ = builtinName + " requires numeric operand";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinSaturateName(expr, builtinName)) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!isNumericExpr(expr.args.front())) {
          error_ = builtinName + " requires numeric operand";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinArrayAccessName(expr, builtinName)) {
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        std::string elemType;
        bool isArrayOrString = resolveArrayTarget(expr.args.front(), elemType) || resolveStringTarget(expr.args.front());
        bool isMap = resolveMapTarget(expr.args.front());
        if (!isArrayOrString && !isMap) {
          error_ = builtinName + " requires array, map, or string target";
          return false;
        }
        if (!isMap) {
          if (!isIntegerOrBoolExpr(expr.args[1], params, locals)) {
            error_ = builtinName + " requires integer index";
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
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (builtinName == "location") {
          const Expr &target = expr.args.front();
          if (target.kind != Expr::Kind::Name || locals.count(target.name) == 0 || isParam(params, target.name)) {
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
        if (builtinName == "array") {
          if (expr.templateArgs.size() != 1) {
            error_ = "array literal requires exactly one template argument";
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
        return true;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          error_ = "assign requires exactly two arguments";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = "assign target must be a mutable binding: " + target.name;
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          if (pointerExpr.kind != Expr::Kind::Name || !isMutableBinding(params, locals, pointerExpr.name)) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          if (!isPointerLikeExpr(pointerExpr, params, locals)) {
            error_ = "assign target must be a mutable pointer binding";
            return false;
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        return true;
      }
      error_ = "unknown call target: " + expr.name;
      return false;
    }
    const auto &calleeParams = paramsByDef_[resolved];
    if (calleeParams.empty() && structNames_.count(resolved) > 0) {
      std::vector<ParameterInfo> fieldParams;
      fieldParams.reserve(it->second->statements.size());
      for (const auto &stmt : it->second->statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + resolved;
          return false;
        }
        ParameterInfo field;
        field.name = stmt.name;
        if (stmt.args.size() == 1) {
          field.defaultExpr = &stmt.args.front();
        }
        fieldParams.push_back(field);
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
    return true;
  }
  return false;
}

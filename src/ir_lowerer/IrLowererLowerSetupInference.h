
  auto comparisonKind = [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    if (left == LocalInfo::ValueKind::Bool) {
      left = LocalInfo::ValueKind::Int32;
    }
    if (right == LocalInfo::ValueKind::Bool) {
      right = LocalInfo::ValueKind::Int32;
    }
    return combineNumericKinds(left, right);
  };

  struct ReturnInfo {
    bool returnsVoid = false;
    bool returnsArray = false;
    LocalInfo::ValueKind kind = LocalInfo::ValueKind::Unknown;
    bool isResult = false;
    bool resultHasValue = false;
  };

  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferArrayElementKind;

  auto typeNameForValueKind = [](LocalInfo::ValueKind kind) -> std::string {
    switch (kind) {
      case LocalInfo::ValueKind::Int32:
        return "i32";
      case LocalInfo::ValueKind::Int64:
        return "i64";
      case LocalInfo::ValueKind::UInt64:
        return "u64";
      case LocalInfo::ValueKind::Float32:
        return "f32";
      case LocalInfo::ValueKind::Float64:
        return "f64";
      case LocalInfo::ValueKind::Bool:
        return "bool";
      case LocalInfo::ValueKind::String:
        return "string";
      default:
        return "";
    }
  };
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  resolveMethodCallDefinition = [&](const Expr &callExpr, const LocalMap &localsIn) -> const Definition * {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || !callExpr.isMethodCall) {
      return nullptr;
    }
    if (callExpr.args.empty()) {
      error = "method call missing receiver";
      return nullptr;
    }
    if (isArrayCountCall(callExpr, localsIn)) {
      return nullptr;
    }
    if (isVectorCapacityCall(callExpr, localsIn)) {
      return nullptr;
    }
    const Expr &receiver = callExpr.args.front();
    if (isEntryArgsName(receiver, localsIn)) {
      error = "unknown method target for " + callExpr.name;
      return nullptr;
    }
    std::string typeName;
    std::string resolvedTypePath;
    if (receiver.kind == Expr::Kind::Name) {
      auto it = localsIn.find(receiver.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + receiver.name;
        return nullptr;
      }
      if (it->second.kind == LocalInfo::Kind::Array) {
        if (!it->second.structTypeName.empty()) {
          resolvedTypePath = it->second.structTypeName;
        } else {
          typeName = "array";
        }
      } else if (it->second.kind == LocalInfo::Kind::Vector) {
        typeName = "vector";
      } else if (it->second.kind == LocalInfo::Kind::Map) {
        typeName = "map";
      } else if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
        if (!it->second.structTypeName.empty()) {
          resolvedTypePath = it->second.structTypeName;
        } else {
          typeName = "array";
        }
      } else if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
        error = "unknown method target for " + callExpr.name;
        return nullptr;
      } else {
        typeName = typeNameForValueKind(it->second.valueKind);
      }
    } else if (receiver.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(receiver, collection)) {
        if (collection == "array" && receiver.templateArgs.size() == 1) {
          typeName = "array";
        } else if (collection == "vector" && receiver.templateArgs.size() == 1) {
          typeName = "vector";
        } else if (collection == "map" && receiver.templateArgs.size() == 2) {
          typeName = "map";
        }
      }
      if (typeName.empty()) {
        typeName = typeNameForValueKind(inferExprKind(receiver, localsIn));
      }
      if (typeName.empty() && !receiver.isBinding && !receiver.isMethodCall) {
        std::string resolved = resolveExprPath(receiver);
        auto importIt = importAliases.find(receiver.name);
        if (structNames.count(resolved) == 0 && importIt != importAliases.end()) {
          resolved = importIt->second;
        }
        if (structNames.count(resolved) > 0) {
          resolvedTypePath = resolved;
        }
      }
    } else {
      typeName = typeNameForValueKind(inferExprKind(receiver, localsIn));
    }
    if (!resolvedTypePath.empty()) {
      const std::string resolved = resolvedTypePath + "/" + callExpr.name;
      auto defIt = defMap.find(resolved);
      if (defIt == defMap.end()) {
        error = "unknown method: " + resolved;
        return nullptr;
      }
      return defIt->second;
    }
    if (typeName.empty()) {
      error = "unknown method target for " + callExpr.name;
      return nullptr;
    }
    const std::string resolved = "/" + typeName + "/" + callExpr.name;
    auto defIt = defMap.find(resolved);
    if (defIt == defMap.end()) {
      error = "unknown method: " + resolved;
      return nullptr;
    }
    return defIt->second;
  };

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
  inferPointerTargetKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return LocalInfo::ValueKind::Unknown;
      }
      if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
        if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
          return LocalInfo::ValueKind::Unknown;
        }
        return it->second.valueKind;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto it = localsIn.find(target.name);
          if (it != localsIn.end()) {
            if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
              return LocalInfo::ValueKind::Unknown;
            }
            return it->second.valueKind;
          }
        }
        return LocalInfo::ValueKind::Unknown;
      }
      std::string builtin;
      if (getBuiltinOperatorName(expr, builtin) && (builtin == "plus" || builtin == "minus") &&
          expr.args.size() == 2) {
        std::function<bool(const Expr &)> isPointerExpr;
        isPointerExpr = [&](const Expr &candidate) -> bool {
          if (candidate.kind == Expr::Kind::Name) {
            auto it = localsIn.find(candidate.name);
            return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer;
          }
          if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
            return true;
          }
          if (candidate.kind == Expr::Kind::Call) {
            std::string innerName;
            if (getBuiltinOperatorName(candidate, innerName) && (innerName == "plus" || innerName == "minus") &&
                candidate.args.size() == 2) {
              return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
            }
          }
          return false;
        };
        if (isPointerExpr(expr.args[0]) && !isPointerExpr(expr.args[1])) {
          return inferPointerTargetKind(expr.args[0], localsIn);
        }
      }
    }
    return LocalInfo::ValueKind::Unknown;
  };

  inferArrayElementKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it != localsIn.end()) {
        if (it->second.kind == LocalInfo::Kind::Array) {
          return it->second.valueKind;
        }
        if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
          return it->second.valueKind;
        }
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(expr, collection) && collection == "array" && expr.templateArgs.size() == 1) {
        return valueKindFromTypeName(expr.templateArgs.front());
      }
      if (!expr.isMethodCall) {
        StructArrayInfo structInfo;
        if (resolveStructArrayInfoFromPath(resolveExprPath(expr), structInfo)) {
          return structInfo.elementKind;
        }
      }
      if (!expr.isMethodCall) {
        const std::string resolved = resolveExprPath(expr);
        auto defIt = defMap.find(resolved);
        if (defIt != defMap.end()) {
          ReturnInfo info;
          if (getReturnInfo && getReturnInfo(resolved, info) && !info.returnsVoid && info.returnsArray) {
            return info.kind;
          }
        }
        if (isSimpleCallName(expr, "count") && expr.args.size() == 1 && !isArrayCountCall(expr, localsIn) &&
            !isStringCountCall(expr, localsIn)) {
          Expr methodExpr = expr;
          methodExpr.isMethodCall = true;
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee) {
            ReturnInfo info;
            if (getReturnInfo && getReturnInfo(callee->fullPath, info) && !info.returnsVoid && info.returnsArray) {
              return info.kind;
            }
          }
        }
      } else {
        const Definition *callee = resolveMethodCallDefinition(expr, localsIn);
        if (callee) {
          ReturnInfo info;
          if (getReturnInfo && getReturnInfo(callee->fullPath, info) && !info.returnsVoid && info.returnsArray) {
            return info.kind;
          }
        }
      }
    }
    return LocalInfo::ValueKind::Unknown;
  };

  inferExprKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    switch (expr.kind) {
      case Expr::Kind::Literal:
        if (expr.isUnsigned) {
          return LocalInfo::ValueKind::UInt64;
        }
        if (expr.intWidth == 64) {
          return LocalInfo::ValueKind::Int64;
        }
        return LocalInfo::ValueKind::Int32;
      case Expr::Kind::FloatLiteral:
        return (expr.floatWidth == 64) ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      case Expr::Kind::BoolLiteral:
        return LocalInfo::ValueKind::Bool;
      case Expr::Kind::StringLiteral:
        return LocalInfo::ValueKind::String;
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it == localsIn.end()) {
          std::string mathConst;
          if (getMathConstantName(expr.name, mathConst)) {
            return LocalInfo::ValueKind::Float64;
          }
          return LocalInfo::ValueKind::Unknown;
        }
        if (it->second.kind == LocalInfo::Kind::Value) {
          return it->second.valueKind;
        }
        if (it->second.kind == LocalInfo::Kind::Reference) {
          if (it->second.referenceToArray) {
            return LocalInfo::ValueKind::Unknown;
          }
          return it->second.valueKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      case Expr::Kind::Call: {
        if (expr.isFieldAccess) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &receiver = expr.args.front();
          std::string structPath;
          if (receiver.kind == Expr::Kind::Name) {
            auto it = localsIn.find(receiver.name);
            if (it != localsIn.end()) {
              structPath = it->second.structTypeName;
            }
          } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
            structPath = resolveExprPath(receiver);
          }
          if (structPath.empty()) {
            return LocalInfo::ValueKind::Unknown;
          }
          int32_t fieldIndex = -1;
          LocalInfo::ValueKind fieldKind = LocalInfo::ValueKind::Unknown;
          if (!resolveStructFieldIndex(structPath, expr.name, fieldIndex, fieldKind)) {
            return LocalInfo::ValueKind::Unknown;
          }
          return fieldKind;
        }
        if (expr.isMethodCall) {
          if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
              expr.args.front().name == "Result" && expr.name == "ok") {
            return expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          }
          if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
            auto it = localsIn.find(expr.args.front().name);
            if (it != localsIn.end() && it->second.isFileHandle) {
              if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" ||
                  expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
                return LocalInfo::ValueKind::Int32;
              }
            }
          }
        } else {
          if (isSimpleCallName(expr, "File")) {
            return LocalInfo::ValueKind::Int64;
          }
          if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
            const Expr &arg = expr.args.front();
            if (arg.kind == Expr::Kind::Name) {
              auto it = localsIn.find(arg.name);
              if (it != localsIn.end() && it->second.isResult) {
                return it->second.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
              }
            }
            if (arg.kind == Expr::Kind::Call) {
              if (!arg.isMethodCall && isSimpleCallName(arg, "File")) {
                return LocalInfo::ValueKind::Int64;
              }
              if (arg.isMethodCall && !arg.args.empty() && arg.args.front().kind == Expr::Kind::Name) {
                auto it = localsIn.find(arg.args.front().name);
                if (it != localsIn.end() && it->second.isFileHandle) {
                  if (arg.name == "write" || arg.name == "write_line" || arg.name == "write_byte" ||
                      arg.name == "write_bytes" || arg.name == "flush" || arg.name == "close") {
                    return LocalInfo::ValueKind::Int32;
                  }
                }
                if (arg.args.front().name == "Result" && arg.name == "ok") {
                  return arg.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
                }
              }
            }
          }
        }
        if (!expr.isMethodCall) {
          const std::string resolved = resolveExprPath(expr);
          auto defIt = defMap.find(resolved);
          if (defIt != defMap.end()) {
            ReturnInfo info;
            if (getReturnInfo && getReturnInfo(resolved, info) && !info.returnsVoid) {
              if (info.returnsArray) {
                return LocalInfo::ValueKind::Unknown;
              }
              return info.kind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
          if (isSimpleCallName(expr, "count") && expr.args.size() == 1 && !isArrayCountCall(expr, localsIn) &&
              !isStringCountCall(expr, localsIn)) {
            Expr methodExpr = expr;
            methodExpr.isMethodCall = true;
            const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
            if (callee) {
              ReturnInfo info;
              if (getReturnInfo && getReturnInfo(callee->fullPath, info) && !info.returnsVoid) {
                if (info.returnsArray) {
                  return LocalInfo::ValueKind::Unknown;
                }
                return info.kind;
              }
              return LocalInfo::ValueKind::Unknown;
            }
          }
        } else {
          const Definition *callee = resolveMethodCallDefinition(expr, localsIn);
          if (callee) {
            ReturnInfo info;
            if (getReturnInfo && getReturnInfo(callee->fullPath, info) && !info.returnsVoid) {
              if (info.returnsArray) {
                return LocalInfo::ValueKind::Unknown;
              }
              return info.kind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
        }
        if (isArrayCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (isStringCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (isVectorCapacityCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::StringLiteral) {
            return LocalInfo::ValueKind::Int32;
          }
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
              return LocalInfo::ValueKind::Int32;
            }
          }
          if (isEntryArgsName(target, localsIn)) {
            return LocalInfo::ValueKind::Unknown;
          }
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
              if (it->second.mapValueKind != LocalInfo::ValueKind::Unknown &&
                  it->second.mapValueKind != LocalInfo::ValueKind::String) {
                return it->second.mapValueKind;
              }
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2) {
              LocalInfo::ValueKind kind = valueKindFromTypeName(target.templateArgs[1]);
              if (kind != LocalInfo::ValueKind::Unknown && kind != LocalInfo::ValueKind::String) {
                return kind;
              }
            }
          }
          LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end()) {
              if (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector) {
                elemKind = it->second.valueKind;
              } else if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
                elemKind = it->second.valueKind;
              }
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
                target.templateArgs.size() == 1) {
              elemKind = valueKindFromTypeName(target.templateArgs.front());
            }
          }
          if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
            return LocalInfo::ValueKind::Unknown;
          }
          return elemKind;
        }
        std::string builtin;
        if (getBuiltinComparisonName(expr, builtin)) {
          return LocalInfo::ValueKind::Bool;
        }
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferExprKind(expr.args.front(), localsIn);
          }
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto left = inferExprKind(expr.args[0], localsIn);
          auto right = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(left, right);
        }
        if (getBuiltinClampName(expr, hasMathImport)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto first = inferExprKind(expr.args[0], localsIn);
          auto second = inferExprKind(expr.args[1], localsIn);
          auto third = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(first, second), third);
        }
        if (getBuiltinMinMaxName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto left = inferExprKind(expr.args[0], localsIn);
          auto right = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(left, right);
        }
        if (getBuiltinLerpName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto startKind = inferExprKind(expr.args[0], localsIn);
          auto endKind = inferExprKind(expr.args[1], localsIn);
          auto tKind = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(startKind, endKind), tKind);
        }
        if (getBuiltinPowName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto leftKind = inferExprKind(expr.args[0], localsIn);
          auto rightKind = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(leftKind, rightKind);
        }
        if (getBuiltinFmaName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto firstKind = inferExprKind(expr.args[0], localsIn);
          auto secondKind = inferExprKind(expr.args[1], localsIn);
          auto thirdKind = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(firstKind, secondKind), thirdKind);
        }
        if (getBuiltinHypotName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto leftKind = inferExprKind(expr.args[0], localsIn);
          auto rightKind = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(leftKind, rightKind);
        }
        if (getBuiltinCopysignName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto leftKind = inferExprKind(expr.args[0], localsIn);
          auto rightKind = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(leftKind, rightKind);
        }
        if (getBuiltinAngleName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinTrigName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinTrig2Name(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto leftKind = inferExprKind(expr.args[0], localsIn);
          auto rightKind = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(leftKind, rightKind);
        }
        if (getBuiltinArcTrigName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinHyperbolicName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinArcHyperbolicName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinExpName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinLogName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinAbsSignName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinSaturateName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinMathPredicateName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return LocalInfo::ValueKind::Bool;
        }
        if (getBuiltinRoundingName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinRootName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const std::string &typeName = expr.templateArgs.front();
          return valueKindFromTypeName(typeName);
        }
        if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement")) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end()) {
              if (it->second.kind != LocalInfo::Kind::Value && it->second.kind != LocalInfo::Kind::Reference) {
                return LocalInfo::ValueKind::Unknown;
              }
              return it->second.valueKind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") && target.args.size() == 1) {
            return inferPointerTargetKind(target.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        if (isIfCall(expr) && expr.args.size() == 3) {
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
          auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
            if (!isIfBlockEnvelope(candidate)) {
              return inferExprKind(candidate, localsBase);
            }
            LocalMap branchLocals = localsBase;
            bool sawValue = false;
            LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
            for (const auto &bodyExpr : candidate.bodyArguments) {
              if (bodyExpr.isBinding) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                LocalInfo info;
                info.index = 0;
                info.isMutable = isBindingMutable(bodyExpr);
                info.kind = bindingKind(bodyExpr);
                LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
                if (hasExplicitBindingTypeTransform(bodyExpr)) {
                  valueKind = bindingValueKind(bodyExpr, info.kind);
                } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
                  valueKind = inferExprKind(bodyExpr.args.front(), branchLocals);
                  if (valueKind == LocalInfo::ValueKind::Unknown) {
                    valueKind = LocalInfo::ValueKind::Int32;
                  }
                }
                info.valueKind = valueKind;
                applyStructArrayInfo(bodyExpr, info);
                branchLocals.emplace(bodyExpr.name, info);
                continue;
              }
              if (isReturnCall(bodyExpr)) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                return inferExprKind(bodyExpr.args.front(), branchLocals);
              }
              sawValue = true;
              lastKind = inferExprKind(bodyExpr, branchLocals);
            }
            return sawValue ? lastKind : LocalInfo::ValueKind::Unknown;
          };

          LocalInfo::ValueKind thenKind = inferBranchValueKind(expr.args[1], localsIn);
          LocalInfo::ValueKind elseKind = inferBranchValueKind(expr.args[2], localsIn);
          if (thenKind == elseKind) {
            return thenKind;
          }
          return combineNumericKinds(thenKind, elseKind);
        }
        if (isBlockCall(expr) && expr.hasBodyArguments) {
          const std::string resolved = resolveExprPath(expr);
          if (defMap.find(resolved) == defMap.end() && expr.args.empty() && expr.templateArgs.empty() &&
              !hasNamedArguments(expr.argNames)) {
            if (expr.bodyArguments.empty()) {
              return LocalInfo::ValueKind::Unknown;
            }
            LocalMap blockLocals = localsIn;
            LocalInfo::ValueKind result = LocalInfo::ValueKind::Unknown;
            for (const auto &bodyExpr : expr.bodyArguments) {
              if (bodyExpr.isBinding) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                LocalInfo info;
                info.index = 0;
                info.isMutable = isBindingMutable(bodyExpr);
                info.kind = bindingKind(bodyExpr);
                LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
                if (hasExplicitBindingTypeTransform(bodyExpr)) {
                  valueKind = bindingValueKind(bodyExpr, info.kind);
                } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
                  valueKind = inferExprKind(bodyExpr.args.front(), blockLocals);
                  if (valueKind == LocalInfo::ValueKind::Unknown) {
                    valueKind = LocalInfo::ValueKind::Int32;
                  }
                }
                info.valueKind = valueKind;
                applyStructArrayInfo(bodyExpr, info);
                blockLocals.emplace(bodyExpr.name, info);
                continue;
              }
              if (isReturnCall(bodyExpr)) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                return inferExprKind(bodyExpr.args.front(), blockLocals);
              }
              result = inferExprKind(bodyExpr, blockLocals);
            }
            return result;
          }
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (builtin == "dereference") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferPointerTargetKind(expr.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };

  getReturnInfo = [&](const std::string &path, ReturnInfo &outInfo) -> bool {
    auto cached = returnInfoCache.find(path);
    if (cached != returnInfoCache.end()) {
      outInfo = cached->second;
      return true;
    }
    auto defIt = defMap.find(path);
    if (defIt == defMap.end() || !defIt->second) {
      error = "native backend cannot resolve definition: " + path;
      return false;
    }
    if (!returnInferenceStack.insert(path).second) {
      error = "native backend return type inference requires explicit annotation on " + path;
      return false;
    }

    const Definition &def = *defIt->second;
    ReturnInfo info;

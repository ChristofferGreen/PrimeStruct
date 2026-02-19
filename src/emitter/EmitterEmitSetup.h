std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, std::vector<Expr>> paramMap;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> importAliases;
  bool hasMathImport = false;
  auto isMathImport = [](const std::string &path) -> bool {
    if (path == "/math/*") {
      return true;
    }
    return path.rfind("/math/", 0) == 0 && path.size() > 6;
  };
  for (const auto &importPath : program.imports) {
    if (isMathImport(importPath)) {
      hasMathImport = true;
      break;
    }
  }
  auto isStructTransformName = [](const std::string &name) {
    return name == "struct" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
           name == "platform_independent_padding";
  };
  auto isStructDefinition = [&](const Definition &def) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturn = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStruct = true;
      }
    }
    if (hasStruct) {
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
    }
    return true;
  };
  enum class HelperKind { Create, Destroy, Copy };
  struct HelperSuffixInfo {
    std::string_view suffix;
    HelperKind kind;
    std::string_view placement;
  };
  auto matchLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, HelperKind &kindOut, std::string &placementOut) -> bool {
    static const std::array<HelperSuffixInfo, 9> suffixes = {{
        {"Create", HelperKind::Create, ""},
        {"Destroy", HelperKind::Destroy, ""},
        {"Copy", HelperKind::Copy, ""},
        {"CreateStack", HelperKind::Create, "stack"},
        {"DestroyStack", HelperKind::Destroy, "stack"},
        {"CreateHeap", HelperKind::Create, "heap"},
        {"DestroyHeap", HelperKind::Destroy, "heap"},
        {"CreateBuffer", HelperKind::Create, "buffer"},
        {"DestroyBuffer", HelperKind::Destroy, "buffer"},
    }};
    for (const auto &info : suffixes) {
      const std::string_view suffix = info.suffix;
      if (fullPath.size() < suffix.size() + 1) {
        continue;
      }
      const size_t suffixStart = fullPath.size() - suffix.size();
      if (fullPath[suffixStart - 1] != '/') {
        continue;
      }
      if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
        continue;
      }
      parentOut = fullPath.substr(0, suffixStart - 1);
      kindOut = info.kind;
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };
  auto isLifecycleHelper = [&](const Definition &def, std::string &parentOut) {
    HelperKind kind = HelperKind::Create;
    std::string placement;
    if (!matchLifecycleHelper(def.fullPath, parentOut, kind, placement)) {
      parentOut.clear();
      return false;
    }
    return true;
  };
  auto isHelperMutable = [](const Definition &def) {
    for (const auto &transform : def.transforms) {
      if (transform.name == "mut") {
        return true;
      }
    }
    return false;
  };
  struct LifecycleHelpers {
    const Definition *create = nullptr;
    const Definition *destroy = nullptr;
    const Definition *createStack = nullptr;
    const Definition *destroyStack = nullptr;
  };
  std::unordered_map<std::string, LifecycleHelpers> helpersByStruct;

  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      structTypeMap[def.fullPath] = toCppName(def.fullPath);
    }
    std::string parentPath;
    HelperKind kind = HelperKind::Create;
    std::string placement;
    if (matchLifecycleHelper(def.fullPath, parentPath, kind, placement)) {
      if (kind == HelperKind::Copy) {
        continue;
      }
      auto &helpers = helpersByStruct[parentPath];
      if (placement == "stack") {
        if (kind == HelperKind::Create) {
          helpers.createStack = &def;
        } else {
          helpers.destroyStack = &def;
        }
      } else if (placement.empty()) {
        if (kind == HelperKind::Create) {
          helpers.create = &def;
        } else {
          helpers.destroy = &def;
        }
      }
    }
  }

  auto makeThisParam = [&](const std::string &structPath, bool isMutable) {
    Expr param;
    param.kind = Expr::Kind::Name;
    param.isBinding = true;
    param.name = "__self";
    Transform typeTransform;
    typeTransform.name = "Reference";
    typeTransform.templateArgs.push_back(structPath);
    param.transforms.push_back(std::move(typeTransform));
    if (isMutable) {
      Transform mutTransform;
      mutTransform.name = "mut";
      param.transforms.push_back(std::move(mutTransform));
    }
    return param;
  };

  auto returnTypeFor = [&](const Definition &def) {
    ReturnKind returnKind = returnKinds[def.fullPath];
    if (returnKind == ReturnKind::Void) {
      return std::string("void");
    }
    if (returnKind == ReturnKind::Int64) {
      return std::string("int64_t");
    }
    if (returnKind == ReturnKind::UInt64) {
      return std::string("uint64_t");
    }
    if (returnKind == ReturnKind::Float32) {
      return std::string("float");
    }
    if (returnKind == ReturnKind::Float64) {
      return std::string("double");
    }
    if (returnKind == ReturnKind::Bool) {
      return std::string("bool");
    }
    if (returnKind == ReturnKind::Array) {
      std::string typeName = "array<i32>";
      for (const auto &transform : def.transforms) {
        if (transform.name == "return" && !transform.templateArgs.empty()) {
          typeName = transform.templateArgs.front();
          break;
        }
      }
      BindingInfo info;
      info.typeName = "array";
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
        info.typeTemplateArg = arg;
      } else {
        info.typeTemplateArg = "i32";
      }
      return bindingTypeToCpp(info, def.namespacePrefix, importAliases, structTypeMap);
    }
    return std::string("int");
  };
  auto appendParam = [&](std::ostringstream &out,
                         const Expr &param,
                         const std::string &typeNamespace) {
    BindingInfo paramInfo = getBindingInfo(param);
    std::string paramType =
        bindingTypeToCpp(paramInfo, typeNamespace, importAliases, structTypeMap);
    const bool refCandidate = isReferenceCandidate(paramInfo);
    const bool passByRef = refCandidate && !paramInfo.isCopy;
    if (passByRef) {
      if (!paramInfo.isMutable && paramType.rfind("const ", 0) != 0) {
        out << "const ";
      }
      out << paramType << " & " << param.name;
      return;
    }
    bool needsConst = !paramInfo.isMutable;
    if (needsConst && paramType.rfind("const ", 0) == 0) {
      needsConst = false;
    }
    out << (needsConst ? "const " : "") << paramType << " " << param.name;
  };

  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      nameMap[def.fullPath] = toCppName(def.fullPath) + "_ctor";
      std::vector<Expr> instanceFields;
      instanceFields.reserve(def.statements.size());
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          continue;
        }
        if (getBindingInfo(stmt).isStatic) {
          continue;
        }
        instanceFields.push_back(stmt);
      }
      paramMap[def.fullPath] = std::move(instanceFields);
    } else {
      nameMap[def.fullPath] = toCppName(def.fullPath);
      std::string parentPath;
      if (isLifecycleHelper(def, parentPath)) {
        std::vector<Expr> params;
        params.reserve(def.parameters.size() + 1);
        params.push_back(makeThisParam(parentPath, isHelperMutable(def)));
        for (const auto &param : def.parameters) {
          params.push_back(param);
        }
        paramMap[def.fullPath] = std::move(params);
      } else {
        paramMap[def.fullPath] = def.parameters;
      }
    }
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
  }

  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      const std::string scopedPrefix = prefix + "/";
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        const std::string rootPath = "/" + remainder;
        if (nameMap.count(rootPath) > 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    auto defIt = defMap.find(importPath);
    if (defIt == defMap.end()) {
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    const std::string rootPath = "/" + remainder;
    if (nameMap.count(rootPath) > 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }

  auto isParam = [](const std::vector<Expr> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<ReturnKind(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<Expr> &,
                           const std::unordered_map<std::string, ReturnKind> &)>
      inferExprReturnKind;

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<Expr> &params,
                            const std::unordered_map<std::string, ReturnKind> &locals) -> ReturnKind {
    auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
      if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::Void || right == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
        return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
        if (left == ReturnKind::Int64 || left == ReturnKind::Int) {
          if (right == ReturnKind::Int64 || right == ReturnKind::Int) {
            return ReturnKind::Int64;
          }
        }
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int && right == ReturnKind::Int) {
        return ReturnKind::Int;
      }
      return ReturnKind::Unknown;
    };

    if (expr.kind == Expr::Kind::Literal) {
      if (expr.isUnsigned) {
        return ReturnKind::UInt64;
      }
      if (expr.intWidth == 64) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Int;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      return ReturnKind::Bool;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return ReturnKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name)) {
        return ReturnKind::Unknown;
      }
      if (isBuiltinMathConstantName(expr.name, hasMathImport)) {
        return ReturnKind::Float64;
      }
      auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return ReturnKind::Unknown;
      }
      return it->second;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string full = resolveExprPath(expr);
      auto defIt = defMap.find(full);
      if (defIt != defMap.end()) {
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        return calleeKind;
      }
      if (isBuiltinBlock(expr, nameMap) && expr.hasBodyArguments) {
        if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
          return ReturnKind::Unknown;
        }
        std::unordered_map<std::string, ReturnKind> blockLocals = locals;
        bool sawValue = false;
        ReturnKind last = ReturnKind::Unknown;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo info = getBindingInfo(bodyExpr);
            ReturnKind bindingKind = returnKindForTypeName(info.typeName);
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
              if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
                bindingKind = initKind;
              }
            }
            blockLocals.emplace(bodyExpr.name, bindingKind);
            continue;
          }
          sawValue = true;
          last = inferExprReturnKind(bodyExpr, params, blockLocals);
        }
        return sawValue ? last : ReturnKind::Unknown;
      }
      if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
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
        auto inferBranchValueKind = [&](const Expr &candidate,
                                        const std::unordered_map<std::string, ReturnKind> &localsBase) -> ReturnKind {
          if (!isIfBlockEnvelope(candidate)) {
            return inferExprReturnKind(candidate, params, localsBase);
          }
          std::unordered_map<std::string, ReturnKind> branchLocals = localsBase;
          bool sawValue = false;
          ReturnKind lastKind = ReturnKind::Unknown;
          for (const auto &bodyExpr : candidate.bodyArguments) {
            if (bodyExpr.isBinding) {
              BindingInfo info = getBindingInfo(bodyExpr);
              ReturnKind bindingKind = returnKindForTypeName(info.typeName);
              if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
                ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, branchLocals);
                if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
                  bindingKind = initKind;
                }
              }
              branchLocals.emplace(bodyExpr.name, bindingKind);
              continue;
            }
            sawValue = true;
            lastKind = inferExprReturnKind(bodyExpr, params, branchLocals);
          }
          return sawValue ? lastKind : ReturnKind::Unknown;
        };

        ReturnKind thenKind = inferBranchValueKind(expr.args[1], locals);
        ReturnKind elseKind = inferBranchValueKind(expr.args[2], locals);
        if (thenKind == ReturnKind::Unknown || elseKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        if (thenKind == elseKind) {
          return thenKind;
        }
        return combineNumeric(thenKind, elseKind);
      }
      const char *cmp = nullptr;
      if (getBuiltinComparison(expr, cmp)) {
        return ReturnKind::Bool;
      }
      char op = '\0';
      if (getBuiltinOperator(expr, op)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (isBuiltinNegate(expr)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (isBuiltinClamp(expr, hasMathImport)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      std::string mathName;
      if (getBuiltinMathName(expr, mathName, hasMathImport)) {
        if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
          return ReturnKind::Bool;
        }
        if (mathName == "lerp" && expr.args.size() == 3) {
          ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
          result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
          result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
          return result;
        }
        if (mathName == "pow" && expr.args.size() == 2) {
          ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
          result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
          return result;
        }
        if (expr.args.empty()) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Float64) {
          return ReturnKind::Float64;
        }
        if (argKind == ReturnKind::Float32) {
          return ReturnKind::Float32;
        }
        return ReturnKind::Unknown;
      }
      std::string convertName;
      if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1) {
        return returnKindForTypeName(expr.templateArgs.front());
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> ReturnKind {
    ReturnKind &kind = returnKinds[def.fullPath];
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      return ReturnKind::Unknown;
    }
    const auto &params = paramMap[def.fullPath];
    ReturnKind inferred = ReturnKind::Unknown;
    bool sawReturn = false;
    std::unordered_map<std::string, ReturnKind> locals;
    std::function<void(const Expr &, std::unordered_map<std::string, ReturnKind> &)> visitStmt;
    visitStmt = [&](const Expr &stmt, std::unordered_map<std::string, ReturnKind> &activeLocals) {
      if (stmt.isBinding) {
        BindingInfo info = getBindingInfo(stmt);
        ReturnKind bindingKind = returnKindForTypeName(info.typeName);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
          if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
            bindingKind = initKind;
          }
        }
        activeLocals.emplace(stmt.name, bindingKind);
        return;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
        }
        if (exprKind == ReturnKind::Unknown) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          return;
        }
        if (inferred != exprKind) {
          inferred = ReturnKind::Unknown;
          return;
        }
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) {
          std::unordered_map<std::string, ReturnKind> blockLocals = activeLocals;
          for (const auto &bodyStmt : block.bodyArguments) {
            visitStmt(bodyStmt, blockLocals);
          }
        };
        walkBlock(thenBlock);
        walkBlock(elseBlock);
        return;
      }
    };
    for (const auto &stmt : def.statements) {
      visitStmt(stmt, locals);
    }
    if (!sawReturn) {
      kind = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      kind = ReturnKind::Int;
    } else {
      kind = inferred;
    }
    inferenceStack.erase(def.fullPath);
    return kind;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      inferDefinitionReturnKind(def);
    }
  }

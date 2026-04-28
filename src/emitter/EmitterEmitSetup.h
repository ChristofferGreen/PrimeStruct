std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, std::vector<Expr>> paramMap;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  std::unordered_map<std::string, ResultInfo> resultInfos;
  std::unordered_map<std::string, std::string> returnStructs;
  std::unordered_map<std::string, std::string> importAliases;
  struct OnErrorHandler {
    std::string errorType;
    std::string handlerPath;
    std::vector<Expr> boundArgs;
  };
  std::unordered_map<std::string, std::optional<OnErrorHandler>> onErrorByDef;
  bool hasMathImport = emitter::runEmitterEmitSetupMathImport(program);
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
  std::unordered_set<std::string> structPaths;
  structPaths.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  auto isLifecycleHelper = [&](const Definition &def, std::string &parentOut) {
    const auto lifecycleMatch = runEmitterEmitSetupLifecycleHelperMatchStep(def.fullPath);
    if (!lifecycleMatch.has_value()) {
      parentOut.clear();
      return false;
    }
    parentOut = lifecycleMatch->parentPath;
    return true;
  };
  auto isStructHelper = [&](const Definition &def, std::string &parentOut) {
    parentOut.clear();
    if (!def.isNested) {
      return false;
    }
    if (structPaths.count(def.fullPath) > 0) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string parent = def.fullPath.substr(0, slash);
    if (structPaths.count(parent) == 0) {
      return false;
    }
    parentOut = parent;
    return true;
  };
  auto isStaticHelper = [](const Definition &def) {
    for (const auto &transform : def.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
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
    const auto lifecycleMatch = runEmitterEmitSetupLifecycleHelperMatchStep(def.fullPath);
    if (lifecycleMatch.has_value()) {
      if (lifecycleMatch->kind == EmitterLifecycleHelperKind::Copy ||
          lifecycleMatch->kind == EmitterLifecycleHelperKind::Move) {
        continue;
      }
      auto &helpers = helpersByStruct[lifecycleMatch->parentPath];
      if (lifecycleMatch->placement == "stack") {
        if (lifecycleMatch->kind == EmitterLifecycleHelperKind::Create) {
          helpers.createStack = &def;
        } else {
          helpers.destroyStack = &def;
        }
      } else if (lifecycleMatch->placement.empty()) {
        if (lifecycleMatch->kind == EmitterLifecycleHelperKind::Create) {
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

  auto parseResultTypeName = [&](const std::string &typeName, ResultInfo &infoOut) -> bool {
    infoOut = ResultInfo{};
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(typeName, base, arg)) {
      return false;
    }
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (normalizedBase != "Result" && normalizedBase != "/std/result/Result" &&
        normalizedBase != "std/result/Result") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return false;
    }
    if (args.size() == 1) {
      infoOut.isResult = true;
      infoOut.hasValue = false;
      infoOut.errorType = args.front();
      return true;
    }
    if (args.size() == 2) {
      infoOut.isResult = true;
      infoOut.hasValue = true;
      infoOut.valueType = args.front();
      infoOut.errorType = args.back();
      return true;
    }
    return false;
  };

  auto parseTransformArgumentExpr = [&](const std::string &text,
                                        const std::string &namespacePrefix,
                                        Expr &out) -> bool {
    Lexer lexer(text);
    Parser parser(lexer.tokenize());
    std::string parseError;
    if (!parser.parseExpression(out, namespacePrefix, parseError)) {
      return false;
    }
    return true;
  };

  auto parseOnErrorTransform = [&](const std::vector<Transform> &transforms,
                                   const std::string &namespacePrefix,
                                   const std::string &context,
                                   std::optional<OnErrorHandler> &out) -> bool {
    (void)context;
    out.reset();
    for (const auto &transform : transforms) {
      if (transform.name != "on_error") {
        continue;
      }
      if (out.has_value()) {
        return false;
      }
      if (transform.templateArgs.size() != 2) {
        return false;
      }
      Expr handlerExpr;
      handlerExpr.kind = Expr::Kind::Name;
      handlerExpr.name = transform.templateArgs[1];
      handlerExpr.namespacePrefix = namespacePrefix;
      std::string handlerPath = resolveExprPath(handlerExpr);
      if (defMap.count(handlerPath) == 0) {
        auto importIt = importAliases.find(transform.templateArgs[1]);
        if (importIt != importAliases.end()) {
          handlerPath = importIt->second;
        }
      }
      auto defIt = defMap.find(handlerPath);
      if (defIt == defMap.end()) {
        return false;
      }
      OnErrorHandler handler;
      handler.errorType = transform.templateArgs.front();
      handler.handlerPath = handlerPath;
      handler.boundArgs.reserve(transform.arguments.size());
      for (const auto &argText : transform.arguments) {
        Expr argExpr;
        if (!parseTransformArgumentExpr(argText, namespacePrefix, argExpr)) {
          return false;
        }
        handler.boundArgs.push_back(std::move(argExpr));
      }
      out = std::move(handler);
    }
    return true;
  };

  std::function<std::string(const std::string &, const std::string &)> resolveStructReturnPath;
  auto resolveCollectionReturnPath = [](const std::string &typeName) -> std::string {
    std::string currentType = normalizeBindingTypeName(typeName);
    while (true) {
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(currentType, base, arg)) {
        return "";
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args)) {
        return "";
      }
      if ((base == "array" || base == "vector") && args.size() == 1) {
        return "/" + base;
      }
      if (base == "map" && args.size() == 2) {
        return "/map";
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
        currentType = normalizeBindingTypeName(args.front());
        continue;
      }
      return "";
    }
  };

  auto returnTypeFor = [&](const Definition &def) {
    ReturnKind returnKind = returnKinds[def.fullPath];
    auto resultIt = resultInfos.find(def.fullPath);
    if (resultIt != resultInfos.end() && resultIt->second.isResult) {
      return sourceResultCppType(resultIt->second.hasValue);
    }
    auto structIt = returnStructs.find(def.fullPath);
    if (structIt != returnStructs.end()) {
      auto typeIt = structTypeMap.find(structIt->second);
      if (typeIt != structTypeMap.end()) {
        return typeIt->second;
      }
    }
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string structPath = resolveStructReturnPath(transform.templateArgs.front(), def.namespacePrefix);
      if (!structPath.empty()) {
        auto typeIt = structTypeMap.find(structPath);
        if (typeIt != structTypeMap.end()) {
          return typeIt->second;
        }
      }
      break;
    }
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
    if (returnKind == ReturnKind::String) {
      return std::string("std::string_view");
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
    if (returnKind == ReturnKind::Unknown) {
      for (const auto &transform : def.transforms) {
        if (transform.name == "return" && !transform.templateArgs.empty()) {
          std::string resolved =
              bindingTypeToCpp(transform.templateArgs.front(), def.namespacePrefix, importAliases, structTypeMap);
          if (resolved != "int") {
            return resolved;
          }
          break;
        }
      }
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
    bool needsConst = !paramInfo.isMutable && !isAliasingBinding(paramInfo);
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
        std::string helperParent;
        if (isStructHelper(def, helperParent) && !isStaticHelper(def)) {
          std::vector<Expr> params;
          params.reserve(def.parameters.size() + 1);
          params.push_back(makeThisParam(helperParent, isHelperMutable(def)));
          for (const auto &param : def.parameters) {
            params.push_back(param);
          }
          paramMap[def.fullPath] = std::move(params);
        } else {
          paramMap[def.fullPath] = def.parameters;
        }
      }
    }
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      ResultInfo resultInfo;
      if (parseResultTypeName(transform.templateArgs.front(), resultInfo)) {
        resultInfos[def.fullPath] = std::move(resultInfo);
      }
      break;
    }
    if (isStructDefinition(def)) {
      returnKinds[def.fullPath] = ReturnKind::Array;
      returnStructs[def.fullPath] = def.fullPath;
    }
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

  onErrorByDef.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    std::optional<OnErrorHandler> handler;
    parseOnErrorTransform(def.transforms, def.namespacePrefix, def.fullPath, handler);
    onErrorByDef.emplace(def.fullPath, std::move(handler));
  }

  resolveStructReturnPath = [&](const std::string &typeName,
                                const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (typeName == "Self") {
      return structTypeMap.count(namespacePrefix) > 0 ? namespacePrefix : "";
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return structTypeMap.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structTypeMap.count(direct) > 0) {
          return direct;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structTypeMap.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structTypeMap.count(root) > 0) {
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
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structTypeMap.count(importIt->second) > 0) {
      return importIt->second;
    }
    return "";
  };

  for (const auto &def : program.definitions) {
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &typeName = transform.templateArgs.front();
      std::string structPath = resolveStructReturnPath(typeName, def.namespacePrefix);
      if (!structPath.empty()) {
        returnKinds[def.fullPath] = ReturnKind::Array;
        returnStructs[def.fullPath] = structPath;
      } else {
        std::string collectionPath = resolveCollectionReturnPath(typeName);
        if (!collectionPath.empty()) {
          returnStructs[def.fullPath] = collectionPath;
        }
      }
      break;
    }
  }

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
    if (!splitTemplateTypeName(typeName, base, arg) || base != "Result") {
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
      return resultIt->second.hasValue ? std::string("uint64_t") : std::string("uint32_t");
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
  std::function<std::string(const Expr &,
                            const std::vector<Expr> &,
                            const std::unordered_map<std::string, std::string> &)>
      inferStructReturnPath;
  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  auto preferCollectionHelperPath = [&](const std::string &path) -> std::string {
    std::string preferred = path;
    if (preferred.rfind("/array/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        if (defMap.count(vectorAlias) > 0) {
          return vectorAlias;
        }
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        if (defMap.count(stdlibAlias) > 0) {
          return stdlibAlias;
        }
      }
    }
    if (preferred.rfind("/vector/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        if (defMap.count(stdlibAlias) > 0) {
          preferred = stdlibAlias;
        } else {
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            const std::string arrayAlias = "/array/" + suffix;
            if (defMap.count(arrayAlias) > 0) {
              preferred = arrayAlias;
            }
          }
        }
      }
    }
    if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        if (defMap.count(vectorAlias) > 0) {
          preferred = vectorAlias;
        } else {
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            const std::string arrayAlias = "/array/" + suffix;
            if (defMap.count(arrayAlias) > 0) {
              preferred = arrayAlias;
            }
          }
        }
      }
    }
    if (preferred.rfind("/map/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string stdlibAlias = "/std/collections/map/" + preferred.substr(std::string("/map/").size());
      if (defMap.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      }
    }
    if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
        const std::string mapAlias = "/map/" + suffix;
        if (defMap.count(mapAlias) > 0) {
          preferred = mapAlias;
        }
      }
    }
    return preferred;
  };
  auto normalizeCollectionMethodName = [](const std::string &receiverStruct,
                                          std::string methodName) -> std::string {
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    if (receiverStruct == "/vector" || receiverStruct == "/array") {
      const std::string vectorPrefix = "vector/";
      const std::string arrayPrefix = "array/";
      const std::string stdVectorPrefix = "std/collections/vector/";
      if (methodName.rfind(vectorPrefix, 0) == 0) {
        return methodName.substr(vectorPrefix.size());
      }
      if (methodName.rfind(arrayPrefix, 0) == 0) {
        return methodName.substr(arrayPrefix.size());
      }
      if (methodName.rfind(stdVectorPrefix, 0) == 0) {
        return methodName.substr(stdVectorPrefix.size());
      }
    }
    if (receiverStruct == "/map") {
      const std::string mapPrefix = "map/";
      const std::string stdMapPrefix = "std/collections/map/";
      if (methodName.rfind(mapPrefix, 0) == 0) {
        return methodName.substr(mapPrefix.size());
      }
      if (methodName.rfind(stdMapPrefix, 0) == 0) {
        return methodName.substr(stdMapPrefix.size());
      }
    }
    return methodName;
  };
  auto collectionMethodPathCandidates = [&](const std::string &receiverStruct,
                                            const std::string &methodName,
                                            const std::string &rawMethodName) -> std::vector<std::string> {
    if (receiverStruct == "/vector") {
      std::vector<std::string> candidates = {
          "/vector/" + methodName,
      };
      const bool blocksRemovedVectorAliasStructReturnForwarding =
          methodName == "at" || methodName == "at_unsafe";
      if (!blocksRemovedVectorAliasStructReturnForwarding) {
        candidates.push_back("/std/collections/vector/" + methodName);
      }
      if (allowsArrayVectorCompatibilitySuffix(methodName) &&
          !blocksRemovedVectorAliasStructReturnForwarding) {
        candidates.push_back("/array/" + methodName);
      }
      return candidates;
    }
    if (receiverStruct == "/array") {
      std::vector<std::string> candidates = {
          "/array/" + methodName,
      };
      if (allowsArrayVectorCompatibilitySuffix(methodName)) {
        candidates.push_back("/vector/" + methodName);
        candidates.push_back("/std/collections/vector/" + methodName);
      }
      return candidates;
    }
    if (receiverStruct == "/map") {
      std::vector<std::string> candidates = {
          "/std/collections/map/" + methodName,
      };
      const bool blocksRemovedMapAliasStructReturnForwarding =
          rawMethodName == "map/at" || rawMethodName == "map/at_unsafe" ||
          rawMethodName == "std/collections/map/at" || rawMethodName == "std/collections/map/at_unsafe";
      if (!blocksRemovedMapAliasStructReturnForwarding) {
        candidates.push_back("/map/" + methodName);
      }
      return candidates;
    }
    return {receiverStruct + "/" + methodName};
  };
  auto collectionHelperPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/std/collections/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
        appendUnique("/map/" + suffix);
      }
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [&](const std::string &path,
                                                               std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto pruneBuiltinVectorAccessStructReturnCandidates = [&](const Expr &candidate,
                                                            const std::string &path,
                                                            const std::vector<Expr> &params,
                                                            const std::unordered_map<std::string, std::string> &locals,
                                                            std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("vector/", 0) == 0 || normalizedPath.rfind("std/collections/vector/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    if (normalizedPath.rfind("/std/collections/vector/", 0) != 0) {
      return;
    }
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (suffix != "at" && suffix != "at_unsafe") {
      return;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
      return;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return;
    }
    const std::string receiverStruct = inferStructReturnPath(candidate.args[receiverIndex], params, locals);
    if (receiverStruct != "/vector" && receiverStruct != "/array" && receiverStruct != "/string") {
      return;
    }
    const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
    for (auto it = candidates.begin(); it != candidates.end();) {
      if (*it == canonicalCandidate) {
        it = candidates.erase(it);
      } else {
        ++it;
      }
    }
  };

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<Expr> &params,
                            const std::unordered_map<std::string, ReturnKind> &locals) -> ReturnKind {
    auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
      if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
          right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void) {
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
      return ReturnKind::String;
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
      const std::string resolvedPath = resolveExprPath(expr);
      const auto resolvedCandidates = collectionHelperPathCandidates(resolvedPath);
      bool sawDefinitionCandidate = false;
      ReturnKind firstValueKind = ReturnKind::Unknown;
      for (const auto &candidate : resolvedCandidates) {
        auto defIt = defMap.find(candidate);
        if (defIt == defMap.end()) {
          continue;
        }
        sawDefinitionCandidate = true;
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          continue;
        }
        if (firstValueKind == ReturnKind::Unknown) {
          firstValueKind = calleeKind;
        }
        auto structIt = returnStructs.find(candidate);
        const bool candidateReturnsStruct =
            calleeKind == ReturnKind::Array && structIt != returnStructs.end() && !structIt->second.empty();
        if (candidateReturnsStruct) {
          return calleeKind;
        }
      }
      if (firstValueKind != ReturnKind::Unknown) {
        return firstValueKind;
      }
      if (resolvedCandidates.empty()) {
        std::string preferred = preferCollectionHelperPath(resolvedPath);
        auto defIt = defMap.find(preferred);
        if (defIt != defMap.end()) {
          sawDefinitionCandidate = true;
          ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
          if (calleeKind != ReturnKind::Void && calleeKind != ReturnKind::Unknown) {
            return calleeKind;
          }
        }
      }
      if (sawDefinitionCandidate) {
        return ReturnKind::Unknown;
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

  auto resolveParamStructPath = [&](const Expr &param, const std::string &namespacePrefix) -> std::string {
    BindingInfo info = getBindingInfo(param);
    if (info.typeName == "Reference" || info.typeName == "Pointer") {
      return "";
    }
    const std::string typeNamespace = param.namespacePrefix.empty() ? namespacePrefix : param.namespacePrefix;
    return resolveStructReturnPath(info.typeName, typeNamespace);
  };

  inferStructReturnPath = [&](const Expr &expr,
                              const std::vector<Expr> &params,
                              const std::unordered_map<std::string, std::string> &locals) -> std::string {
    auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return allowAnyName || isBuiltinBlock(candidate, nameMap);
    };
    auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
      if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
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

    if (expr.isLambda) {
      return "";
    }

    if (expr.kind == Expr::Kind::Name) {
      for (const auto &param : params) {
        if (param.name == expr.name) {
          return resolveParamStructPath(param, expr.namespacePrefix);
        }
      }
      auto it = locals.find(expr.name);
      return it != locals.end() ? it->second : "";
    }

    if (expr.kind == Expr::Kind::Call) {
      if (expr.isMethodCall) {
        if (expr.args.empty()) {
          return "";
        }
        std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
        if (receiverStruct.empty()) {
          return "";
        }
        std::string rawMethodName = expr.name;
        if (!rawMethodName.empty() && rawMethodName.front() == '/') {
          rawMethodName.erase(rawMethodName.begin());
        }
        const std::string methodName = normalizeCollectionMethodName(receiverStruct, expr.name);
        auto candidates = collectionMethodPathCandidates(receiverStruct, methodName, rawMethodName);
        if ((receiverStruct == "/vector" || receiverStruct == "/array" || receiverStruct == "/string") &&
            (methodName == "at" || methodName == "at_unsafe")) {
          const std::string canonicalCandidate = "/std/collections/vector/" + methodName;
          for (auto it = candidates.begin(); it != candidates.end();) {
            if (*it == canonicalCandidate) {
              it = candidates.erase(it);
            } else {
              ++it;
            }
          }
        }
        for (const auto &candidate : candidates) {
          auto it = returnStructs.find(candidate);
          if (it != returnStructs.end()) {
            return it->second;
          }
        }
        for (const auto &candidate : candidates) {
          auto defIt = defMap.find(candidate);
          if (defIt == defMap.end()) {
            continue;
          }
          (void)inferDefinitionReturnKind(*defIt->second);
          auto it = returnStructs.find(candidate);
          if (it != returnStructs.end()) {
            return it->second;
          }
        }
        return "";
      }

      if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
        const Expr &thenArg = expr.args[1];
        const Expr &elseArg = expr.args[2];
        const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
        const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
        const Expr &thenExpr = thenValue ? *thenValue : thenArg;
        const Expr &elseExpr = elseValue ? *elseValue : elseArg;
        std::string thenPath = inferStructReturnPath(thenExpr, params, locals);
        if (thenPath.empty()) {
          return "";
        }
        std::string elsePath = inferStructReturnPath(elseExpr, params, locals);
        return thenPath == elsePath ? thenPath : "";
      }

      if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
        if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
          return inferStructReturnPath(valueExpr->args.front(), params, locals);
        }
        return inferStructReturnPath(*valueExpr, params, locals);
      }

      std::string collectionName;
      if (getBuiltinCollectionName(expr, collectionName)) {
        if ((collectionName == "array" || collectionName == "vector") && expr.templateArgs.size() == 1) {
          return "/" + collectionName;
        }
        if (collectionName == "map" && expr.templateArgs.size() == 2) {
          return "/map";
        }
      }

      const std::string resolvedExprPath = resolveExprPath(expr);
      auto resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
      pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
      pruneBuiltinVectorAccessStructReturnCandidates(expr, resolvedExprPath, params, locals, resolvedCandidates);
      for (const auto &candidate : resolvedCandidates) {
        auto it = returnStructs.find(candidate);
        if (it != returnStructs.end()) {
          return it->second;
        }
      }
      std::string resolved = resolvedCandidates.empty() ? preferCollectionHelperPath(resolveExprPath(expr))
                                                        : resolvedCandidates.front();
      if (structTypeMap.count(resolved) == 0) {
        auto importIt = importAliases.find(expr.name);
        if (importIt != importAliases.end() && structTypeMap.count(importIt->second) > 0) {
          return importIt->second;
        }
      }
      if (structTypeMap.count(resolved) > 0) {
        return resolved;
      }
      bool hasDefinitionCandidate = false;
      for (const auto &candidate : resolvedCandidates) {
        auto defIt = defMap.find(candidate);
        if (defIt == defMap.end()) {
          continue;
        }
        hasDefinitionCandidate = true;
        (void)inferDefinitionReturnKind(*defIt->second);
        auto it = returnStructs.find(candidate);
        if (it != returnStructs.end()) {
          return it->second;
        }
      }
      if (!hasDefinitionCandidate && resolvedCandidates.empty()) {
        auto defIt = defMap.find(resolved);
        if (defIt != defMap.end()) {
          (void)inferDefinitionReturnKind(*defIt->second);
          auto it = returnStructs.find(resolved);
          return it != returnStructs.end() ? it->second : "";
        }
      }
    }

    return "";
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> ReturnKind {
    ReturnKind &kind = returnKinds[def.fullPath];
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      return ReturnKind::Unknown;
    }
    bool hasExplicitReturnTransform = false;
    bool hasAutoReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasExplicitReturnTransform = true;
        hasAutoReturnTransform = transform.templateArgs.size() == 1 && transform.templateArgs.front() == "auto";
        break;
      }
    }
    const auto &params = paramMap[def.fullPath];
    ReturnKind inferred = ReturnKind::Unknown;
    std::string inferredStructPath;
    bool inferredStructConflict = false;
    bool sawNonCollectionReturn = false;
    bool sawReturn = false;
    std::unordered_map<std::string, ReturnKind> locals;
    std::unordered_map<std::string, std::string> localStructs;
    std::function<void(const Expr &,
                       std::unordered_map<std::string, ReturnKind> &,
                       std::unordered_map<std::string, std::string> &)>
        visitStmt;
    visitStmt = [&](const Expr &stmt,
                    std::unordered_map<std::string, ReturnKind> &activeLocals,
                    std::unordered_map<std::string, std::string> &activeStructs) {
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
        std::string structPath = resolveStructReturnPath(info.typeName, def.namespacePrefix);
        if (structPath.empty() && stmt.args.size() == 1) {
          structPath = inferStructReturnPath(stmt.args.front(), params, activeStructs);
        }
        if (!structPath.empty()) {
          activeStructs.emplace(stmt.name, structPath);
        }
        return;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        std::string exprStructPath;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
          exprStructPath = inferStructReturnPath(stmt.args.front(), params, activeStructs);
        }
        if (stmt.args.empty() || exprStructPath.empty()) {
          sawNonCollectionReturn = true;
        }
        if (exprKind != ReturnKind::Unknown && exprKind != ReturnKind::Array) {
          sawNonCollectionReturn = true;
        }
        if (!exprStructPath.empty()) {
          if (inferredStructPath.empty()) {
            inferredStructPath = exprStructPath;
          } else if (inferredStructPath != exprStructPath) {
            inferredStructConflict = true;
          }
        } else if (!inferredStructPath.empty() && !stmt.args.empty()) {
          inferredStructConflict = true;
        }
        if (exprKind == ReturnKind::Unknown) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          if (!exprStructPath.empty()) {
            inferredStructPath = exprStructPath;
          }
          return;
        }
        if (inferred != exprKind) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Array) {
          if (!exprStructPath.empty()) {
            if (inferredStructPath.empty()) {
              inferredStructPath = exprStructPath;
            } else if (inferredStructPath != exprStructPath) {
              inferred = ReturnKind::Unknown;
              inferredStructConflict = true;
            }
          } else if (!inferredStructPath.empty()) {
            inferred = ReturnKind::Unknown;
            inferredStructConflict = true;
          }
        }
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) {
          std::unordered_map<std::string, ReturnKind> blockLocals = activeLocals;
          std::unordered_map<std::string, std::string> blockStructs = activeStructs;
          for (const auto &bodyStmt : block.bodyArguments) {
            visitStmt(bodyStmt, blockLocals, blockStructs);
          }
        };
        walkBlock(thenBlock);
        walkBlock(elseBlock);
        return;
      }
    };
    for (const auto &stmt : def.statements) {
      visitStmt(stmt, locals, localStructs);
    }
    if (!sawReturn) {
      kind = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      if (hasExplicitReturnTransform) {
        kind = ReturnKind::Unknown;
      } else {
        kind = ReturnKind::Int;
      }
    } else {
      kind = inferred;
    }
    const bool keepInferredStructPath =
        (!hasExplicitReturnTransform || hasAutoReturnTransform || kind == ReturnKind::Array);
    if (keepInferredStructPath && !inferredStructPath.empty() && !inferredStructConflict &&
        !sawNonCollectionReturn) {
      returnStructs[def.fullPath] = inferredStructPath;
    }
    inferenceStack.erase(def.fullPath);
    return kind;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      inferDefinitionReturnKind(def);
    }
  }

  for (const auto &def : program.definitions) {
    if (returnStructs.count(def.fullPath) > 0) {
      continue;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string structPath = resolveStructReturnPath(transform.templateArgs.front(), def.namespacePrefix);
      if (!structPath.empty()) {
        returnStructs[def.fullPath] = structPath;
      }
      break;
    }
  }

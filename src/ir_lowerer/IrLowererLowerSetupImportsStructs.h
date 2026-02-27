
  std::unordered_map<std::string, const Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  std::unordered_map<std::string, std::string> importAliases;
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
    importAliases.emplace(remainder, importPath);
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
  std::unordered_set<std::string> structNames;
  for (const auto &def : program.definitions) {
    if (isStructDefinition(def)) {
      structNames.insert(def.fullPath);
    }
  }
  auto alignTo = [](uint32_t value, uint32_t alignment) -> uint32_t {
    if (alignment == 0) {
      return value;
    }
    uint32_t remainder = value % alignment;
    if (remainder == 0) {
      return value;
    }
    return value + (alignment - remainder);
  };
  auto parsePositiveInt = [](const std::string &text, int &valueOut) -> bool {
    std::string digits = text;
    if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
      digits.resize(digits.size() - 3);
    }
    if (digits.empty()) {
      return false;
    }
    int parsed = 0;
    for (char c : digits) {
      if (!std::isdigit(static_cast<unsigned char>(c))) {
        return false;
      }
      int digit = c - '0';
      if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
        return false;
      }
      parsed = parsed * 10 + digit;
    }
    if (parsed <= 0) {
      return false;
    }
    valueOut = parsed;
    return true;
  };
  auto extractAlignment = [&](const std::vector<Transform> &transforms,
                              const std::string &context,
                              uint32_t &alignmentOut,
                              bool &hasAlignment) -> bool {
    alignmentOut = 1;
    hasAlignment = false;
    for (const auto &transform : transforms) {
      if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
        continue;
      }
      if (hasAlignment) {
        error = "duplicate " + transform.name + " transform on " + context;
        return false;
      }
      if (transform.arguments.size() != 1 || !transform.templateArgs.empty()) {
        error = transform.name + " requires exactly one integer argument";
        return false;
      }
      int value = 0;
      if (!parsePositiveInt(transform.arguments[0], value)) {
        error = transform.name + " requires a positive integer argument";
        return false;
      }
      uint64_t bytes = static_cast<uint64_t>(value);
      if (transform.name == "align_kbytes") {
        bytes *= 1024ull;
      }
      if (bytes > std::numeric_limits<uint32_t>::max()) {
        error = transform.name + " alignment too large on " + context;
        return false;
      }
      alignmentOut = static_cast<uint32_t>(bytes);
      hasAlignment = true;
    }
    return true;
  };
  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (!typeName.empty() && typeName[0] == '/') {
      return typeName;
    }
    std::string resolved = namespacePrefix.empty() ? ("/" + typeName) : (namespacePrefix + "/" + typeName);
    if (structNames.count(resolved) > 0) {
      return resolved;
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end()) {
      return importIt->second;
    }
    return resolved;
  };
  auto joinTemplateArgs = [](const std::vector<std::string> &args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += args[i];
    }
    return out;
  };
  struct FieldBinding {
    std::string typeName;
    std::string typeTemplateArg;
  };
  auto isLayoutQualifierName = [](const std::string &name) {
    return name == "public" || name == "private" || name == "static" || name == "mut" || name == "copy" ||
           name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "pod" ||
           name == "handle" || name == "gpu_lane";
  };
  auto extractExplicitBindingTypeForLayout = [&](const Expr &expr, FieldBinding &bindingOut) -> bool {
    bindingOut = {};
    for (const auto &transform : expr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isLayoutQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      if (!transform.templateArgs.empty()) {
        bindingOut.typeName = transform.name;
        bindingOut.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
        return true;
      }
      bindingOut.typeName = transform.name;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    return false;
  };
  auto resolveStructLayoutExprPath = [&](const Expr &expr) -> std::string {
    if (expr.name.empty()) {
      return "";
    }
    if (expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      const size_t lastSlash = expr.namespacePrefix.find_last_of('/');
      const std::string suffix = lastSlash == std::string::npos ? expr.namespacePrefix
                                                                 : expr.namespacePrefix.substr(lastSlash + 1);
      if (suffix == expr.name && defMap.count(expr.namespacePrefix) > 0) {
        return expr.namespacePrefix;
      }
      std::string prefix = expr.namespacePrefix;
      while (!prefix.empty()) {
        std::string candidate = prefix + "/" + expr.name;
        if (defMap.count(candidate) > 0) {
          return candidate;
        }
        const size_t slash = prefix.find_last_of('/');
        if (slash == std::string::npos) {
          break;
        }
        prefix = prefix.substr(0, slash);
      }
      auto importIt = importAliases.find(expr.name);
      if (importIt != importAliases.end()) {
        return importIt->second;
      }
      return expr.namespacePrefix + "/" + expr.name;
    }
    std::string root = "/" + expr.name;
    if (defMap.count(root) > 0) {
      return root;
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return importIt->second;
    }
    return root;
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return nullptr;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return nullptr;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return nullptr;
    }
    if (!allowAnyName && !isBlockCall(candidate)) {
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
  std::function<bool(const Expr &, const std::unordered_map<std::string, FieldBinding> &, FieldBinding &)>
      inferPrimitiveFieldBinding;
  inferPrimitiveFieldBinding = [&](const Expr &initializer,
                                   const std::unordered_map<std::string, FieldBinding> &knownFields,
                                   FieldBinding &bindingOut) -> bool {
    if (initializer.kind == Expr::Kind::Literal) {
      bindingOut.typeName = initializer.isUnsigned ? "u64" : (initializer.intWidth == 64 ? "i64" : "i32");
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (initializer.kind == Expr::Kind::BoolLiteral) {
      bindingOut.typeName = "bool";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (initializer.kind == Expr::Kind::FloatLiteral) {
      bindingOut.typeName = initializer.floatWidth == 64 ? "f64" : "f32";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (initializer.kind == Expr::Kind::StringLiteral) {
      bindingOut.typeName = "string";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (initializer.kind == Expr::Kind::Name) {
      auto fieldIt = knownFields.find(initializer.name);
      if (fieldIt != knownFields.end()) {
        bindingOut = fieldIt->second;
        return true;
      }
      return false;
    }
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isFieldAccess) {
      return false;
    }
    if (initializer.hasBodyArguments && initializer.args.empty()) {
      const Expr *valueExpr = getEnvelopeValueExpr(initializer, false);
      return valueExpr != nullptr && inferPrimitiveFieldBinding(*valueExpr, knownFields, bindingOut);
    }
    std::string collection;
    if (getBuiltinCollectionName(initializer, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "Buffer") &&
          initializer.templateArgs.size() == 1) {
        bindingOut.typeName = collection;
        bindingOut.typeTemplateArg = initializer.templateArgs.front();
        return true;
      }
      if (collection == "map" && initializer.templateArgs.size() == 2) {
        bindingOut.typeName = "map";
        bindingOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
        return true;
      }
      return false;
    }
    if (getBuiltinConvertName(initializer) && initializer.templateArgs.size() == 1) {
      const std::string &target = initializer.templateArgs.front();
      if (target == "int") {
        bindingOut.typeName = "i32";
      } else {
        bindingOut.typeName = target;
      }
      bindingOut.typeTemplateArg.clear();
      return bindingOut.typeName == "i32" || bindingOut.typeName == "i64" || bindingOut.typeName == "u64" ||
             bindingOut.typeName == "f32" || bindingOut.typeName == "f64" || bindingOut.typeName == "bool" ||
             bindingOut.typeName == "string" || bindingOut.typeName == "integer" || bindingOut.typeName == "decimal" ||
             bindingOut.typeName == "complex";
    }
    return false;
  };
  std::function<std::string(const std::string &, std::unordered_set<std::string> &)> inferStructReturnPathFromDefinition;
  std::function<std::string(const Expr &,
                            const std::unordered_map<std::string, FieldBinding> &,
                            std::unordered_set<std::string> &)>
      inferStructReturnPathFromExpr;
  inferStructReturnPathFromExpr =
      [&](const Expr &expr,
          const std::unordered_map<std::string, FieldBinding> &knownFields,
          std::unordered_set<std::string> &visitedDefs) -> std::string {
    if (expr.kind == Expr::Kind::Name) {
      auto fieldIt = knownFields.find(expr.name);
      if (fieldIt == knownFields.end()) {
        return "";
      }
      std::string fieldType = fieldIt->second.typeName;
      if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldIt->second.typeTemplateArg.empty()) {
        fieldType = fieldIt->second.typeTemplateArg;
      }
      std::string resolved = resolveStructTypePath(fieldType, expr.namespacePrefix);
      return structNames.count(resolved) > 0 ? resolved : "";
    }
    if (expr.kind != Expr::Kind::Call) {
      return "";
    }
    if (isMatchCall(expr)) {
      Expr lowered;
      std::string loweredError;
      if (!lowerMatchToIf(expr, lowered, loweredError)) {
        return "";
      }
      return inferStructReturnPathFromExpr(lowered, knownFields, visitedDefs);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      const Expr *thenValue = getEnvelopeValueExpr(expr.args[1], true);
      const Expr *elseValue = getEnvelopeValueExpr(expr.args[2], true);
      const Expr &thenExpr = thenValue ? *thenValue : expr.args[1];
      const Expr &elseExpr = elseValue ? *elseValue : expr.args[2];
      std::string thenPath = inferStructReturnPathFromExpr(thenExpr, knownFields, visitedDefs);
      if (thenPath.empty()) {
        return "";
      }
      std::string elsePath = inferStructReturnPathFromExpr(elseExpr, knownFields, visitedDefs);
      return thenPath == elsePath ? thenPath : "";
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferStructReturnPathFromExpr(valueExpr->args.front(), knownFields, visitedDefs);
      }
      return inferStructReturnPathFromExpr(*valueExpr, knownFields, visitedDefs);
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        return "";
      }
      std::string receiverStruct = inferStructReturnPathFromExpr(expr.args.front(), knownFields, visitedDefs);
      if (receiverStruct.empty()) {
        return "";
      }
      return inferStructReturnPathFromDefinition(receiverStruct + "/" + expr.name, visitedDefs);
    }
    std::string resolved = resolveStructLayoutExprPath(expr);
    if (structNames.count(resolved) > 0) {
      return resolved;
    }
    return inferStructReturnPathFromDefinition(resolved, visitedDefs);
  };
  inferStructReturnPathFromDefinition = [&](const std::string &defPath,
                                            std::unordered_set<std::string> &visitedDefs) -> std::string {
    if (defPath.empty()) {
      return "";
    }
    if (!visitedDefs.insert(defPath).second) {
      return "";
    }
    auto defIt = defMap.find(defPath);
    if (defIt == defMap.end() || !defIt->second) {
      return "";
    }
    const Definition &def = *defIt->second;
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string resolved = resolveStructTypePath(transform.templateArgs.front(), def.namespacePrefix);
      if (structNames.count(resolved) > 0) {
        return resolved;
      }
      break;
    }
    auto inferFromReturnValue = [&](const Expr &valueExpr) -> std::string {
      std::unordered_map<std::string, FieldBinding> noFields;
      std::string inferred = inferStructReturnPathFromExpr(valueExpr, noFields, visitedDefs);
      return inferred;
    };
    if (def.returnExpr.has_value()) {
      std::string inferred = inferFromReturnValue(*def.returnExpr);
      if (!inferred.empty()) {
        return inferred;
      }
    }
    std::string inferred;
    for (const auto &stmt : def.statements) {
      if (!isReturnCall(stmt) || stmt.args.size() != 1) {
        continue;
      }
      std::string candidate = inferFromReturnValue(stmt.args.front());
      if (candidate.empty()) {
        continue;
      }
      if (inferred.empty()) {
        inferred = std::move(candidate);
        continue;
      }
      if (candidate != inferred) {
        return "";
      }
    }
    return inferred;
  };
  auto resolveFieldBindingForLayout = [&](const Definition &def,
                                          const Expr &expr,
                                          const std::unordered_map<std::string, FieldBinding> &knownFields,
                                          FieldBinding &bindingOut) -> bool {
    if (extractExplicitBindingTypeForLayout(expr, bindingOut)) {
      return true;
    }
    const std::string fieldPath = def.fullPath + "/" + expr.name;
    if (expr.args.size() != 1) {
      error = "omitted struct field envelope requires exactly one initializer: " + fieldPath;
      return false;
    }
    if (inferPrimitiveFieldBinding(expr.args.front(), knownFields, bindingOut)) {
      return true;
    }
    std::unordered_set<std::string> visitedDefs;
    std::string inferredStruct = inferStructReturnPathFromExpr(expr.args.front(), knownFields, visitedDefs);
    if (!inferredStruct.empty()) {
      bindingOut.typeName = std::move(inferredStruct);
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    error = "unresolved or ambiguous omitted struct field envelope: " + fieldPath;
    return false;
  };
  auto formatEnvelope = [&](const FieldBinding &binding) -> std::string {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto fieldCategory = [&](const Expr &expr) -> IrStructFieldCategory {
    bool hasPod = false;
    bool hasHandle = false;
    bool hasGpuLane = false;
    for (const auto &transform : expr.transforms) {
      if (transform.name == "pod") {
        hasPod = true;
      } else if (transform.name == "handle") {
        hasHandle = true;
      } else if (transform.name == "gpu_lane") {
        hasGpuLane = true;
      }
    }
    if (hasPod) {
      return IrStructFieldCategory::Pod;
    }
    if (hasHandle) {
      return IrStructFieldCategory::Handle;
    }
    if (hasGpuLane) {
      return IrStructFieldCategory::GpuLane;
    }
    return IrStructFieldCategory::Default;
  };
  auto fieldVisibility = [&](const Expr &expr) -> IrStructVisibility {
    IrStructVisibility visibility = IrStructVisibility::Public;
    for (const auto &transform : expr.transforms) {
      if (transform.name == "public") {
        visibility = IrStructVisibility::Public;
      } else if (transform.name == "private") {
        visibility = IrStructVisibility::Private;
      }
    }
    return visibility;
  };
  auto isStaticField = [&](const Expr &expr) {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  struct StructFieldInfo {
    std::string name;
    FieldBinding binding;
    bool isStatic = false;
  };
  std::unordered_map<std::string, std::vector<StructFieldInfo>> structFieldInfoByName;
  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def)) {
      continue;
    }
    std::vector<StructFieldInfo> fields;
    std::unordered_map<std::string, FieldBinding> knownFields;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      FieldBinding binding;
      if (!resolveFieldBindingForLayout(def, stmt, knownFields, binding)) {
        return false;
      }
      StructFieldInfo field;
      field.name = stmt.name;
      field.binding = std::move(binding);
      field.isStatic = isStaticField(stmt);
      knownFields[field.name] = field.binding;
      fields.push_back(std::move(field));
    }
    structFieldInfoByName.emplace(def.fullPath, std::move(fields));
  }
  struct TypeLayout {
    uint32_t sizeBytes = 0;
    uint32_t alignmentBytes = 1;
  };
  std::unordered_map<std::string, IrStructLayout> layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::function<bool(const Definition &, IrStructLayout &)> computeStructLayout;
  std::function<bool(const FieldBinding &, const std::string &, TypeLayout &)> typeLayoutForBinding;

  computeStructLayout = [&](const Definition &def, IrStructLayout &layoutOut) -> bool {
    auto cached = layoutCache.find(def.fullPath);
    if (cached != layoutCache.end()) {
      layoutOut = cached->second;
      return true;
    }
    if (!layoutStack.insert(def.fullPath).second) {
      error = "recursive struct layout not supported: " + def.fullPath;
      return false;
    }
    IrStructLayout layout;
    layout.name = def.fullPath;
    uint32_t structAlign = 1;
    uint32_t explicitStructAlign = 1;
    bool hasStructAlign = false;
    if (!extractAlignment(def.transforms, "struct " + def.fullPath, explicitStructAlign, hasStructAlign)) {
      return false;
    }
    uint32_t offset = 0;
    auto fieldInfoIt = structFieldInfoByName.find(def.fullPath);
    if (fieldInfoIt == structFieldInfoByName.end()) {
      error = "internal error: missing struct field info for " + def.fullPath;
      return false;
    }
    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      if (fieldIndex >= fieldInfoIt->second.size()) {
        error = "internal error: mismatched struct field info for " + def.fullPath;
        return false;
      }
      const FieldBinding &binding = fieldInfoIt->second[fieldIndex].binding;
      ++fieldIndex;
      const bool fieldIsStatic = isStaticField(stmt);
      if (fieldIsStatic) {
        IrStructField field;
        field.name = stmt.name;
        field.envelope = formatEnvelope(binding);
        field.offsetBytes = 0;
        field.sizeBytes = 0;
        field.alignmentBytes = 1;
        field.paddingKind = IrStructPaddingKind::None;
        field.category = fieldCategory(stmt);
        field.visibility = fieldVisibility(stmt);
        field.isStatic = true;
        layout.fields.push_back(std::move(field));
        continue;
      }
      TypeLayout typeLayout;
      if (!typeLayoutForBinding(binding, def.namespacePrefix, typeLayout)) {
        return false;
      }
      uint32_t explicitFieldAlign = 1;
      bool hasFieldAlign = false;
      const std::string fieldContext = "field " + def.fullPath + "/" + stmt.name;
      if (!extractAlignment(stmt.transforms, fieldContext, explicitFieldAlign, hasFieldAlign)) {
        return false;
      }
      if (hasFieldAlign && explicitFieldAlign < typeLayout.alignmentBytes) {
        error = "alignment requirement on " + fieldContext + " is smaller than required alignment of " +
                std::to_string(typeLayout.alignmentBytes);
        return false;
      }
      uint32_t fieldAlign = hasFieldAlign ? std::max(explicitFieldAlign, typeLayout.alignmentBytes)
                                          : typeLayout.alignmentBytes;
      uint32_t &activeOffset = offset;
      uint32_t alignedOffset = alignTo(activeOffset, fieldAlign);
      IrStructField field;
      field.name = stmt.name;
      field.envelope = formatEnvelope(binding);
      field.offsetBytes = alignedOffset;
      field.sizeBytes = typeLayout.sizeBytes;
      field.alignmentBytes = fieldAlign;
      field.paddingKind = (alignedOffset != activeOffset) ? IrStructPaddingKind::Align : IrStructPaddingKind::None;
      field.category = fieldCategory(stmt);
      field.visibility = fieldVisibility(stmt);
      field.isStatic = false;
      layout.fields.push_back(std::move(field));
      activeOffset = alignedOffset + typeLayout.sizeBytes;
      structAlign = std::max(structAlign, fieldAlign);
    }
    if (hasStructAlign && explicitStructAlign < structAlign) {
      error = "alignment requirement on struct " + def.fullPath + " is smaller than required alignment of " +
              std::to_string(structAlign);
      return false;
    }
    structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
    layout.alignmentBytes = structAlign;
    layout.totalSizeBytes = alignTo(offset, structAlign);
    layoutCache.emplace(def.fullPath, layout);
    layoutStack.erase(def.fullPath);
    layoutOut = layout;
    return true;
  };

  typeLayoutForBinding = [&](const FieldBinding &binding,
                             const std::string &namespacePrefix,
                             TypeLayout &layoutOut) -> bool {
    auto normalizeTypeName = [](const std::string &name) -> std::string {
      if (name == "int") {
        return "i32";
      }
      if (name == "float") {
        return "f32";
      }
      return name;
    };
    const std::string normalized = normalizeTypeName(binding.typeName);
    if (normalized == "i32" || normalized == "f32") {
      layoutOut = {4u, 4u};
      return true;
    }
    if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "bool") {
      layoutOut = {1u, 1u};
      return true;
    }
    if (normalized == "string") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "uninitialized") {
      if (binding.typeTemplateArg.empty()) {
        error = "uninitialized requires a template argument for layout";
        return false;
      }
      std::string base;
      std::string arg;
      FieldBinding unwrapped = binding;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
        unwrapped.typeName = base;
        unwrapped.typeTemplateArg = arg;
      } else {
        unwrapped.typeName = binding.typeTemplateArg;
        unwrapped.typeTemplateArg.clear();
      }
      return typeLayoutForBinding(unwrapped, namespacePrefix, layoutOut);
    }
    if (binding.typeName == "Pointer" || binding.typeName == "Reference") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "map") {
      layoutOut = {8u, 8u};
      return true;
    }
    std::string structPath = resolveStructTypePath(binding.typeName, namespacePrefix);
    auto defIt = defMap.find(structPath);
    if (defIt == defMap.end()) {
      error = "unknown struct type for layout: " + binding.typeName;
      return false;
    }
    IrStructLayout nested;
    if (!computeStructLayout(*defIt->second, nested)) {
      return false;
    }
    layoutOut.sizeBytes = nested.totalSizeBytes;
    layoutOut.alignmentBytes = nested.alignmentBytes;
    return true;
  };

  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def)) {
      continue;
    }
    IrStructLayout layout;
    if (!computeStructLayout(def, layout)) {
      return false;
    }
    out.structLayouts.push_back(std::move(layout));
  }

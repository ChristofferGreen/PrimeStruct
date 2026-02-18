
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
    return name == "public" || name == "private" || name == "package" || name == "static" || name == "mut" ||
           name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "pod" ||
           name == "handle" || name == "gpu_lane";
  };
  auto getBindingTypeForLayout = [&](const Expr &expr, FieldBinding &bindingOut) {
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
        return;
      }
      bindingOut.typeName = transform.name;
      bindingOut.typeTemplateArg.clear();
      return;
    }
    if (bindingOut.typeName.empty()) {
      bindingOut.typeName = "int";
    }
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
    IrStructVisibility visibility = IrStructVisibility::Private;
    for (const auto &transform : expr.transforms) {
      if (transform.name == "public") {
        visibility = IrStructVisibility::Public;
      } else if (transform.name == "package") {
        visibility = IrStructVisibility::Package;
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
    uint32_t staticOffset = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      FieldBinding binding;
      getBindingTypeForLayout(stmt, binding);
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
      const bool fieldIsStatic = isStaticField(stmt);
      uint32_t &activeOffset = fieldIsStatic ? staticOffset : offset;
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
      field.isStatic = fieldIsStatic;
      layout.fields.push_back(std::move(field));
      activeOffset = alignedOffset + typeLayout.sizeBytes;
      if (!fieldIsStatic) {
        structAlign = std::max(structAlign, fieldAlign);
      }
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

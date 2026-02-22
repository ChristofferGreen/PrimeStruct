bool IrLowerer::lower(const Program &program,
                      const std::string &entryPath,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects,
                      IrModule &out,
                      std::string &error) const {
  out = IrModule{};

  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "native backend requires entry definition " + entryPath;
    return false;
  }

  auto isSoftwareNumericName = [](const std::string &name) {
    return name == "integer" || name == "decimal" || name == "complex";
  };
  auto splitTopLevelTemplateArgs = [](const std::string &text, std::vector<std::string> &out) -> bool {
    out.clear();
    int depth = 0;
    size_t start = 0;
    auto pushSegment = [&](size_t end) {
      size_t segStart = start;
      while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
        ++segStart;
      }
      size_t segEnd = end;
      while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
        --segEnd;
      }
      out.push_back(text.substr(segStart, segEnd - segStart));
    };
    for (size_t i = 0; i < text.size(); ++i) {
      char c = text[i];
      if (c == '<') {
        ++depth;
        continue;
      }
      if (c == '>') {
        if (depth > 0) {
          --depth;
        }
        continue;
      }
      if (c == ',' && depth == 0) {
        pushSegment(i);
        start = i + 1;
      }
    }
    pushSegment(text.size());
    for (const auto &seg : out) {
      if (seg.empty()) {
        return false;
      }
    }
    return !out.empty();
  };
  std::function<std::string(const std::string &)> findSoftwareNumericType;
  findSoftwareNumericType = [&](const std::string &typeName) -> std::string {
    if (typeName.empty()) {
      return {};
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(typeName, base, arg)) {
      return isSoftwareNumericName(typeName) ? typeName : std::string{};
    }
    if (isSoftwareNumericName(base)) {
      return base;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return {};
    }
    for (const auto &nested : args) {
      std::string found = findSoftwareNumericType(nested);
      if (!found.empty()) {
        return found;
      }
    }
    return {};
  };
  auto scanTransforms = [&](const std::vector<Transform> &transforms) -> std::string {
    for (const auto &transform : transforms) {
      std::string found = findSoftwareNumericType(transform.name);
      if (!found.empty()) {
        return found;
      }
      for (const auto &arg : transform.templateArgs) {
        found = findSoftwareNumericType(arg);
        if (!found.empty()) {
          return found;
        }
      }
    }
    return {};
  };
  std::function<std::string(const Expr &)> scanExpr;
  scanExpr = [&](const Expr &expr) -> std::string {
    std::string found = scanTransforms(expr.transforms);
    if (!found.empty()) {
      return found;
    }
    for (const auto &arg : expr.templateArgs) {
      found = findSoftwareNumericType(arg);
      if (!found.empty()) {
        return found;
      }
    }
    for (const auto &arg : expr.args) {
      found = scanExpr(arg);
      if (!found.empty()) {
        return found;
      }
    }
    for (const auto &arg : expr.bodyArguments) {
      found = scanExpr(arg);
      if (!found.empty()) {
        return found;
      }
    }
    return {};
  };
  auto scanProgram = [&]() -> std::string {
    for (const auto &def : program.definitions) {
      std::string found = scanTransforms(def.transforms);
      if (!found.empty()) {
        return found;
      }
      for (const auto &param : def.parameters) {
        found = scanExpr(param);
        if (!found.empty()) {
          return found;
        }
      }
      for (const auto &stmt : def.statements) {
        found = scanExpr(stmt);
        if (!found.empty()) {
          return found;
        }
      }
      if (def.returnExpr.has_value()) {
        found = scanExpr(*def.returnExpr);
        if (!found.empty()) {
          return found;
        }
      }
    }
    for (const auto &exec : program.executions) {
      std::string found = scanTransforms(exec.transforms);
      if (!found.empty()) {
        return found;
      }
      for (const auto &arg : exec.arguments) {
        found = scanExpr(arg);
        if (!found.empty()) {
          return found;
        }
      }
      for (const auto &arg : exec.bodyArguments) {
        found = scanExpr(arg);
        if (!found.empty()) {
          return found;
        }
      }
    }
    return {};
  };
  if (std::string found = scanProgram(); !found.empty()) {
    error = "native backend does not support software numeric types: " + found;
    return false;
  }
  auto effectBitForName = [&](const std::string &name, uint64_t &outBit) -> bool {
    if (name == "io_out") {
      outBit = EffectIoOut;
      return true;
    }
    if (name == "io_err") {
      outBit = EffectIoErr;
      return true;
    }
    if (name == "heap_alloc") {
      outBit = EffectHeapAlloc;
      return true;
    }
    if (name == "pathspace_notify") {
      outBit = EffectPathSpaceNotify;
      return true;
    }
    if (name == "pathspace_insert") {
      outBit = EffectPathSpaceInsert;
      return true;
    }
    if (name == "pathspace_take") {
      outBit = EffectPathSpaceTake;
      return true;
    }
    if (name == "file_write") {
      outBit = EffectFileWrite;
      return true;
    }
    if (name == "gpu_dispatch") {
      outBit = EffectGpuDispatch;
      return true;
    }
    return false;
  };

  auto isSupportedEffect = [](const std::string &name) {
    return name == "io_out" || name == "io_err" || name == "heap_alloc" || name == "pathspace_notify" ||
           name == "pathspace_insert" || name == "pathspace_take" || name == "file_write" ||
           name == "gpu_dispatch";
  };
  auto resolveActiveEffects = [&](const std::vector<Transform> &transforms, bool isEntry) {
    std::unordered_set<std::string> effects;
    bool sawEffects = false;
    for (const auto &transform : transforms) {
      if (transform.name != "effects") {
        continue;
      }
      sawEffects = true;
      effects.clear();
      for (const auto &effect : transform.arguments) {
        effects.insert(effect);
      }
    }
    if (!sawEffects) {
      const auto &defaults = isEntry ? entryDefaultEffects : defaultEffects;
      for (const auto &effect : defaults) {
        effects.insert(effect);
      }
    }
    return effects;
  };
  auto validateEffectsTransforms =
      [&](const std::vector<Transform> &transforms, const std::string &context) -> bool {
    for (const auto &transform : transforms) {
      if (transform.name != "effects") {
        continue;
      }
      for (const auto &effect : transform.arguments) {
        if (!isSupportedEffect(effect)) {
          error = "native backend does not support effect: " + effect + " on " + context;
          return false;
        }
      }
    }
    return true;
  };
  auto validateActiveEffects =
      [&](const std::vector<Transform> &transforms, const std::string &context, bool isEntry) -> bool {
    const auto effects = resolveActiveEffects(transforms, isEntry);
    for (const auto &effect : effects) {
      if (!isSupportedEffect(effect)) {
        error = "native backend does not support effect: " + effect + " on " + context;
        return false;
      }
    }
    return true;
  };
  auto resolveEffectMask =
      [&](const std::vector<Transform> &transforms, bool isEntry, uint64_t &maskOut) -> bool {
    const auto effects = resolveActiveEffects(transforms, isEntry);
    maskOut = 0;
    for (const auto &effect : effects) {
      uint64_t bit = 0;
      if (!effectBitForName(effect, bit)) {
        error = "unsupported effect in metadata: " + effect;
        return false;
      }
      maskOut |= bit;
    }
    return true;
  };
  auto resolveCapabilityMask =
      [&](const std::vector<Transform> &transforms, const std::unordered_set<std::string> &effects, uint64_t &maskOut)
      -> bool {
    bool sawCapabilities = false;
    std::unordered_set<std::string> capabilities;
    for (const auto &transform : transforms) {
      if (transform.name != "capabilities") {
        continue;
      }
      if (sawCapabilities) {
        error = "duplicate capabilities transform on " + entryPath;
        return false;
      }
      sawCapabilities = true;
      capabilities.clear();
      for (const auto &arg : transform.arguments) {
        capabilities.insert(arg);
      }
    }
    if (!sawCapabilities) {
      capabilities = effects;
    }
    maskOut = 0;
    for (const auto &capability : capabilities) {
      uint64_t bit = 0;
      if (!effectBitForName(capability, bit)) {
        error = "unsupported capability in metadata: " + capability;
        return false;
      }
      maskOut |= bit;
    }
    return true;
  };
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  auto validateExprEffects = [&](const auto &self, const Expr &expr, const std::string &context) -> bool {
    if (!validateEffectsTransforms(expr.transforms, context)) {
      return false;
    }
    for (const auto &arg : expr.args) {
      if (!self(self, arg, context)) {
        return false;
      }
    }
    for (const auto &bodyArg : expr.bodyArguments) {
      if (!self(self, bodyArg, context)) {
        return false;
      }
    }
    return true;
  };
  for (const auto &def : program.definitions) {
    if (!validateActiveEffects(def.transforms, def.fullPath, def.fullPath == entryPath)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!validateExprEffects(validateExprEffects, param, def.fullPath)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!validateExprEffects(validateExprEffects, stmt, def.fullPath)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!validateExprEffects(validateExprEffects, *def.returnExpr, def.fullPath)) {
        return false;
      }
    }
  }
  for (const auto &exec : program.executions) {
    if (!validateActiveEffects(exec.transforms, exec.fullPath, false)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExprEffects(validateExprEffects, arg, exec.fullPath)) {
        return false;
      }
    }
    for (const auto &bodyArg : exec.bodyArguments) {
      if (!validateExprEffects(validateExprEffects, bodyArg, exec.fullPath)) {
        return false;
      }
    }
  }
  if (!resolveEffectMask(entryDef->transforms, true, entryEffectMask)) {
    return false;
  }
  if (!resolveCapabilityMask(entryDef->transforms, resolveActiveEffects(entryDef->transforms, true),
                             entryCapabilityMask)) {
    return false;
  }

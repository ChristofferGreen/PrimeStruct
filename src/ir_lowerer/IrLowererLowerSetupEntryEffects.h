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

  auto isSupportedEffect = [](const std::string &name) {
    return name == "io_out" || name == "io_err" || name == "heap_alloc" || name == "pathspace_notify" ||
           name == "pathspace_insert" || name == "pathspace_take" || name == "file_write";
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

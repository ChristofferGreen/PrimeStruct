bool IrLowerer::lower(const Program &program,
                      const std::string &entryPath,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects,
                      IrModule &out,
                      std::string &error) const {
  out = IrModule{};

  const Definition *entryDef = nullptr;
  if (!findEntryDefinition(program, entryPath, entryDef, error)) {
    return false;
  }

  if (!validateNoSoftwareNumericTypes(program, error)) {
    return false;
  }

  auto resolveActiveEffects = [&](const std::vector<Transform> &transforms, bool isEntry) {
    return ir_lowerer::resolveActiveEffects(transforms, isEntry, defaultEffects, entryDefaultEffects);
  };
  auto validateEffectsTransforms =
      [&](const std::vector<Transform> &transforms, const std::string &context) -> bool {
    return ir_lowerer::validateEffectsTransforms(transforms, context, error);
  };
  auto validateActiveEffects =
      [&](const std::vector<Transform> &transforms, const std::string &context, bool isEntry) -> bool {
    return ir_lowerer::validateActiveEffects(
        transforms, context, isEntry, defaultEffects, entryDefaultEffects, error);
  };
  auto resolveEffectMask =
      [&](const std::vector<Transform> &transforms, bool isEntry, uint64_t &maskOut) -> bool {
    return ir_lowerer::resolveEffectMask(
        transforms, isEntry, defaultEffects, entryDefaultEffects, maskOut, error);
  };
  auto resolveCapabilityMask =
      [&](const std::vector<Transform> &transforms, const std::unordered_set<std::string> &effects, uint64_t &maskOut)
      -> bool {
    return ir_lowerer::resolveCapabilityMask(transforms, effects, entryPath, maskOut, error);
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

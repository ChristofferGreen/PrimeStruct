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
  if (!ir_lowerer::validateProgramEffects(program, entryPath, defaultEffects, entryDefaultEffects, error)) {
    return false;
  }
  if (!resolveEffectMask(entryDef->transforms, true, entryEffectMask)) {
    return false;
  }
  if (!resolveCapabilityMask(entryDef->transforms, resolveActiveEffects(entryDef->transforms, true),
                             entryCapabilityMask)) {
    return false;
  }

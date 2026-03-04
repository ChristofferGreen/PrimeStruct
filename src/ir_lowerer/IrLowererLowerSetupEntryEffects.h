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

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  if (!ir_lowerer::validateProgramEffects(program, entryPath, defaultEffects, entryDefaultEffects, error)) {
    return false;
  }
  if (!ir_lowerer::resolveEntryMetadataMasks(*entryDef,
                                             entryPath,
                                             defaultEffects,
                                             entryDefaultEffects,
                                             entryEffectMask,
                                             entryCapabilityMask,
                                             error)) {
    return false;
  }

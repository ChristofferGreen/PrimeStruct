bool IrLowerer::lower(const Program &program,
                      const std::string &entryPath,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects,
                      IrModule &out,
                      std::string &error) const {
  out = IrModule{};

  const Definition *entryDef = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  if (!ir_lowerer::runLowerEntrySetup(program,
                                      entryPath,
                                      defaultEffects,
                                      entryDefaultEffects,
                                      entryDef,
                                      entryEffectMask,
                                      entryCapabilityMask,
                                      error)) {
    return false;
  }

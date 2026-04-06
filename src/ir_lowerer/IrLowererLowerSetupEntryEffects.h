bool IrLowerer::lower(const Program &program,
                      const SemanticProgram *semanticProgram,
                      const std::string &entryPath,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects,
                      IrModule &out,
                      std::string &error,
                      DiagnosticSinkReport *diagnosticInfo) const {
  out = IrModule{};
  error.clear();
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();
  bool loweringSucceeded = false;
  struct LoweringDiagnosticScope {
    DiagnosticSink &diagnosticSink;
    std::string &error;
    bool &loweringSucceeded;

    ~LoweringDiagnosticScope() {
      if (!loweringSucceeded && !error.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
  } loweringDiagnosticScope{diagnosticSink, error, loweringSucceeded};

  if (semanticProgram == nullptr) {
    error = "semantic product is required for IR lowering";
    return false;
  }

  const Definition *entryDef = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  if (!ir_lowerer::runLowerEntrySetup(program,
                                      semanticProgram,
                                      entryPath,
                                      defaultEffects,
                                      entryDefaultEffects,
                                      entryDef,
                                      entryEffectMask,
                                      entryCapabilityMask,
                                      error)) {
    return false;
  }

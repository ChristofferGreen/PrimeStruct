




bool findEntryDefinition(const Program &program,
                         const std::string &entryPath,
                         const Definition *&entryDefOut,
                         std::string &error);

bool validateNoSoftwareNumericTypes(const Program &program, std::string &error);
bool validateNoRuntimeReflectionQueries(const Program &program, std::string &error);

bool effectBitForName(const std::string &name, uint64_t &outBit);
bool isSupportedEffect(const std::string &name);

std::unordered_set<std::string> resolveActiveEffects(const std::vector<Transform> &transforms,
                                                     bool isEntry,
                                                     const std::vector<std::string> &defaultEffects,
                                                     const std::vector<std::string> &entryDefaultEffects);

bool validateEffectsTransforms(const std::vector<Transform> &transforms,
                               const std::string &context,
                               std::string &error);

bool validateActiveEffects(const std::vector<Transform> &transforms,
                           const std::string &context,
                           bool isEntry,
                           const std::vector<std::string> &defaultEffects,
                           const std::vector<std::string> &entryDefaultEffects,
                           std::string &error);

bool validateProgramEffects(const Program &program,
                            const std::string &entryPath,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects,
                            std::string &error);

bool validateProgramEffects(const Program &program,
                            const SemanticProgram *semanticProgram,
                            const std::string &entryPath,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects,
                            std::string &error);

bool resolveEntryMetadataMasks(const Definition &entryDef,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               uint64_t &entryEffectMaskOut,
                               uint64_t &entryCapabilityMaskOut,
                               std::string &error);
bool resolveEntryMetadataMasks(const Definition &entryDef,
                               const SemanticProgram *semanticProgram,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               uint64_t &entryEffectMaskOut,
                               uint64_t &entryCapabilityMaskOut,
                               std::string &error);

bool resolveEffectMask(const std::vector<Transform> &transforms,
                       bool isEntry,
                       const std::vector<std::string> &defaultEffects,
                       const std::vector<std::string> &entryDefaultEffects,
                       uint64_t &maskOut,
                       std::string &error);

bool resolveCapabilityMask(const std::vector<Transform> &transforms,
                           const std::unordered_set<std::string> &effects,
                           const std::string &duplicateContext,
                           uint64_t &maskOut,
                           std::string &error);

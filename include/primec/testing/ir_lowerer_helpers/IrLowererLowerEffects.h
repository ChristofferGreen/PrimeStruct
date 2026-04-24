




struct Definition;
struct Program;
struct SemanticProgram;
struct Transform;

bool findEntryDefinition(const ::primec::Program &program,
                         const std::string &entryPath,
                         const ::primec::Definition *&entryDefOut,
                         std::string &error);

bool validateNoSoftwareNumericTypesForBackendSurface(const ::primec::SemanticProgram *semanticProgram,
                                                     std::string_view backendSurfaceName,
                                                     std::string &error);
bool validateNoRuntimeReflectionQueriesForBackendSurface(const ::primec::SemanticProgram *semanticProgram,
                                                         std::string_view backendSurfaceName,
                                                         std::string &error);

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

bool validateEffectsTransformsForBackendSurface(const std::vector<Transform> &transforms,
                                                const std::string &context,
                                                std::string_view backendSurfaceName,
                                                std::string &error);

bool validateActiveEffectsForBackendSurface(const std::vector<Transform> &transforms,
                                            const std::string &context,
                                            bool isEntry,
                                            const std::vector<std::string> &defaultEffects,
                                            const std::vector<std::string> &entryDefaultEffects,
                                            std::string_view backendSurfaceName,
                                            std::string &error);

bool validateProgramEffectsForBackendSurface(const ::primec::Program &program,
                                             const ::primec::SemanticProgram *semanticProgram,
                                             const std::string &entryPath,
                                             const std::vector<std::string> &defaultEffects,
                                             const std::vector<std::string> &entryDefaultEffects,
                                             std::string_view backendSurfaceName,
                                             std::string &error);

bool resolveEntryMetadataMasks(const ::primec::Definition &entryDef,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               uint64_t &entryEffectMaskOut,
                               uint64_t &entryCapabilityMaskOut,
                               std::string &error);
bool resolveEntryMetadataMasks(const ::primec::Definition &entryDef,
                               const ::primec::SemanticProgram *semanticProgram,
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

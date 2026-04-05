



struct Program;
struct Definition;
struct SemanticProgram;

using OnErrorByDefinition = std::unordered_map<std::string, std::optional<OnErrorHandler>>;
struct EntryCallOnErrorSetup {
  CallResolutionAdapters callResolutionAdapters{};
  bool hasTailExecution = false;
  OnErrorByDefinition onErrorByDefinition{};
};
struct EntryCountCallOnErrorSetup {
  EntryCountAccessSetup countAccessSetup{};
  EntryCallOnErrorSetup callOnErrorSetup{};
};

bool parseTransformArgumentExpr(const std::string &text,
                                const std::string &namespacePrefix,
                                Expr &out,
                                std::string &error);

bool parseOnErrorTransform(const std::vector<Transform> &transforms,
                           const std::string &namespacePrefix,
                           const std::string &context,
                           const ResolveExprPathFn &resolveExprPath,
                           const DefinitionExistsFn &definitionExists,
                           std::optional<OnErrorHandler> &out,
                           std::string &error);

bool buildOnErrorByDefinition(const Program &program,
                              const ResolveExprPathFn &resolveExprPath,
                              const DefinitionExistsFn &definitionExists,
                              OnErrorByDefinition &out,
                              std::string &error);
bool buildOnErrorByDefinition(const Program &program,
                              const SemanticProgram *semanticProgram,
                              const ResolveExprPathFn &resolveExprPath,
                              const DefinitionExistsFn &definitionExists,
                              OnErrorByDefinition &out,
                              std::string &error);
bool buildOnErrorByDefinitionFromCallResolutionAdapters(
    const Program &program,
    const CallResolutionAdapters &callResolutionAdapters,
    OnErrorByDefinition &out,
    std::string &error);
bool buildOnErrorByDefinitionFromCallResolutionAdapters(
    const Program &program,
    const SemanticProgram *semanticProgram,
    const CallResolutionAdapters &callResolutionAdapters,
    OnErrorByDefinition &out,
    std::string &error);
bool buildEntryCallOnErrorSetup(const Program &program,
                                const Definition &entryDef,
                                bool definitionReturnsVoid,
                                const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::unordered_map<std::string, std::string> &importAliases,
                                EntryCallOnErrorSetup &out,
                                std::string &error);
bool buildEntryCallOnErrorSetup(const Program &program,
                                const Definition &entryDef,
                                bool definitionReturnsVoid,
                                const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::unordered_map<std::string, std::string> &importAliases,
                                const SemanticProgram *semanticProgram,
                                EntryCallOnErrorSetup &out,
                                std::string &error);
bool buildEntryCountCallOnErrorSetup(const Program &program,
                                     const Definition &entryDef,
                                     bool definitionReturnsVoid,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const std::unordered_map<std::string, std::string> &importAliases,
                                     EntryCountCallOnErrorSetup &out,
                                     std::string &error);
bool buildEntryCountCallOnErrorSetup(const Program &program,
                                     const Definition &entryDef,
                                     bool definitionReturnsVoid,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const std::unordered_map<std::string, std::string> &importAliases,
                                     const SemanticProgram *semanticProgram,
                                     EntryCountCallOnErrorSetup &out,
                                     std::string &error);

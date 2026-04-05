




struct Definition;
struct IrFunction;
struct LayoutFieldBinding;
struct Program;
struct SemanticProgram;

bool runLowerLocalsSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    SetupLocalsOrchestration &setupLocalsOrchestrationOut,
    std::string &errorOut);
bool runLowerLocalsSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const SemanticProgram *semanticProgram,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    SetupLocalsOrchestration &setupLocalsOrchestrationOut,
    std::string &errorOut);

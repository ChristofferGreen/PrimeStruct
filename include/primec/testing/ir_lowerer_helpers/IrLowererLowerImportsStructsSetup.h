




struct Definition;
struct LayoutFieldBinding;
struct Program;
struct SemanticProgram;

bool runLowerImportsStructsSetup(
    const Program &program,
    const SemanticProgram *semanticProgram,
    IrModule &outModule,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    std::unordered_map<std::string, std::string> &importAliasesOut,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByNameOut,
    std::string &errorOut);

inline bool runLowerImportsStructsSetup(
    const Program &program,
    IrModule &outModule,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    std::unordered_map<std::string, std::string> &importAliasesOut,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByNameOut,
    std::string &errorOut) {
  return runLowerImportsStructsSetup(program,
                                     nullptr,
                                     outModule,
                                     defMapOut,
                                     structNamesOut,
                                     importAliasesOut,
                                     structFieldInfoByNameOut,
                                     errorOut);
}

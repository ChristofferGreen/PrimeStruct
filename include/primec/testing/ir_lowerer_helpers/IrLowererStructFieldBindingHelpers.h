
struct LayoutFieldBinding {
  std::string typeName;
  std::string typeTemplateArg;
  std::string name;

  LayoutFieldBinding() = default;
  LayoutFieldBinding(const std::string &typeNameIn, const std::string &typeTemplateArgIn)
      : typeName(typeNameIn), typeTemplateArg(typeTemplateArgIn) {}
  LayoutFieldBinding(const std::string &typeNameIn,
                     const std::string &typeTemplateArgIn,
                     const std::string &nameIn)
      : typeName(typeNameIn), typeTemplateArg(typeTemplateArgIn), name(nameIn) {}
};

const Expr *getEnvelopeValueExpr(const Expr &candidate, bool allowAnyName);
bool extractExplicitLayoutFieldBinding(const Expr &expr, LayoutFieldBinding &bindingOut);
bool inferPrimitiveFieldBinding(const Expr &initializer,
                                const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
                                LayoutFieldBinding &bindingOut);
bool resolveLayoutFieldBinding(
    const Definition &def,
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    LayoutFieldBinding &bindingOut,
    std::string &errorOut);
bool collectStructLayoutFieldBindings(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const SemanticProgram *semanticProgram,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut);
inline bool collectStructLayoutFieldBindings(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  return collectStructLayoutFieldBindings(program,
                                          structNames,
                                          resolveStructTypePath,
                                          resolveStructLayoutExprPath,
                                          defMap,
                                          nullptr,
                                          fieldsByStructOut,
                                          errorOut);
}
bool collectStructLayoutFieldBindingsFromProgramContext(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut);
inline bool collectStructLayoutFieldBindingsFromProgramContext(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  return collectStructLayoutFieldBindingsFromProgramContext(program,
                                                            structNames,
                                                            resolveStructTypePath,
                                                            defMap,
                                                            importAliases,
                                                            nullptr,
                                                            fieldsByStructOut,
                                                            errorOut);
}
bool resolveStructLayoutFieldBinding(
    const std::string &structPath,
    const std::string &fieldName,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStruct,
    const std::unordered_map<std::string, const Definition *> &defMap,
    LayoutFieldBinding &bindingOut);
std::string formatLayoutFieldEnvelope(const LayoutFieldBinding &binding);

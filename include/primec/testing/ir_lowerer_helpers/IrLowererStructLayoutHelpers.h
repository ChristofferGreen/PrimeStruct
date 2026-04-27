



struct LayoutFieldBinding;

struct BindingTypeLayout {
  uint32_t sizeBytes = 0;
  uint32_t alignmentBytes = 1;
};

uint32_t alignTo(uint32_t value, uint32_t alignment);
bool parsePositiveInt(const std::string &text, int &valueOut);
bool extractAlignment(const std::vector<Transform> &transforms,
                      const std::string &context,
                      uint32_t &alignmentOut,
                      bool &hasAlignment,
                      std::string &error);
bool classifyBindingTypeLayout(const LayoutFieldBinding &binding,
                               BindingTypeLayout &layoutOut,
                               std::string &structTypeNameOut,
                               std::string &errorOut);
bool resolveBindingTypeLayout(
    const LayoutFieldBinding &binding,
    const std::string &namespacePrefix,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &, std::string &)> &computeStructLayout,
    BindingTypeLayout &layoutOut,
    std::string &errorOut);
bool computeStructLayoutWithCache(
    const std::string &structPath,
    std::unordered_map<std::string, IrStructLayout> &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    const std::function<bool(IrStructLayout &, std::string &)> &computeUncachedLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool validateSemanticProductStructLayoutCoverage(const ::primec::Program &program,
                                                 const ::primec::SemanticProgram *semanticProgram,
                                                 std::string &errorOut);
bool computeStructLayoutUncached(
    const ::primec::Definition &def,
    const std::vector<LayoutFieldBinding> &fieldBindings,
    const std::function<bool(const LayoutFieldBinding &, BindingTypeLayout &, std::string &)> &resolveFieldTypeLayout,
    const SemanticProgramTypeMetadata *typeMetadata,
    IrStructLayout &layoutOut,
    std::string &errorOut);
inline bool computeStructLayoutUncached(
    const ::primec::Definition &def,
    const std::vector<LayoutFieldBinding> &fieldBindings,
    const std::function<bool(const LayoutFieldBinding &, BindingTypeLayout &, std::string &)> &resolveFieldTypeLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  return computeStructLayoutUncached(def, fieldBindings, resolveFieldTypeLayout, nullptr, layoutOut, errorOut);
}
bool computeStructLayoutFromFieldInfo(
    const ::primec::Definition &def,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    const ::primec::SemanticProgram *semanticProgram,
    IrStructLayout &layoutOut,
    std::string &errorOut);
inline bool computeStructLayoutFromFieldInfo(
    const ::primec::Definition &def,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  return computeStructLayoutFromFieldInfo(
      def, structFieldInfoByName, resolveStructTypePath, defMap, computeStructLayout, nullptr, layoutOut, errorOut);
}
bool appendProgramStructLayouts(
    const ::primec::Program &program,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const ::primec::SemanticProgram *semanticProgram,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut);
inline bool appendProgramStructLayouts(
    const ::primec::Program &program,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut) {
  return appendProgramStructLayouts(program, defMap, nullptr, computeStructLayout, layoutsOut, errorOut);
}
inline bool appendProgramStructLayouts(
    const ::primec::Program &program,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut) {
  std::unordered_map<std::string, const ::primec::Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }
  return appendProgramStructLayouts(program, defMap, nullptr, computeStructLayout, layoutsOut, errorOut);
}
bool appendStructLayoutField(const std::string &structPath,
                             const Expr &fieldExpr,
                             const LayoutFieldBinding &binding,
                             const std::function<bool(const LayoutFieldBinding &,
                                                      BindingTypeLayout &,
                                                      std::string &)> &resolveTypeLayout,
                             uint32_t &offsetInOut,
                             uint32_t &structAlignmentInOut,
                             IrStructLayout &layoutOut,
                             std::string &errorOut);
bool isLayoutQualifierName(const std::string &name);
IrStructFieldCategory fieldCategory(const Expr &expr);
IrStructVisibility fieldVisibility(const Expr &expr);
bool isStaticField(const Expr &expr);

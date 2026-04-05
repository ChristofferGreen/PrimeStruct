



struct Definition;

struct InstructionSourceRange {
  size_t beginIndex = 0;
  size_t endIndex = 0;
  uint32_t line = 0;
  uint32_t column = 0;
  IrSourceMapProvenance provenance = IrSourceMapProvenance::CanonicalAst;
};

struct LowerStatementsSourceMapStepInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, std::vector<InstructionSourceRange>> *instructionSourceRangesByFunction = nullptr;
  std::vector<std::string> *stringTable = nullptr;
  IrModule *outModule = nullptr;
};

bool runLowerStatementsSourceMapStep(const LowerStatementsSourceMapStepInput &input,
                                     std::string &errorOut);

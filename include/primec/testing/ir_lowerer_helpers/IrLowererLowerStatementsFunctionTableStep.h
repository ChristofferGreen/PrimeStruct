


struct ReturnInfo;

struct LowerStatementsFunctionTableStepInput {
  const Program *program = nullptr;
  const Definition *entryDef = nullptr;
  const SemanticProgram *semanticProgram = nullptr;
  IrFunction *function = nullptr;
  const std::unordered_set<std::string> *loweredCallTargets = nullptr;

  std::function<bool(const Definition &)> isStructDefinition;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  const std::vector<std::string> *defaultEffects = nullptr;
  const std::vector<std::string> *entryDefaultEffects = nullptr;
  std::function<bool(const Expr &)> isTailCallCandidate;

  std::function<void()> resetDefinitionLoweringState;
  std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> buildDefinitionCallContext;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;

  int32_t *nextLocal = nullptr;
  std::vector<IrFunction> *outFunctions = nullptr;
  int32_t *entryIndex = nullptr;
};

bool runLowerStatementsFunctionTableStep(const LowerStatementsFunctionTableStepInput &input,
                                         std::string &errorOut);

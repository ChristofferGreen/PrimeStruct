



struct Definition;

struct LowerStatementsEntryExecutionStepInput {
  const Definition *entryDef = nullptr;
  bool returnsVoid = false;
  bool *sawReturn = nullptr;

  const std::unordered_map<std::string, std::optional<OnErrorHandler>> *onErrorByDef = nullptr;
  std::optional<OnErrorHandler> *currentOnError = nullptr;
  std::optional<ResultReturnInfo> *currentReturnResult = nullptr;

  bool entryHasResultInfo = false;
  const ResultReturnInfo *entryResultInfo = nullptr;

  std::function<bool(const Expr &)> emitEntryStatement;
  std::function<void()> pushFileScope;
  std::function<void()> emitCurrentFileScopeCleanup;
  std::function<void()> popFileScope;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsEntryExecutionStep(const LowerStatementsEntryExecutionStepInput &input,
                                          std::string &errorOut);

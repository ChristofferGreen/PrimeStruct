


struct Definition;
struct ReturnInfo;

struct LowerStatementsCallsStepInput {
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<int32_t()> allocTempLocal;

  std::function<std::string(const Expr &)> resolveExprPath;
  std::function<const Definition *(const std::string &)> findDefinitionByPath;

  std::function<bool(const Expr &, const LocalMap &)> isArrayCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isStringCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isVectorCapacityCall;
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut);

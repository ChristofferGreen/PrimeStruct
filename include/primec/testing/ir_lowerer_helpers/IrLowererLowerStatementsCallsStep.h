


struct ReturnInfo;

struct LowerStatementsCallsStepInput {
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<std::string(const Expr &, const LocalMap &)> inferStructExprPath;
  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<int32_t()> allocTempLocal;

  std::function<std::string(const Expr &)> resolveExprPath;
  std::function<const ::primec::Definition *(const std::string &)> findDefinitionByPath;
  std::function<const ::primec::Definition *(const std::string &)> resolveDestroyHelperForStruct;
  std::function<const ::primec::Definition *(const std::string &)> resolveMoveHelperForStruct;

  std::function<bool(const Expr &, const LocalMap &)> isArrayCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isStringCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isVectorCapacityCall;
  std::function<const ::primec::Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const ::primec::Definition *(const Expr &)> resolveDefinitionCall;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<bool(const Expr &, const ::primec::Definition &, const LocalMap &, bool)>
      emitInlineDefinitionCall;
  std::function<void()> emitArrayIndexOutOfBounds;
  std::function<void()> emitVectorCapacityExceeded;
  std::function<void()> emitVectorPopOnEmpty;
  std::function<void()> emitVectorIndexOutOfBounds;
  std::function<void()> emitVectorReserveNegative;
  std::function<void()> emitVectorReserveExceeded;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut);

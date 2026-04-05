



struct LowerStatementsEntryStatementStepInput {
  IrFunction *function = nullptr;
  std::function<bool(const Expr &)> emitStatement;
  std::function<void(const std::string &, const Expr &, size_t, size_t)> appendInstructionSourceRange;
};

bool runLowerStatementsEntryStatementStep(const LowerStatementsEntryStatementStepInput &input,
                                          const Expr &stmt,
                                          std::string &errorOut);

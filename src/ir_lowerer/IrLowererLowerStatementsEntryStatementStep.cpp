#include "IrLowererLowerStatementsEntryStatementStep.h"

namespace primec::ir_lowerer {

bool runLowerStatementsEntryStatementStep(const LowerStatementsEntryStatementStepInput &input,
                                          const Expr &stmt,
                                          std::string &errorOut) {
  if (input.function == nullptr) {
    errorOut = "native backend missing statements entry-statement step dependency: function";
    return false;
  }
  if (!input.emitStatement) {
    errorOut = "native backend missing statements entry-statement step dependency: emitStatement";
    return false;
  }
  if (!input.appendInstructionSourceRange) {
    errorOut = "native backend missing statements entry-statement step dependency: appendInstructionSourceRange";
    return false;
  }

  const size_t startInstructionIndex = input.function->instructions.size();
  if (!input.emitStatement(stmt)) {
    return false;
  }
  input.appendInstructionSourceRange(
      input.function->name, stmt, startInstructionIndex, input.function->instructions.size());
  return true;
}

} // namespace primec::ir_lowerer

#include "primec/ir_lowerer/IrLowererLowerStatementsFunctionTableStep.h"

namespace primec::ir_lowerer {

bool runLowerStatementsFunctionTableStep(const LowerStatementsFunctionTableStepInput &input,
                                         std::string &errorOut) {
  if (input.function == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: function";
    return false;
  }
  if (input.outFunctions == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: outFunctions";
    return false;
  }
  if (input.entryIndex == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: entryIndex";
    return false;
  }

  finalizeEntryFunctionTable(*input.function, *input.outFunctions, *input.entryIndex);
  return true;
}

} // namespace primec::ir_lowerer

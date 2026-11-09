#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerStatementsFunctionTableStepInput {
  IrFunction *function = nullptr;
  std::vector<IrFunction> *outFunctions = nullptr;
  int32_t *entryIndex = nullptr;
};

bool runLowerStatementsFunctionTableStep(const LowerStatementsFunctionTableStepInput &input,
                                         std::string &errorOut);

} // namespace primec::ir_lowerer

#pragma once

#include <cstdint>
#include <string>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerInlineCallReturnValueStepInput {
  IrFunction *function = nullptr;
  bool returnsVoid = true;
  int32_t returnLocal = -1;
  bool structDefinition = false;
  bool requireValue = false;
};

bool runLowerInlineCallReturnValueStep(const LowerInlineCallReturnValueStepInput &input,
                                       std::string &errorOut);

} // namespace primec::ir_lowerer

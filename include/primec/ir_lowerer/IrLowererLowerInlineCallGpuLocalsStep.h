#pragma once

#include <string>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerInlineCallGpuLocalsStepInput {
  const LocalMap *callerLocals = nullptr;
  LocalMap *calleeLocals = nullptr;
};

bool runLowerInlineCallGpuLocalsStep(const LowerInlineCallGpuLocalsStepInput &input,
                                     std::string &errorOut);

} // namespace primec::ir_lowerer

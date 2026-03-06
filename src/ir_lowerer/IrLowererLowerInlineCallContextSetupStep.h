#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerInlineCallContextSetupStepInput {
  IrFunction *function = nullptr;
  const ReturnInfo *returnInfo = nullptr;
  std::function<int32_t()> allocTempLocal;
};

struct LowerInlineCallContextSetupStepOutput {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  int32_t returnLocal = -1;
};

bool runLowerInlineCallContextSetupStep(const LowerInlineCallContextSetupStepInput &input,
                                        LowerInlineCallContextSetupStepOutput &output,
                                        std::string &errorOut);

} // namespace primec::ir_lowerer

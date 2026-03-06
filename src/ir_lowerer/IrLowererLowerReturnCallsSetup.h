#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererRuntimeErrorHelpers.h"

namespace primec {

struct IrFunction;

namespace ir_lowerer {

using LowerReturnCallsEmitFileErrorWhyFn = std::function<bool(int32_t)>;

struct LowerReturnCallsSetupInput {
  IrFunction *function = nullptr;
  InternRuntimeErrorStringFn internString;
};

bool runLowerReturnCallsSetup(const LowerReturnCallsSetupInput &input,
                              LowerReturnCallsEmitFileErrorWhyFn &emitFileErrorWhyOut,
                              std::string &errorOut);

} // namespace ir_lowerer
} // namespace primec

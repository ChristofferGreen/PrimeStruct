#include "IrLowererLowerReturnCallsSetup.h"

#include "primec/Ir.h"

namespace primec::ir_lowerer {

bool runLowerReturnCallsSetup(const LowerReturnCallsSetupInput &input,
                              LowerReturnCallsEmitFileErrorWhyFn &emitFileErrorWhyOut,
                              std::string &errorOut) {
  emitFileErrorWhyOut = {};
  if (input.function == nullptr) {
    errorOut = "native backend missing return/calls setup dependency: function";
    return false;
  }
  if (!input.internString) {
    errorOut = "native backend missing return/calls setup dependency: internString";
    return false;
  }

  IrFunction *function = input.function;
  const auto internString = input.internString;
  emitFileErrorWhyOut = [function, internString](int32_t errorLocal) {
    ir_lowerer::emitFileErrorWhy(*function, errorLocal, internString);
    return true;
  };
  return true;
}

} // namespace primec::ir_lowerer

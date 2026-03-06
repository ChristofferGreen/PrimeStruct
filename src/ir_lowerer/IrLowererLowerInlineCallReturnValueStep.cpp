#include "IrLowererLowerInlineCallReturnValueStep.h"

namespace primec::ir_lowerer {

bool runLowerInlineCallReturnValueStep(const LowerInlineCallReturnValueStepInput &input,
                                       std::string &errorOut) {
  if (input.function == nullptr) {
    errorOut = "native backend missing inline-call return-value step dependency: function";
    return false;
  }
  if (!input.returnsVoid && input.returnLocal < 0) {
    errorOut = "native backend missing inline-call return-value step dependency: returnLocal";
    return false;
  }

  if (!input.returnsVoid) {
    input.function->instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(input.returnLocal)});
  }
  if (input.structDefinition && input.requireValue && input.returnsVoid) {
    // VM/native treat struct constructor calls as void; synthesize a value for expression contexts.
    input.function->instructions.push_back({IrOpcode::PushI32, 0});
  }
  return true;
}

} // namespace primec::ir_lowerer

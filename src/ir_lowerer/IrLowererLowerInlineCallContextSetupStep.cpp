#include "IrLowererLowerInlineCallContextSetupStep.h"

namespace primec::ir_lowerer {

bool runLowerInlineCallContextSetupStep(const LowerInlineCallContextSetupStepInput &input,
                                        LowerInlineCallContextSetupStepOutput &output,
                                        std::string &errorOut) {
  if (input.function == nullptr) {
    errorOut = "native backend missing inline-call context-setup step dependency: function";
    return false;
  }
  if (input.returnInfo == nullptr) {
    errorOut = "native backend missing inline-call context-setup step dependency: returnInfo";
    return false;
  }
  if (!input.returnInfo->returnsVoid && !input.allocTempLocal) {
    errorOut = "native backend missing inline-call context-setup step dependency: allocTempLocal";
    return false;
  }

  output.returnsVoid = input.returnInfo->returnsVoid;
  output.returnsArray = input.returnInfo->returnsArray;
  output.returnKind = input.returnInfo->kind;
  output.returnLocal = -1;

  if (!output.returnsVoid) {
    output.returnLocal = input.allocTempLocal();
    IrOpcode zeroOp = IrOpcode::PushI32;
    if (output.returnsArray || output.returnKind == LocalInfo::ValueKind::Int64 ||
        output.returnKind == LocalInfo::ValueKind::UInt64 || output.returnKind == LocalInfo::ValueKind::String) {
      zeroOp = IrOpcode::PushI64;
    } else if (output.returnKind == LocalInfo::ValueKind::Float64) {
      zeroOp = IrOpcode::PushF64;
    } else if (output.returnKind == LocalInfo::ValueKind::Float32) {
      zeroOp = IrOpcode::PushF32;
    }
    input.function->instructions.push_back({zeroOp, 0});
    input.function->instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(output.returnLocal)});
  }
  return true;
}

} // namespace primec::ir_lowerer

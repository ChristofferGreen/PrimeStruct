#include "IrLowererLowerStatementsFunctionTableStep.h"

namespace primec::ir_lowerer {

bool runLowerStatementsFunctionTableStep(const LowerStatementsFunctionTableStepInput &input,
                                         std::string &errorOut) {
  if (input.program == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: program";
    return false;
  }
  if (input.entryDef == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: entryDef";
    return false;
  }
  if (input.function == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: function";
    return false;
  }
  if (input.loweredCallTargets == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: loweredCallTargets";
    return false;
  }
  if (!input.isStructDefinition) {
    errorOut = "native backend missing statements function-table step dependency: isStructDefinition";
    return false;
  }
  if (!input.getReturnInfo) {
    errorOut = "native backend missing statements function-table step dependency: getReturnInfo";
    return false;
  }
  if (input.defaultEffects == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: defaultEffects";
    return false;
  }
  if (input.entryDefaultEffects == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: entryDefaultEffects";
    return false;
  }
  if (!input.isTailCallCandidate) {
    errorOut = "native backend missing statements function-table step dependency: isTailCallCandidate";
    return false;
  }
  if (!input.resetDefinitionLoweringState) {
    errorOut = "native backend missing statements function-table step dependency: resetDefinitionLoweringState";
    return false;
  }
  if (!input.buildDefinitionCallContext) {
    errorOut = "native backend missing statements function-table step dependency: buildDefinitionCallContext";
    return false;
  }
  if (!input.emitInlineDefinitionCall) {
    errorOut = "native backend missing statements function-table step dependency: emitInlineDefinitionCall";
    return false;
  }
  if (input.nextLocal == nullptr) {
    errorOut = "native backend missing statements function-table step dependency: nextLocal";
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

  const auto functionTableResult = finalizeEntryFunctionTableAndLowerCallables(
      *input.program,
      *input.entryDef,
      *input.function,
      *input.loweredCallTargets,
      input.isStructDefinition,
      input.getReturnInfo,
      *input.defaultEffects,
      *input.entryDefaultEffects,
      input.isTailCallCandidate,
      input.resetDefinitionLoweringState,
      input.buildDefinitionCallContext,
      input.emitInlineDefinitionCall,
      *input.nextLocal,
      *input.outFunctions,
      *input.entryIndex,
      errorOut);
  return functionTableResult == FunctionTableFinalizationResult::Emitted;
}

} // namespace primec::ir_lowerer

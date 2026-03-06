#include "IrLowererLowerStatementsEntryExecutionStep.h"

namespace primec::ir_lowerer {

bool runLowerStatementsEntryExecutionStep(const LowerStatementsEntryExecutionStepInput &input,
                                          std::string &errorOut) {
  if (input.entryDef == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: entryDef";
    return false;
  }
  if (input.sawReturn == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: sawReturn";
    return false;
  }
  if (input.onErrorByDef == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: onErrorByDef";
    return false;
  }
  if (input.currentOnError == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: currentOnError";
    return false;
  }
  if (input.currentReturnResult == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: currentReturnResult";
    return false;
  }
  if (input.entryHasResultInfo && input.entryResultInfo == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: entryResultInfo";
    return false;
  }
  if (!input.emitEntryStatement) {
    errorOut = "native backend missing statements entry-execution step dependency: emitEntryStatement";
    return false;
  }
  if (!input.pushFileScope) {
    errorOut = "native backend missing statements entry-execution step dependency: pushFileScope";
    return false;
  }
  if (!input.emitCurrentFileScopeCleanup) {
    errorOut = "native backend missing statements entry-execution step dependency: emitCurrentFileScopeCleanup";
    return false;
  }
  if (!input.popFileScope) {
    errorOut = "native backend missing statements entry-execution step dependency: popFileScope";
    return false;
  }
  if (input.instructions == nullptr) {
    errorOut = "native backend missing statements entry-execution step dependency: instructions";
    return false;
  }

  std::optional<OnErrorHandler> entryOnError;
  auto entryOnErrorIt = input.onErrorByDef->find(input.entryDef->fullPath);
  if (entryOnErrorIt != input.onErrorByDef->end()) {
    entryOnError = entryOnErrorIt->second;
  }

  std::optional<ResultReturnInfo> entryResult;
  if (input.entryHasResultInfo) {
    entryResult = *input.entryResultInfo;
  }

  const auto entryExecutionResult = emitEntryCallableExecutionWithCleanup(
      *input.entryDef,
      input.returnsVoid,
      *input.sawReturn,
      *input.currentOnError,
      entryOnError,
      *input.currentReturnResult,
      entryResult,
      input.emitEntryStatement,
      input.pushFileScope,
      input.emitCurrentFileScopeCleanup,
      input.popFileScope,
      *input.instructions,
      errorOut);
  return entryExecutionResult == EntryCallableExecutionResult::Emitted;
}

} // namespace primec::ir_lowerer

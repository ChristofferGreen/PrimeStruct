#include "IrLowererLowerStatementsCallsStage.h"

#include "IrLowererLowerStatementsCallsStep.h"
#include "IrLowererLowerStatementsEntryExecutionStep.h"
#include "IrLowererLowerStatementsEntryStatementStep.h"
#include "IrLowererLowerStatementsFunctionTableStep.h"

namespace primec::ir_lowerer {

bool runLowerStatementsCallsStage(const LowerStatementsCallsStageInput &input,
                                  std::string &errorOut) {
  if (input.program == nullptr || input.entryDef == nullptr || input.semanticProgram == nullptr ||
      input.defaultEffects == nullptr || input.entryDefaultEffects == nullptr || input.defMap == nullptr ||
      input.structNames == nullptr || input.onErrorByDef == nullptr || input.stringTable == nullptr ||
      input.loweredCallTargets == nullptr || input.instructionSourceRangesByFunction == nullptr ||
      input.currentOnError == nullptr || input.currentReturnResult == nullptr ||
      input.sawReturn == nullptr || input.nextLocal == nullptr || input.onErrorTempCounter == nullptr ||
      input.entryResultInfo == nullptr || input.function == nullptr || input.locals == nullptr ||
      input.outModule == nullptr) {
    errorOut = "native backend missing statements-calls stage input";
    return false;
  }
  if (!input.inferExprKind || !input.emitExpr || !input.emitStatement || !input.allocTempLocal ||
      !input.appendInstructionSourceRange || !input.pushFileScope ||
      !input.emitCurrentFileScopeCleanup || !input.popFileScope || !input.resolveExprPath ||
      !input.resolveMethodCallDefinition || !input.resolveDefinitionCall || !input.getReturnInfo ||
      !input.emitInlineDefinitionCall || !input.isTailCallCandidate ||
      !input.isStructDefinition || !input.isArrayCountCall || !input.isStringCountCall ||
      !input.isVectorCapacityCall || !input.buildDefinitionCallContext ||
      !input.resetDefinitionLoweringState) {
    errorOut = "native backend missing statements-calls stage dependency";
    return false;
  }

  auto emitEntryStatement = [&](const Expr &stmt) -> bool {
    return runLowerStatementsEntryStatementStep(
        {
            .function = input.function,
            .emitStatement = [&](const Expr &entryStmt) { return input.emitStatement(entryStmt, *input.locals); },
            .appendInstructionSourceRange = input.appendInstructionSourceRange,
        },
        stmt,
        errorOut);
  };

  if (!runLowerStatementsEntryExecutionStep(
          {
              .entryDef = input.entryDef,
              .returnsVoid = input.returnsVoid,
              .sawReturn = input.sawReturn,
              .onErrorByDef = input.onErrorByDef,
              .currentOnError = input.currentOnError,
              .currentReturnResult = input.currentReturnResult,
              .entryHasResultInfo = input.entryHasResultInfo,
              .entryResultInfo = input.entryResultInfo,
              .emitEntryStatement = emitEntryStatement,
              .pushFileScope = input.pushFileScope,
              .emitCurrentFileScopeCleanup = input.emitCurrentFileScopeCleanup,
              .popFileScope = input.popFileScope,
              .instructions = &input.function->instructions,
          },
          errorOut)) {
    return false;
  }

  if (!runLowerStatementsFunctionTableStep(
          {
              .program = input.program,
              .entryDef = input.entryDef,
              .semanticProgram = input.semanticProgram,
              .function = input.function,
              .loweredCallTargets = input.loweredCallTargets,
              .isStructDefinition = input.isStructDefinition,
              .getReturnInfo = input.getReturnInfo,
              .defaultEffects = input.defaultEffects,
              .entryDefaultEffects = input.entryDefaultEffects,
              .isTailCallCandidate = input.isTailCallCandidate,
              .resetDefinitionLoweringState = input.resetDefinitionLoweringState,
              .buildDefinitionCallContext = input.buildDefinitionCallContext,
              .emitInlineDefinitionCall = input.emitInlineDefinitionCall,
              .nextLocal = input.nextLocal,
              .outFunctions = &input.outModule->functions,
              .entryIndex = &input.outModule->entryIndex,
          },
          errorOut)) {
    return false;
  }

  return runLowerStatementsSourceMapStep(
      {
          .defMap = input.defMap,
          .instructionSourceRangesByFunction = input.instructionSourceRangesByFunction,
          .stringTable = input.stringTable,
          .outModule = input.outModule,
      },
      errorOut);
}

} // namespace primec::ir_lowerer

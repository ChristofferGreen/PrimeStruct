#include "IrLowererLowerStatementsCallsStage.h"

#include "IrLowererLowerStatementsCallsStep.h"
#include "IrLowererLowerStatementsEntryExecutionStep.h"
#include "IrLowererLowerStatementsEntryStatementStep.h"
#include "IrLowererLowerStatementsFunctionTableStep.h"

namespace primec::ir_lowerer {

namespace {

// One definition whose top-level statements get lowered directly into a
// target IrFunction (as opposed to being inlined at each call site). Today
// this is always exactly the program's entry definition - the loop below
// has a single iteration and is byte-identical to a direct call. This
// exists so a future pass that decides which additional definitions need a
// real, non-inlined IrFunction (TODO-4747 Phase 1) can populate more
// entries here, reusing the same already-built emitStatement/emitExpr
// closures (they capture their target state by reference, see the driver
// refactor's Step 4a inventory in docs/todo.md) instead of restructuring
// this loop. Phase 1 will still need to solve two things this refactor
// deliberately leaves open: giving each additional body its own IrFunction
// target (currently only the entry has one, allocated further down this
// pipeline in runLowerStatementsFunctionTableStep), and rebuilding the
// count-access classifiers (isArrayCountCall/isStringCountCall/
// isVectorCapacityCall) per body, since those close over hasEntryArgs/
// entryArgsName by value at construction time, not by reference.
struct CallableBodyToLower {
  const Definition *def = nullptr;
  bool returnsVoid = false;
  bool hasResultInfo = false;
  const ResultReturnInfo *resultInfo = nullptr;
  IrFunction *targetFunction = nullptr;
};

} // namespace

bool runLowerStatementsCallsStage(const LowerStatementsCallsStageInput &input,
                                  std::string &errorOut) {
  if (input.program == nullptr || input.entryDef == nullptr || input.semanticProgram == nullptr ||
      input.defaultEffects == nullptr || input.entryDefaultEffects == nullptr || input.defMap == nullptr ||
      input.structNames == nullptr || input.onErrorByDef == nullptr || input.stringTable == nullptr ||
      input.loweredCallTargets == nullptr || input.instructionSourceRangesByFunction == nullptr ||
      input.functionSyntaxProvenanceByName == nullptr ||
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

  const std::vector<CallableBodyToLower> bodiesToLower = {
      {input.entryDef, input.returnsVoid, input.entryHasResultInfo, input.entryResultInfo, input.function},
  };

  for (const CallableBodyToLower &body : bodiesToLower) {
    auto emitBodyStatement = [&](const Expr &stmt) -> bool {
      return runLowerStatementsEntryStatementStep(
          {
              .function = body.targetFunction,
              .emitStatement = [&](const Expr &bodyStmt) { return input.emitStatement(bodyStmt, *input.locals); },
              .appendInstructionSourceRange = input.appendInstructionSourceRange,
          },
          stmt,
          errorOut);
    };

    if (!runLowerStatementsEntryExecutionStep(
            {
                .entryDef = body.def,
                .returnsVoid = body.returnsVoid,
                .sawReturn = input.sawReturn,
                .onErrorByDef = input.onErrorByDef,
                .currentOnError = input.currentOnError,
                .currentReturnResult = input.currentReturnResult,
                .entryHasResultInfo = body.hasResultInfo,
                .entryResultInfo = body.resultInfo,
                .emitEntryStatement = emitBodyStatement,
                .pushFileScope = input.pushFileScope,
                .emitCurrentFileScopeCleanup = input.emitCurrentFileScopeCleanup,
                .popFileScope = input.popFileScope,
                .instructions = &body.targetFunction->instructions,
            },
            errorOut)) {
      if (errorOut.empty()) {
        errorOut = "lower statements entry execution failed without diagnostic";
      }
      return false;
    }
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
    if (errorOut.empty()) {
      errorOut = "lower statements function table failed without diagnostic";
    }
    return false;
  }

  const bool sourceMapLowered = runLowerStatementsSourceMapStep(
      {
          .functionSyntaxProvenanceByName = input.functionSyntaxProvenanceByName,
          .instructionSourceRangesByFunction = input.instructionSourceRangesByFunction,
          .stringTable = input.stringTable,
          .outModule = input.outModule,
      },
      errorOut);
  if (!sourceMapLowered && errorOut.empty()) {
    errorOut = "lower statements source map failed without diagnostic";
  }
  return sourceMapLowered;
}

} // namespace primec::ir_lowerer

#include "IrLowererLowerStatementsCallsStage.h"

#include "IrLowererLowerEffects.h"
#include "IrLowererLowerStatementsCallsStep.h"
#include "IrLowererLowerStatementsEntryExecutionStep.h"
#include "IrLowererLowerStatementsEntryStatementStep.h"
#include "IrLowererLowerStatementsFunctionTableStep.h"
#include "IrLowererRecursionAnalysis.h"
#include "IrLowererSetupTypeHelpers.h"

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

  // TODO-4747 Phase 1: lower the bodies of definitions statically identified
  // as safe for real (non-inlined) Call/CallVoid emission - see
  // computeRealCallEligibleDefinitionPaths. This must run after the block
  // above: `input.function` (the reused scratch IrFunction) was just moved
  // into `input.outModule->functions` by finalizeEntryFunctionTableAndLower-
  // Callables (via runLowerStatementsFunctionTableStep), leaving it safe to
  // reset and reuse here, and `input.outModule->functions.size()` now gives
  // the base index new functions get appended at.
  if (input.realCallEligibleOrder != nullptr && !input.realCallEligibleOrder->empty()) {
    if (input.realCallReservationIndex == nullptr) {
      errorOut = "native backend missing statements-calls stage dependency: realCallReservationIndex";
      return false;
    }
    const uint64_t baseFunctionIndex = static_cast<uint64_t>(input.outModule->functions.size());

    for (const std::string &path : *input.realCallEligibleOrder) {
      const auto defIt = input.defMap->find(path);
      if (defIt == input.defMap->end() || defIt->second == nullptr) {
        errorOut = "internal error: missing definition for real-call target " + path;
        return false;
      }
      const Definition &def = *defIt->second;

      ReturnInfo returnInfo;
      if (!input.getReturnInfo(path, returnInfo)) {
        errorOut = "internal error: missing return info for real-call target " + path;
        return false;
      }

      input.resetDefinitionLoweringState();
      *input.function = IrFunction{};
      input.locals->clear();
      *input.nextLocal = 0;

      input.function->name = def.fullPath;
      input.function->parameterCount = static_cast<uint32_t>(def.parameters.size());
      const auto activeEffects =
          resolveActiveEffects(def.transforms, false, *input.defaultEffects, *input.entryDefaultEffects);
      if (!resolveEffectMask(def.transforms,
                             false,
                             *input.defaultEffects,
                             *input.entryDefaultEffects,
                             input.function->metadata.effectMask,
                             errorOut)) {
        return false;
      }
      if (!resolveCapabilityMask(
              def.transforms, activeEffects, def.fullPath, input.function->metadata.capabilityMask, errorOut)) {
        return false;
      }
      input.function->metadata.schedulingScope = IrSchedulingScope::Default;
      input.function->metadata.instrumentationFlags = 0;

      // The eligibility scan already verified every parameter statically
      // resolves to a scalar type - re-derive it here with the identical
      // extraction rather than trusting that verdict blindly, and fail
      // loudly on a mismatch instead of silently miscompiling.
      for (const Expr &param : def.parameters) {
        const std::string typeName = extractParameterTypeNameStatic(param);
        if (typeName.empty() || !isSupportedScalarTypeName(typeName)) {
          errorOut = "internal error: real-call eligibility mismatch on parameter type for " + def.fullPath;
          return false;
        }
        LocalInfo info;
        info.index = (*input.nextLocal)++;
        info.kind = LocalInfo::Kind::Value;
        info.valueKind = valueKindFromTypeName(typeName);
        input.locals->emplace(param.name, info);
      }
      // Prologue: the caller left its evaluated arguments on the shared
      // operand stack in parameter order (see the real-call redirect in
      // IrLowererLowerInlineCalls.h) - pop them into locals 0..N-1, last
      // pushed first, matching the calling convention already used by every
      // existing Call/CallVoid site in the native/VM/C++ backends.
      for (size_t i = def.parameters.size(); i-- > 0;) {
        input.function->instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(i)});
      }

      auto emitBodyStatement = [&](const Expr &stmt) -> bool {
        return runLowerStatementsEntryStatementStep(
            {
                .function = input.function,
                .emitStatement = [&](const Expr &bodyStmt) { return input.emitStatement(bodyStmt, *input.locals); },
                .appendInstructionSourceRange = input.appendInstructionSourceRange,
            },
            stmt,
            errorOut);
      };

      if (!runLowerStatementsEntryExecutionStep(
              {
                  .entryDef = &def,
                  .returnsVoid = returnInfo.returnsVoid,
                  .sawReturn = input.sawReturn,
                  .onErrorByDef = input.onErrorByDef,
                  .currentOnError = input.currentOnError,
                  .currentReturnResult = input.currentReturnResult,
                  .entryHasResultInfo = false,
                  .entryResultInfo = nullptr,
                  .emitEntryStatement = emitBodyStatement,
                  .pushFileScope = input.pushFileScope,
                  .emitCurrentFileScopeCleanup = input.emitCurrentFileScopeCleanup,
                  .popFileScope = input.popFileScope,
                  .instructions = &input.function->instructions,
              },
              errorOut)) {
        if (errorOut.empty()) {
          errorOut = "lower statements entry execution failed without diagnostic for real-call target " + path;
        }
        return false;
      }

      input.outModule->functions.push_back(std::move(*input.function));
    }

    // Fixup pass: rewrite every placeholder Call/CallVoid imm
    // ((1<<32)|reservationIndex) emitted by the real-call redirect - by
    // either the entry, an orchestration-lowered callable, or one of the
    // bodies just lowered above (mutual recursion) - to its target's final
    // index in input.outModule->functions.
    for (IrFunction &fn : input.outModule->functions) {
      for (IrInstruction &inst : fn.instructions) {
        if ((inst.op == IrOpcode::Call || inst.op == IrOpcode::CallVoid) && (inst.imm >> 32) == 1ull) {
          const uint64_t reservationIndex = inst.imm & 0xFFFFFFFFull;
          inst.imm = baseFunctionIndex + reservationIndex;
        }
      }
    }
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

#include "primec/ir_lowerer/IrLowererLowerStatementsCallsStage.h"

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerEffects.h"
#include "primec/ir_lowerer/IrLowererLowerStatementsCallsStep.h"
#include "primec/ir_lowerer/IrLowererLowerStatementsEntryExecutionStep.h"
#include "primec/ir_lowerer/IrLowererLowerStatementsEntryStatementStep.h"
#include "primec/ir_lowerer/IrLowererLowerStatementsFunctionTableStep.h"
#include "primec/ir_lowerer/IrLowererRuntimeErrorHelpers.h"
#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"

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
    if (input.entryReturnsVoidStorage == nullptr) {
      errorOut = "native backend missing statements-calls stage dependency: entryReturnsVoidStorage";
      return false;
    }
    const uint64_t baseFunctionIndex = static_cast<uint64_t>(input.outModule->functions.size());

    // The return-statement lowering machinery reached via input.emitStatement
    // reads *input.entryReturnsVoidStorage by reference rather than a value
    // threaded per body (it was bound once when the entry's own closures
    // were built) - each iteration below must override it to match the body
    // actually being lowered here, and this guard restores the entry's own
    // value once every eligible body has been lowered.
    struct EntryReturnsVoidRestoreGuard {
      bool *storage;
      bool savedValue;
      ~EntryReturnsVoidRestoreGuard() { *storage = savedValue; }
    } entryReturnsVoidRestoreGuard{input.entryReturnsVoidStorage, *input.entryReturnsVoidStorage};
    if (input.realCallStructReturnStorage == nullptr) {
      errorOut = "native backend missing statements-calls stage dependency: realCallStructReturnStorage";
      return false;
    }
    struct StructReturnFlagResetGuard {
      int32_t *storage;
      ~StructReturnFlagResetGuard() { *storage = -1; }
    } structReturnFlagResetGuard{input.realCallStructReturnStorage};

    // A runtime error in a called function ends the program, not just the function (address ABI targets).
    const RuntimeErrorsTrapScope trapScope(input.realCallAddressAbi);
    // Lowering a body can reserve further definitions (its own call sites), which append to the order: walk it by
    // index until no new reservation appears.
    for (size_t orderIndex = 0; orderIndex < input.realCallEligibleOrder->size(); ++orderIndex) {
      const std::string path = (*input.realCallEligibleOrder)[orderIndex];
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
      *input.entryReturnsVoidStorage = returnInfo.returnsVoid;
      *input.realCallStructReturnStorage = -1;
      const bool returnsStruct = input.realCallAddressAbi && input.structNames != nullptr &&
                                 !realCallStructReturnPath(def, *input.structNames, *input.defMap).empty();

      input.resetDefinitionLoweringState();
      *input.function = IrFunction{};
      input.locals->clear();
      *input.nextLocal = 0;

      std::string helperParent;
      const bool takesThis = input.structNames != nullptr &&
                             isStructHelperDefinition(def, *input.structNames, helperParent) &&
                             !definitionHasTransform(def, "static");
      const size_t parameterCount = def.parameters.size() + (takesThis ? 1 : 0);
      input.function->name = def.fullPath;
      input.function->parameterCount = static_cast<uint32_t>(parameterCount);
      // Match lowerCallableDefinitionOrchestration's precedence
      // (IrLowererStatementCallHelpers.cpp): prefer the semantic-product's
      // transitively-computed active effects/capabilities when available -
      // resolveEffectMask(def.transforms, ...) only sees this definition's
      // own declared transforms, which can be a narrower mask than what the
      // definition's body actually does (e.g. via effects an inlined callee
      // contributes), a soundness gap for target effect-mask enforcement
      // (see IrValidation.cpp's wasm/browser allowed-mask check).
      if (const auto *callableSummary =
              findSemanticProductCallableSummary(input.semanticProgram, def.fullPath);
          callableSummary != nullptr) {
        input.function->metadata.effectMask = 0;
        for (const auto &effect : callableSummary->activeEffects) {
          uint64_t bit = 0;
          if (!effectBitForName(effect, bit)) {
            errorOut = "unsupported effect in metadata: " + effect;
            return false;
          }
          input.function->metadata.effectMask |= bit;
        }
        input.function->metadata.capabilityMask = 0;
        for (const auto &capability : callableSummary->activeCapabilities) {
          uint64_t bit = 0;
          if (!effectBitForName(capability, bit)) {
            errorOut = "unsupported capability in metadata: " + capability;
            return false;
          }
          input.function->metadata.capabilityMask |= bit;
        }
      } else {
        if (input.semanticProgram != nullptr) {
          errorOut = "missing semantic-product callable summary: " + def.fullPath;
          return false;
        }
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
      }
      input.function->metadata.schedulingScope = IrSchedulingScope::Default;
      input.function->metadata.instrumentationFlags =
          hasTailExecutionCandidate(def.statements, returnInfo.returnsVoid, input.isTailCallCandidate)
              ? InstrumentationTailExecution
              : 0;

      if (input.realCallAddressAbi) {
        // The parameters (and the receiver first) are bound as for a callable definition; each arrives as one slot,
        // the value the inlined call binds. A `mut` scalar or struct parameter is the address of the caller's place,
        // as in an inlined call.
        Expr contextCall;
        if (!input.buildDefinitionCallContext(def, *input.nextLocal, *input.locals, contextCall, errorOut)) {
          if (errorOut.empty()) {
            errorOut = "internal error: could not bind the parameters of real-call target " + def.fullPath;
          }
          return false;
        }
        std::vector<int32_t> parameterLocals;
        if (takesThis) {
          const auto thisIt = input.locals->find("this");
          if (thisIt == input.locals->end()) {
            errorOut = "internal error: real-call target has no receiver: " + def.fullPath;
            return false;
          }
          parameterLocals.push_back(thisIt->second.index);
        }
        for (const Expr &param : def.parameters) {
          const auto localIt = input.locals->find(param.name);
          if (localIt == input.locals->end()) {
            errorOut =
                "internal error: real-call target has no local for parameter: " + def.fullPath;
            return false;
          }
          LocalInfo &info = localIt->second;
          if (info.kind == LocalInfo::Kind::Value && info.isMutable && !info.isFileHandle &&
              (!info.structTypeName.empty() || (!info.isFileError && !info.isResult &&
                                                info.valueKind != LocalInfo::ValueKind::Unknown &&
                                                info.valueKind != LocalInfo::ValueKind::String))) {
            info.kind = LocalInfo::Kind::Reference;
          }
          parameterLocals.push_back(info.index);
        }
        if (returnsStruct) {
          // The address of the caller's result block comes last; a return copies the result there.
          *input.realCallStructReturnStorage = (*input.nextLocal)++;
          parameterLocals.push_back(*input.realCallStructReturnStorage);
        }
        for (size_t i = parameterLocals.size(); i-- > 0;) {
          input.function->instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(parameterLocals[i])});
        }
        input.function->parameterCount = static_cast<uint32_t>(parameterLocals.size());
      } else {
        // The scalar ABI: the eligibility scan already verified every parameter is a scalar - re-derive it here and
        // fail loudly on a mismatch instead of silently miscompiling.
        for (const Expr &param : def.parameters) {
          const std::string typeName = extractParameterTypeNameStatic(param);
          if (typeName.empty() || !isSupportedScalarTypeName(typeName)) {
            errorOut = "internal error: real-call eligibility mismatch on parameter type for " +
                       def.fullPath;
            return false;
          }
        }
        if (takesThis) {
          // The implicit `this` (an address) and the parameters, bound as for a callable definition.
          Expr contextCall;
          if (!input.buildDefinitionCallContext(
                  def, *input.nextLocal, *input.locals, contextCall, errorOut)) {
            if (errorOut.empty()) {
              errorOut = "internal error: could not bind the parameters of real-call target " +
                         def.fullPath;
            }
            return false;
          }
        } else {
          for (const Expr &param : def.parameters) {
            const std::string typeName = extractParameterTypeNameStatic(param);
            LocalInfo info;
            info.index = (*input.nextLocal)++;
            info.kind = LocalInfo::Kind::Value;
            info.valueKind = valueKindFromTypeName(typeName);
            input.locals->emplace(param.name, info);
          }
        }
        // Prologue: the caller left its evaluated arguments on the shared operand stack in parameter order - a
        // scalar is one slot and the receiver is its slots. Pop them last pushed first. The receiver is rebuilt in a
        // block of this frame and `this` points at the block.
        uint32_t stackSlots = 0;
        for (size_t i = def.parameters.size(); i-- > 0;) {
          const auto localIt = input.locals->find(def.parameters[i].name);
          if (localIt == input.locals->end()) {
            errorOut =
                "internal error: real-call target has no local for parameter: " + def.fullPath;
            return false;
          }
          input.function->instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(localIt->second.index)});
          stackSlots += 1;
        }
        if (takesThis) {
          const auto thisIt = input.locals->find("this");
          if (thisIt == input.locals->end() || thisIt->second.structSlotCount <= 0) {
            errorOut = "internal error: real-call target has no receiver layout: " + def.fullPath;
            return false;
          }
          const int32_t slotCount = thisIt->second.structSlotCount;
          const int32_t base = *input.nextLocal;
          *input.nextLocal += slotCount;
          for (int32_t slot = slotCount; slot-- > 0;) {
            input.function->instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(base + slot)});
          }
          input.function->instructions.push_back(
              {IrOpcode::AddressOfLocal, static_cast<uint64_t>(base)});
          input.function->instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(thisIt->second.index)});
          stackSlots += static_cast<uint32_t>(slotCount);
        }
        input.function->parameterCount = stackSlots;
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

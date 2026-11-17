#include "IrLowererLowerSumHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"

namespace primec::ir_lowerer {

    LoweredSumPickEmitResult SumHelpersContext::tryEmitPickStatement(const Expr &stmt, LocalMap &localsIn) {
      if (!isPickCall(stmt)) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (!stmt.hasBodyArguments && stmt.bodyArguments.empty()) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (stmt.args.size() != 1 || stmt.bodyArguments.empty()) {
        error = "native backend requires pick(value) with variant arms";
        return LoweredSumPickEmitResult::Error;
      }
      const Definition *sumDef = resolvePickTargetSumDefinition(stmt.args.front(), localsIn);
      if (sumDef == nullptr) {
        if (error.empty()) {
          error = "native backend pick target requires sum value";
        }
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      int32_t subjectDropFlagLocal = -1;
      const bool ownsSubject = beginOwnedPickSubjectScope(
          stmt.args.front(), *sumDef, sumPtrLocal, localsIn, subjectDropFlagLocal);
      std::vector<size_t> endJumps;
      for (const Expr &arm : stmt.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SumVariant *variant = resolvePickArmVariant(*sumDef, arm);
        if (variant == nullptr) {
          return LoweredSumPickEmitResult::Error;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef->fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef->fullPath + "/" + variant->name;
          return LoweredSumPickEmitResult::Error;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(*sumDef, *variant, "pick arm", tagValue)) {
          return LoweredSumPickEmitResult::Error;
        }
        emitSumTagComparison(sumPtrLocal, tagValue);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = localsIn;
        // An arm is a scope of its own, so no later exit reads the drop flag of an arm that never ran.
        returnEmitStage.pushFileScope();
        if (payloadArm && !bindPickPayload(*sumDef,
                                           *variant,
                                           arm.args.front(),
                                           sumPtrLocal,
                                           branchLocals,
                                           isMutableLocalExpr(stmt.args.front(), localsIn),
                                           subjectDropFlagLocal)) {
          return LoweredSumPickEmitResult::Error;
        }
        const std::vector<const Expr *> armBody = pickArmBodyExprs(arm);
        for (const Expr *bodyExprPtr : armBody) {
          if (!emitStatement(*bodyExprPtr, branchLocals)) {
            return LoweredSumPickEmitResult::Error;
          }
        }
        // An arm that ends in a `return` has left through its exit already.
        if (armBody.empty() || !isReturnCall(*armBody.back())) {
          returnEmitStage.emitFileScopeCleanup(setupStage.fileScopeStack.back());
        }
        returnEmitStage.popFileScope();
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextArmJump].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      if (ownsSubject) {
        endOwnedPickSubjectScope();
      }
      return LoweredSumPickEmitResult::Emitted;
    }

} // namespace primec::ir_lowerer

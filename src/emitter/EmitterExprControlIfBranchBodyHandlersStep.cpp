#include "EmitterExprControlIfBranchBodyHandlersStep.h"

#include "EmitterExprControlIfBranchBodyBindingStep.h"
#include "EmitterExprControlIfBranchBodyDispatchStep.h"
#include "EmitterExprControlIfBranchBodyReturnStep.h"
#include "EmitterExprControlIfBranchBodyStatementStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchBodyHandlersStepResult
runEmitterExprControlIfBranchBodyHandlersStep(
    const Expr &stmt,
    bool isLast,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBranchBodyHandlersIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchBodyHandlersGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchBodyHandlersHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchBodyHandlersInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchBodyHandlersTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchBodyHandlersIsReferenceCandidateFn &isReferenceCandidate,
    const EmitterExprControlIfBranchBodyHandlersEmitExprFn &emitExpr) {
  const auto dispatchStep = runEmitterExprControlIfBranchBodyDispatchStep(
      stmt,
      isLast,
      [&](const Expr &candidate, bool candidateIsLast) {
        if (const auto returnStep =
                runEmitterExprControlIfBranchBodyReturnStep(
                    candidate, candidateIsLast, isReturnCall, emitExpr);
            returnStep.handled) {
          return returnStep.emitted;
        }
        return EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const Expr &candidate) {
        if (const auto bindingStep =
                runEmitterExprControlIfBranchBodyBindingStep(
                    candidate,
                    branchTypes,
                    returnKinds,
                    allowMathBare,
                    importAliases,
                    structTypeMap,
                    getBindingInfo,
                    hasExplicitBindingTypeTransform,
                    inferPrimitiveReturnKind,
                    typeNameForReturnKind,
                    isReferenceCandidate,
                    emitExpr);
            bindingStep.handled) {
          return bindingStep.emitted;
        }
        return EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const Expr &candidate) {
        if (const auto statementStep =
                runEmitterExprControlIfBranchBodyStatementStep(
                    candidate, emitExpr);
            statementStep.handled) {
          return statementStep.emitted;
        }
        return EmitterExprControlIfBranchBodyEmitResult{};
      });
  if (!dispatchStep.handled) {
    return {};
  }

  EmitterExprControlIfBranchBodyHandlersStepResult result;
  result.handled = true;
  result.emitted = dispatchStep.emitted;
  return result;
}

} // namespace primec::emitter

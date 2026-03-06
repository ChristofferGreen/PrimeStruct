#include "EmitterExprControlIfBranchEmitStep.h"

#include "EmitterExprControlIfBranchBodyHandlersStep.h"
#include "EmitterExprControlIfBranchValueStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchEmitStepResult runEmitterExprControlIfBranchEmitStep(
    const Expr &candidate,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBranchEmitIsEnvelopeFn &isIfBlockEnvelope,
    const EmitterExprControlIfBranchEmitIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBranchEmitGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchEmitHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchEmitInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchEmitTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchEmitIsReferenceCandidateFn &isReferenceCandidate,
    const EmitterExprControlIfBranchEmitEmitExprFn &emitExpr) {
  if (!isIfBlockEnvelope || !isReturnCall || !getBindingInfo ||
      !hasExplicitBindingTypeTransform || !inferPrimitiveReturnKind ||
      !typeNameForReturnKind || !isReferenceCandidate || !emitExpr) {
    return {};
  }

  if (const auto branchValueStep = runEmitterExprControlIfBranchValueStep(
          candidate,
          [&](const Expr &candidateExpr) { return isIfBlockEnvelope(candidateExpr); },
          [&](const Expr &candidateExpr) { return emitExpr(candidateExpr); },
          [&](const Expr &stmt, bool isLast) {
            if (const auto handlersStep = runEmitterExprControlIfBranchBodyHandlersStep(
                    stmt,
                    isLast,
                    branchTypes,
                    returnKinds,
                    allowMathBare,
                    importAliases,
                    structTypeMap,
                    isReturnCall,
                    getBindingInfo,
                    hasExplicitBindingTypeTransform,
                    inferPrimitiveReturnKind,
                    typeNameForReturnKind,
                    isReferenceCandidate,
                    emitExpr);
                handlersStep.handled) {
              return handlersStep.emitted;
            }
            return EmitterExprControlIfBranchBodyEmitResult{};
          });
      branchValueStep.handled) {
    EmitterExprControlIfBranchEmitStepResult result;
    result.handled = true;
    result.emittedExpr = branchValueStep.emittedExpr;
    return result;
  }

  return {};
}

} // namespace primec::emitter

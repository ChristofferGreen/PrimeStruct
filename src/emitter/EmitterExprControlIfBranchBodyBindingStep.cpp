#include "EmitterExprControlIfBranchBodyBindingStep.h"

#include "EmitterExprControlIfBlockBindingAutoStep.h"
#include "EmitterExprControlIfBlockBindingExplicitStep.h"
#include "EmitterExprControlIfBlockBindingFallbackStep.h"
#include "EmitterExprControlIfBlockBindingPreludeStep.h"
#include "EmitterExprControlIfBlockBindingQualifiersStep.h"

namespace primec::emitter {

EmitterExprControlIfBranchBodyBindingStepResult
runEmitterExprControlIfBranchBodyBindingStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBranchBodyBindingGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchBodyBindingHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchBodyBindingInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchBodyBindingTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchBodyBindingIsReferenceCandidateFn &isReferenceCandidate,
    const EmitterExprControlIfBranchBodyBindingEmitExprFn &emitExpr) {
  const auto bindingPrelude = runEmitterExprControlIfBlockBindingPreludeStep(
      stmt,
      branchTypes,
      returnKinds,
      allowMathBare,
      getBindingInfo,
      hasExplicitBindingTypeTransform,
      inferPrimitiveReturnKind,
      typeNameForReturnKind);
  if (!bindingPrelude.handled) {
    return {};
  }

  EmitterExprControlIfBranchBodyBindingStepResult result;
  result.handled = true;

  Emitter::BindingInfo binding = bindingPrelude.binding;
  const bool hasExplicitType = bindingPrelude.hasExplicitType;
  const bool useAuto = bindingPrelude.useAuto;
  if (const auto autoBindingStep = runEmitterExprControlIfBlockBindingAutoStep(
          stmt, binding, useAuto, emitExpr);
      autoBindingStep.handled) {
    result.emitted.handled = true;
    result.emitted.emittedStatement = autoBindingStep.emittedStatement;
    result.emitted.shouldBreak = false;
    return result;
  }

  const auto bindingQualifiers = runEmitterExprControlIfBlockBindingQualifiersStep(
      binding,
      !stmt.args.empty(),
      isReferenceCandidate);
  const bool needsConst = bindingQualifiers.needsConst;
  const bool useRef = bindingQualifiers.useRef;
  if (const auto explicitBindingStep = runEmitterExprControlIfBlockBindingExplicitStep(
          stmt,
          binding,
          hasExplicitType,
          needsConst,
          useRef,
          stmt.namespacePrefix,
          importAliases,
          structTypeMap,
          emitExpr);
      explicitBindingStep.handled) {
    result.emitted.handled = true;
    result.emitted.emittedStatement = explicitBindingStep.emittedStatement;
    result.emitted.shouldBreak = false;
    return result;
  }

  if (const auto fallbackBindingStep = runEmitterExprControlIfBlockBindingFallbackStep(
          stmt, hasExplicitType, needsConst, useRef, emitExpr);
      fallbackBindingStep.handled) {
    result.emitted.handled = true;
    result.emitted.emittedStatement = fallbackBindingStep.emittedStatement;
    result.emitted.shouldBreak = false;
    return result;
  }

  return result;
}

} // namespace primec::emitter

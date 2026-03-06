#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIfBranchEmitIsEnvelopeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchEmitIsReturnCallFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchEmitGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBranchEmitHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchEmitInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBranchEmitTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
using EmitterExprControlIfBranchEmitIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
using EmitterExprControlIfBranchEmitEmitExprFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchEmitStepResult {
  bool handled = false;
  std::string emittedExpr;
};

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
    const EmitterExprControlIfBranchEmitEmitExprFn &emitExpr);

} // namespace primec::emitter

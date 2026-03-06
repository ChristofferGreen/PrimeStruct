#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIfBranchBodyBindingGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBranchBodyBindingHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyBindingInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBranchBodyBindingTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
using EmitterExprControlIfBranchBodyBindingIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
using EmitterExprControlIfBranchBodyBindingEmitExprFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchBodyBindingStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

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
    const EmitterExprControlIfBranchBodyBindingEmitExprFn &emitExpr);

} // namespace primec::emitter

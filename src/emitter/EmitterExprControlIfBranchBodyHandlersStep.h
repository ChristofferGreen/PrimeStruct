#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "EmitterExprControlIfBranchBodyStep.h"
#include "primec/ast/Ast.h"
#include "primec/backend/Emitter.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {

using ExprPredicateFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyHandlersGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBranchBodyHandlersHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBranchBodyHandlersInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBranchBodyHandlersTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;
using EmitterExprControlIfBranchBodyHandlersIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;
using ExprStringFn =
    std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBranchBodyHandlersStepResult {
  bool handled = false;
  EmitterExprControlIfBranchBodyEmitResult emitted;
};

EmitterExprControlIfBranchBodyHandlersStepResult
runEmitterExprControlIfBranchBodyHandlersStep(
    const Expr &stmt,
    bool isLast,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const ExprPredicateFn &isReturnCall,
    const EmitterExprControlIfBranchBodyHandlersGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBranchBodyHandlersHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBranchBodyHandlersInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBranchBodyHandlersTypeNameForReturnKindFn &typeNameForReturnKind,
    const EmitterExprControlIfBranchBodyHandlersIsReferenceCandidateFn &isReferenceCandidate,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

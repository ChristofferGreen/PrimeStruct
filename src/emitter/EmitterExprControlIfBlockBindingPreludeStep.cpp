#include "EmitterExprControlIfBlockBindingPreludeStep.h"

namespace primec::emitter {

EmitterExprControlIfBlockBindingPreludeStepResult runEmitterExprControlIfBlockBindingPreludeStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const EmitterExprControlIfBlockBindingPreludeGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBlockBindingPreludeHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBlockBindingPreludeInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBlockBindingPreludeTypeNameForReturnKindFn &typeNameForReturnKind) {
  if (!stmt.isBinding || !getBindingInfo || !hasExplicitBindingTypeTransform) {
    return {};
  }

  EmitterExprControlIfBlockBindingPreludeStepResult result;
  result.handled = true;
  result.binding = getBindingInfo(stmt);
  result.hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  result.lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
  if (!result.hasExplicitType && stmt.args.size() == 1 && !result.lambdaInit && inferPrimitiveReturnKind &&
      typeNameForReturnKind) {
    Emitter::ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), branchTypes, returnKinds, allowMathBare);
    std::string inferred = typeNameForReturnKind(initKind);
    if (!inferred.empty()) {
      result.binding.typeName = inferred;
      result.binding.typeTemplateArg.clear();
    }
  }
  branchTypes[stmt.name] = result.binding;
  result.useAuto = !result.hasExplicitType && result.lambdaInit;
  return result;
}

} // namespace primec::emitter

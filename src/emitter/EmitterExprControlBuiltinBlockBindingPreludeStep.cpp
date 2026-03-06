#include "EmitterExprControlBuiltinBlockBindingPreludeStep.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockBindingPreludeStepResult runEmitterExprControlBuiltinBlockBindingPreludeStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &blockTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const EmitterExprControlBuiltinBlockBindingPreludeGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlBuiltinBlockBindingPreludeHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlBuiltinBlockBindingPreludeInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlBuiltinBlockBindingPreludeTypeNameForReturnKindFn &typeNameForReturnKind) {
  if (!stmt.isBinding || !getBindingInfo || !hasExplicitBindingTypeTransform) {
    return {};
  }

  EmitterExprControlBuiltinBlockBindingPreludeStepResult result;
  result.handled = true;
  result.binding = getBindingInfo(stmt);
  result.hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  result.lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
  if (!result.hasExplicitType && stmt.args.size() == 1 && !result.lambdaInit && inferPrimitiveReturnKind &&
      typeNameForReturnKind) {
    Emitter::ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), blockTypes, returnKinds, allowMathBare);
    std::string inferred = typeNameForReturnKind(initKind);
    if (!inferred.empty()) {
      result.binding.typeName = inferred;
      result.binding.typeTemplateArg.clear();
    }
  }
  blockTypes[stmt.name] = result.binding;
  result.useAuto = !result.hasExplicitType && result.lambdaInit;
  return result;
}

} // namespace primec::emitter

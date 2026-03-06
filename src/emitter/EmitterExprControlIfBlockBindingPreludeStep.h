#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIfBlockBindingPreludeGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlIfBlockBindingPreludeHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlIfBlockBindingPreludeInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlIfBlockBindingPreludeTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;

struct EmitterExprControlIfBlockBindingPreludeStepResult {
  bool handled = false;
  Emitter::BindingInfo binding;
  bool hasExplicitType = false;
  bool lambdaInit = false;
  bool useAuto = false;
};

EmitterExprControlIfBlockBindingPreludeStepResult runEmitterExprControlIfBlockBindingPreludeStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &branchTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const EmitterExprControlIfBlockBindingPreludeGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlIfBlockBindingPreludeHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlIfBlockBindingPreludeInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlIfBlockBindingPreludeTypeNameForReturnKindFn &typeNameForReturnKind);

} // namespace primec::emitter

#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockBindingPreludeGetBindingInfoFn =
    std::function<Emitter::BindingInfo(const Expr &)>;
using EmitterExprControlBuiltinBlockBindingPreludeHasExplicitTypeFn =
    std::function<bool(const Expr &)>;
using EmitterExprControlBuiltinBlockBindingPreludeInferReturnKindFn =
    std::function<Emitter::ReturnKind(const Expr &,
                                      const std::unordered_map<std::string, Emitter::BindingInfo> &,
                                      const std::unordered_map<std::string, Emitter::ReturnKind> &,
                                      bool)>;
using EmitterExprControlBuiltinBlockBindingPreludeTypeNameForReturnKindFn =
    std::function<std::string(Emitter::ReturnKind)>;

struct EmitterExprControlBuiltinBlockBindingPreludeStepResult {
  bool handled = false;
  Emitter::BindingInfo binding;
  bool hasExplicitType = false;
  bool lambdaInit = false;
  bool useAuto = false;
};

EmitterExprControlBuiltinBlockBindingPreludeStepResult runEmitterExprControlBuiltinBlockBindingPreludeStep(
    const Expr &stmt,
    std::unordered_map<std::string, Emitter::BindingInfo> &blockTypes,
    const std::unordered_map<std::string, Emitter::ReturnKind> &returnKinds,
    bool allowMathBare,
    const EmitterExprControlBuiltinBlockBindingPreludeGetBindingInfoFn &getBindingInfo,
    const EmitterExprControlBuiltinBlockBindingPreludeHasExplicitTypeFn &hasExplicitBindingTypeTransform,
    const EmitterExprControlBuiltinBlockBindingPreludeInferReturnKindFn &inferPrimitiveReturnKind,
    const EmitterExprControlBuiltinBlockBindingPreludeTypeNameForReturnKindFn &typeNameForReturnKind);

} // namespace primec::emitter

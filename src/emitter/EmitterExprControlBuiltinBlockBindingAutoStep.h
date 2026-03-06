#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockBindingAutoEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlBuiltinBlockBindingAutoStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockBindingAutoStepResult runEmitterExprControlBuiltinBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const EmitterExprControlBuiltinBlockBindingAutoEmitExprFn &emitExpr);

} // namespace primec::emitter

#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIfBlockBindingAutoEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBlockBindingAutoStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockBindingAutoStepResult runEmitterExprControlIfBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const EmitterExprControlIfBlockBindingAutoEmitExprFn &emitExpr);

} // namespace primec::emitter

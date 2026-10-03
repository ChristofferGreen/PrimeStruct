#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/backend/Emitter.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlIfBlockBindingAutoStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockBindingAutoStepResult runEmitterExprControlIfBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

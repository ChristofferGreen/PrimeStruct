#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlBuiltinBlockBindingFallbackStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockBindingFallbackStepResult runEmitterExprControlBuiltinBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

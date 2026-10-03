#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlBuiltinBlockFinalValueStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockFinalValueStepResult runEmitterExprControlBuiltinBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const ExprPredicateFn &isReturnCall,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

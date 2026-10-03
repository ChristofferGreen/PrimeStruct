#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlBuiltinBlockEarlyReturnStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockEarlyReturnStepResult runEmitterExprControlBuiltinBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const ExprPredicateFn &isReturnCall,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

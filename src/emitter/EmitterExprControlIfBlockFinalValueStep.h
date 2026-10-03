#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlIfBlockFinalValueStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockFinalValueStepResult runEmitterExprControlIfBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const ExprPredicateFn &isReturnCall,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

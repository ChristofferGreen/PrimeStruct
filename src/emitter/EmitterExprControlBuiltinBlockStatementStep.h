#pragma once

#include <functional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


struct EmitterExprControlBuiltinBlockStatementStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockStatementStepResult runEmitterExprControlBuiltinBlockStatementStep(
    const Expr &stmt,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter

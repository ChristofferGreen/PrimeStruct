#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockStatementEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlBuiltinBlockStatementStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockStatementStepResult runEmitterExprControlBuiltinBlockStatementStep(
    const Expr &stmt,
    const EmitterExprControlBuiltinBlockStatementEmitExprFn &emitExpr);

} // namespace primec::emitter

#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBlockStatementEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBlockStatementStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockStatementStepResult runEmitterExprControlIfBlockStatementStep(
    const Expr &stmt,
    const EmitterExprControlIfBlockStatementEmitExprFn &emitExpr);

} // namespace primec::emitter

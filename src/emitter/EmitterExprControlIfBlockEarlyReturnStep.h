#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBlockEarlyReturnIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBlockEarlyReturnEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBlockEarlyReturnStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockEarlyReturnStepResult runEmitterExprControlIfBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBlockEarlyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBlockEarlyReturnEmitExprFn &emitExpr);

} // namespace primec::emitter

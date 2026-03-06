#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockEarlyReturnIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlBuiltinBlockEarlyReturnEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlBuiltinBlockEarlyReturnStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockEarlyReturnStepResult runEmitterExprControlBuiltinBlockEarlyReturnStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlBuiltinBlockEarlyReturnIsReturnCallFn &isReturnCall,
    const EmitterExprControlBuiltinBlockEarlyReturnEmitExprFn &emitExpr);

} // namespace primec::emitter

#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockFinalValueIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlBuiltinBlockFinalValueEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlBuiltinBlockFinalValueStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockFinalValueStepResult runEmitterExprControlBuiltinBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlBuiltinBlockFinalValueIsReturnCallFn &isReturnCall,
    const EmitterExprControlBuiltinBlockFinalValueEmitExprFn &emitExpr);

} // namespace primec::emitter

#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBlockFinalValueIsReturnCallFn = std::function<bool(const Expr &)>;
using EmitterExprControlIfBlockFinalValueEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBlockFinalValueStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockFinalValueStepResult runEmitterExprControlIfBlockFinalValueStep(
    const Expr &stmt,
    bool isLast,
    const EmitterExprControlIfBlockFinalValueIsReturnCallFn &isReturnCall,
    const EmitterExprControlIfBlockFinalValueEmitExprFn &emitExpr);

} // namespace primec::emitter

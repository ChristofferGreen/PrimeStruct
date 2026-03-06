#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlIfBlockBindingFallbackEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBlockBindingFallbackStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockBindingFallbackStepResult runEmitterExprControlIfBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const EmitterExprControlIfBlockBindingFallbackEmitExprFn &emitExpr);

} // namespace primec::emitter

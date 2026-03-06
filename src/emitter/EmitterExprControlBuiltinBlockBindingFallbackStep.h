#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockBindingFallbackEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlBuiltinBlockBindingFallbackStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockBindingFallbackStepResult runEmitterExprControlBuiltinBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const EmitterExprControlBuiltinBlockBindingFallbackEmitExprFn &emitExpr);

} // namespace primec::emitter

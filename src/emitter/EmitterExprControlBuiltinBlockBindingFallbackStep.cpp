#include "EmitterExprControlBuiltinBlockBindingFallbackStep.h"

#include <sstream>

namespace primec::emitter {

EmitterExprControlBuiltinBlockBindingFallbackStepResult runEmitterExprControlBuiltinBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const EmitterExprControlBuiltinBlockBindingFallbackEmitExprFn &emitExpr) {
  if (hasExplicitType) {
    return {};
  }

  EmitterExprControlBuiltinBlockBindingFallbackStepResult result;
  result.handled = true;

  std::ostringstream out;
  if (useRef) {
    out << "const auto & " << stmt.name;
  } else {
    out << (needsConst ? "const " : "") << "auto " << stmt.name;
  }

  if (!stmt.args.empty() && emitExpr) {
    out << " = " << emitExpr(stmt.args.front());
  }

  out << "; ";
  result.emittedStatement = out.str();
  return result;
}

} // namespace primec::emitter

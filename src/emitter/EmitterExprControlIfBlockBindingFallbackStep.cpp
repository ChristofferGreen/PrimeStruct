#include "EmitterExprControlIfBlockBindingFallbackStep.h"

#include <sstream>

namespace primec::emitter {

EmitterExprControlIfBlockBindingFallbackStepResult runEmitterExprControlIfBlockBindingFallbackStep(
    const Expr &stmt,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const EmitterExprControlIfBlockBindingFallbackEmitExprFn &emitExpr) {
  if (hasExplicitType) {
    return {};
  }

  EmitterExprControlIfBlockBindingFallbackStepResult result;
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

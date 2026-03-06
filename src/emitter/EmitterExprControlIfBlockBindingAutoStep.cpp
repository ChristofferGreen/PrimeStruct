#include "EmitterExprControlIfBlockBindingAutoStep.h"

#include <sstream>

namespace primec::emitter {

EmitterExprControlIfBlockBindingAutoStepResult runEmitterExprControlIfBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const EmitterExprControlIfBlockBindingAutoEmitExprFn &emitExpr) {
  if (!useAuto) {
    return {};
  }

  EmitterExprControlIfBlockBindingAutoStepResult result;
  result.handled = true;
  std::ostringstream out;
  out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
  if (!stmt.args.empty() && emitExpr) {
    out << " = " << emitExpr(stmt.args.front());
  }
  out << "; ";
  result.emittedStatement = out.str();
  return result;
}

} // namespace primec::emitter

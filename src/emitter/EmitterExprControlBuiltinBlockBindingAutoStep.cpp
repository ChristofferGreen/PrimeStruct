#include "EmitterExprControlBuiltinBlockBindingAutoStep.h"

#include <sstream>

namespace primec::emitter {

EmitterExprControlBuiltinBlockBindingAutoStepResult runEmitterExprControlBuiltinBlockBindingAutoStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool useAuto,
    const EmitterExprControlBuiltinBlockBindingAutoEmitExprFn &emitExpr) {
  if (!useAuto) {
    return {};
  }

  EmitterExprControlBuiltinBlockBindingAutoStepResult result;
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

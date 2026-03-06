#include "EmitterExprControlFieldAccessStep.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlFieldAccessStep(const Expr &expr,
                                                                const EmitFieldAccessReceiverFn &emitReceiverExpr) {
  if (expr.kind != Expr::Kind::Call || !expr.isFieldAccess || expr.args.empty()) {
    return std::nullopt;
  }
  if (!emitReceiverExpr) {
    return std::nullopt;
  }
  return emitReceiverExpr(expr.args.front()) + "." + expr.name;
}

} // namespace primec::emitter

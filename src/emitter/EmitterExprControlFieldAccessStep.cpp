#include "EmitterExprControlFieldAccessStep.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlFieldAccessStep(const Expr &expr,
                                                                const EmitFieldAccessReceiverFn &emitReceiverExpr,
                                                                const ResolveFieldAccessStaticReceiverFn &resolveStaticReceiverExpr) {
  if (expr.kind != Expr::Kind::Call || !expr.isFieldAccess || expr.args.empty()) {
    return std::nullopt;
  }
  if (!emitReceiverExpr) {
    return std::nullopt;
  }
  if (resolveStaticReceiverExpr) {
    if (const auto staticReceiverExpr = resolveStaticReceiverExpr(expr.args.front()); staticReceiverExpr.has_value()) {
      return *staticReceiverExpr + "::" + expr.name;
    }
  }
  return emitReceiverExpr(expr.args.front()) + "." + expr.name;
}

} // namespace primec::emitter

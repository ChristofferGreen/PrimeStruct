#include "EmitterExprControlBoolLiteralStep.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlBoolLiteralStep(const Expr &expr) {
  if (expr.kind != Expr::Kind::BoolLiteral) {
    return std::nullopt;
  }
  return expr.boolValue ? "true" : "false";
}

} // namespace primec::emitter

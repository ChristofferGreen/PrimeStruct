#include "EmitterExprControlFloatLiteralStep.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlFloatLiteralStep(const Expr &expr) {
  if (expr.kind != Expr::Kind::FloatLiteral) {
    return std::nullopt;
  }
  if (expr.floatWidth == 64) {
    return expr.floatValue;
  }
  std::string literal = expr.floatValue;
  if (literal.find('.') == std::string::npos && literal.find('e') == std::string::npos &&
      literal.find('E') == std::string::npos) {
    literal += ".0";
  }
  return literal + "f";
}

} // namespace primec::emitter

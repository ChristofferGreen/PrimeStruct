#include "EmitterExprControlStringLiteralStep.h"

#include "EmitterHelpers.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlStringLiteralStep(const Expr &expr) {
  if (expr.kind != Expr::Kind::StringLiteral) {
    return std::nullopt;
  }
  return "std::string_view(" + stripStringLiteralSuffix(expr.stringValue) + ")";
}

} // namespace primec::emitter

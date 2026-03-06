#include "EmitterExprControlIntegerLiteralStep.h"

#include <sstream>

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlIntegerLiteralStep(const Expr &expr) {
  if (expr.kind != Expr::Kind::Literal) {
    return std::nullopt;
  }
  if (!expr.isUnsigned && expr.intWidth == 32) {
    return std::to_string(static_cast<int64_t>(expr.literalValue));
  }
  if (expr.isUnsigned) {
    std::ostringstream out;
    out << "static_cast<uint64_t>(" << static_cast<uint64_t>(expr.literalValue) << ")";
    return out.str();
  }
  std::ostringstream out;
  out << "static_cast<int64_t>(" << static_cast<int64_t>(expr.literalValue) << ")";
  return out.str();
}

} // namespace primec::emitter

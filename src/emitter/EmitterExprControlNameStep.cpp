#include "EmitterExprControlNameStep.h"

#include "EmitterHelpers.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlNameStep(
    const Expr &expr,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    bool allowMathBare) {
  if (expr.kind != Expr::Kind::Name) {
    return std::nullopt;
  }
  if (localTypes.count(expr.name) == 0 && isBuiltinMathConstantName(expr.name, allowMathBare)) {
    std::string constantName = expr.name;
    if (!constantName.empty() && constantName[0] == '/') {
      constantName.erase(0, 1);
    }
    if (constantName.rfind("std/math/", 0) == 0) {
      constantName.erase(0, 9);
    }
    return "ps_const_" + constantName;
  }
  return expr.name;
}

} // namespace primec::emitter

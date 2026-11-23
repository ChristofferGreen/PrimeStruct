#include "IrLowererStringParameterUse.h"

#include <string_view>

namespace primec::ir_lowerer {

namespace {

// Whether every use of the `string` parameter `name` in `def` is `count()` of it or its passing to
// `__pstext_literal`: the two reads of a runtime string that cost one instruction. Any other use (indexing, a
// comparison, a print, forwarding it) keeps the definition inline, where the string is a table index the lowering
// knows statically.
bool stringParameterUseIsCountOrLiteral(const Expr &expr,
                                        const std::string &name,
                                        const Expr *parent) {
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    if (parent == nullptr || parent->kind != Expr::Kind::Call || parent->args.size() != 1 ||
        &parent->args.front() != &expr) {
      return false;
    }
    const size_t slash = parent->name.rfind('/');
    const std::string_view leaf = slash == std::string::npos
                                      ? std::string_view(parent->name)
                                      : std::string_view(parent->name).substr(slash + 1);
    return leaf == "count" || leaf == "__pstext_literal";
  }
  for (const Expr &arg : expr.args) {
    if (!stringParameterUseIsCountOrLiteral(arg, name, &expr)) {
      return false;
    }
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    if (!stringParameterUseIsCountOrLiteral(bodyArg, name, &expr)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool stringParameterUsedOnlyAsCountOrLiteral(const Definition &def, const std::string &name) {
  for (const Expr &stmt : def.statements) {
    if (!stringParameterUseIsCountOrLiteral(stmt, name, nullptr)) {
      return false;
    }
  }
  return !def.returnExpr.has_value() ||
         stringParameterUseIsCountOrLiteral(*def.returnExpr, name, nullptr);
}

} // namespace primec::ir_lowerer

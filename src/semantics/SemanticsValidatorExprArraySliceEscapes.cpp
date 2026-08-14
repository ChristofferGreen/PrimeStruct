#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

// TODO-4609: conservative, lexical check for a slice view of a local array
// owner escaping its defining scope. Only recognizes the direct
// `slice(receiver, start, end)` call shape (matching how the other view
// escape checks in this validator, e.g. isStandaloneSoaRefCall, match the
// escaping expression by shape rather than tracking aliases through
// intermediate bindings).
bool SemanticsValidator::resolveEscapingArraySliceRoot(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &rootOut) {
  rootOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      !isSimpleCallName(expr, "slice") || expr.args.size() != 3) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  if (receiver.kind != Expr::Kind::Name) {
    return false;
  }
  if (findParamBinding(params, receiver.name) != nullptr) {
    // Parameter-owned array: caller retains ownership beyond this scope.
    return false;
  }
  if (locals.find(receiver.name) == locals.end()) {
    return false;
  }
  rootOut = receiver.name;
  return true;
}

} // namespace primec::semantics

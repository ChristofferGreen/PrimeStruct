#include "SemanticsValidator.h"

#include <string>

namespace primec::semantics {

bool SemanticsValidator::validateExprLateUnknownTargetFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprLateUnknownTargetFallbackContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.resolveMapTarget != nullptr && expr.isMethodCall &&
      (expr.name == "count" || expr.name == "contains" ||
       expr.name == "tryAt" || expr.name == "at" ||
       expr.name == "at_unsafe") &&
      !expr.args.empty() && context.resolveMapTarget(expr.args.front())) {
    const std::string canonicalMapMethodTarget =
        "/std/collections/map/" + expr.name;
    const std::string aliasMapMethodTarget = "/map/" + expr.name;
    Expr rewrittenMapMethodCall = expr;
    rewrittenMapMethodCall.isMethodCall = false;
    rewrittenMapMethodCall.namespacePrefix.clear();
    if (hasDeclaredDefinitionPath(canonicalMapMethodTarget) ||
        hasImportedDefinitionPath(canonicalMapMethodTarget)) {
      rewrittenMapMethodCall.name = canonicalMapMethodTarget;
    } else if (hasDeclaredDefinitionPath(aliasMapMethodTarget) ||
               hasImportedDefinitionPath(aliasMapMethodTarget)) {
      rewrittenMapMethodCall.name = aliasMapMethodTarget;
    } else {
      rewrittenMapMethodCall.name = canonicalMapMethodTarget;
    }
    handledOut = true;
    return validateExpr(params, locals, rewrittenMapMethodCall);
  }

  handledOut = true;
  error_ = "unknown call target: " + formatUnknownCallTarget(expr);
  return false;
}

} // namespace primec::semantics

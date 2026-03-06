#include "EmitterExprControlCountRewriteStep.h"

#include "EmitterHelpers.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlCountRewriteStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlCountRewriteResolveMethodPathFn &resolveMethodPath) {
  if (expr.isMethodCall || !isSimpleCallName(expr, "count") || expr.args.size() != 1 || nameMap.count(resolvedPath) != 0) {
    return std::nullopt;
  }
  if ((isArrayCountCall && isArrayCountCall(expr, localTypes)) ||
      (isMapCountCall && isMapCountCall(expr, localTypes)) ||
      (isStringCountCall && isStringCountCall(expr, localTypes))) {
    return std::nullopt;
  }
  if (!resolveMethodPath) {
    return std::nullopt;
  }
  Expr methodExpr = expr;
  methodExpr.isMethodCall = true;
  std::string methodPath;
  if (!resolveMethodPath(methodExpr, methodPath)) {
    return std::nullopt;
  }
  return methodPath;
}

} // namespace primec::emitter

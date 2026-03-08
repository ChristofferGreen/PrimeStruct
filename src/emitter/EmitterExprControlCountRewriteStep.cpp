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
  (void)localTypes;
  (void)isArrayCountCall;
  (void)isMapCountCall;
  (void)isStringCountCall;
  if (expr.isMethodCall || nameMap.count(resolvedPath) != 0) {
    return std::nullopt;
  }
  const bool isCountLikeCall = isSimpleCallName(expr, "count") && expr.args.size() == 1;
  const bool isCapacityLikeCall = isSimpleCallName(expr, "capacity") && expr.args.size() == 1;
  const bool isAccessLikeCall =
      (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe")) && expr.args.size() == 2;
  if (!isCountLikeCall && !isCapacityLikeCall && !isAccessLikeCall) {
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

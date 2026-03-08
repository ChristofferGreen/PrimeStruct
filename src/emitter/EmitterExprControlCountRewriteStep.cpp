#include "EmitterExprControlCountRewriteStep.h"

#include <utility>
#include <vector>

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
    const EmitterExprControlCountRewriteResolveMethodPathFn &resolveMethodPath,
    const EmitterExprControlCountRewriteIsCollectionAccessReceiverFn &isCollectionAccessReceiverExpr) {
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
  const bool hasNamedArgs = hasNamedArguments(expr.argNames);
  std::vector<size_t> receiverIndices{0};
  if (hasNamedArgs && expr.args.size() > 1) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      receiverIndices.push_back(i);
    }
  }
  const bool probePositionalReorderedAccessReceiver =
      isAccessLikeCall && !hasNamedArgs && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal ||
       (expr.args.front().kind == Expr::Kind::Name &&
        (!isCollectionAccessReceiverExpr || !isCollectionAccessReceiverExpr(expr.args.front()))));
  if (probePositionalReorderedAccessReceiver) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      receiverIndices.push_back(i);
    }
  }
  for (size_t receiverIndex : receiverIndices) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    if (receiverIndex != 0 && receiverIndex < methodExpr.args.size()) {
      std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
      if (methodExpr.argNames.size() < methodExpr.args.size()) {
        methodExpr.argNames.resize(methodExpr.args.size());
      }
      std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
    }
    std::string methodPath;
    if (resolveMethodPath(methodExpr, methodPath)) {
      return methodPath;
    }
  }
  return std::nullopt;
}

} // namespace primec::emitter

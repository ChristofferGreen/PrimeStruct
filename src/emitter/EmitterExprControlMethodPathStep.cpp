#include "EmitterExprControlMethodPathStep.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlMethodPathStep(
    const Expr &expr,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlResolveMethodPathFn &resolveMethodPath) {
  (void)localTypes;
  (void)isArrayCountCall;
  (void)isMapCountCall;
  (void)isStringCountCall;
  if (!expr.isMethodCall) {
    return std::nullopt;
  }
  if (!resolveMethodPath) {
    return std::nullopt;
  }
  std::string methodPath;
  if (!resolveMethodPath(methodPath)) {
    return std::nullopt;
  }
  return methodPath;
}

} // namespace primec::emitter

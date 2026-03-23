#include "SemanticsValidator.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprCollectionAccessDispatchContext(
    const ExprCollectionDispatchSetup &dispatchSetup,
    bool shouldBuiltinValidateBareMapContainsCall,
    bool shouldBuiltinValidateBareMapAccessCall,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    const std::function<bool(const Expr &)> &resolveMapTarget,
    ExprCollectionAccessDispatchContext &contextOut) {
  contextOut = {};
  contextOut.isNamespacedVectorHelperCall =
      dispatchSetup.isNamespacedVectorHelperCall;
  contextOut.isNamespacedMapHelperCall =
      dispatchSetup.isNamespacedMapHelperCall;
  contextOut.namespacedHelper = dispatchSetup.namespacedHelper;
  contextOut.shouldBuiltinValidateBareMapContainsCall =
      shouldBuiltinValidateBareMapContainsCall;
  contextOut.shouldBuiltinValidateBareMapAccessCall =
      shouldBuiltinValidateBareMapAccessCall;
  contextOut.resolveArrayTarget = dispatchResolvers.resolveArrayTarget;
  contextOut.resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  contextOut.resolveSoaVectorTarget = dispatchResolvers.resolveSoaVectorTarget;
  contextOut.resolveStringTarget = dispatchResolvers.resolveStringTarget;
  contextOut.resolveMapTarget = resolveMapTarget;
  contextOut.hasResolvableMapHelperPath = [&](const std::string &path) {
    return this->hasResolvableMapHelperPath(path);
  };
  contextOut.isIndexedArgsPackMapReceiverTarget = [&](const Expr &target) {
    return this->isIndexedArgsPackMapReceiverTarget(target, dispatchResolvers);
  };
}

} // namespace primec::semantics

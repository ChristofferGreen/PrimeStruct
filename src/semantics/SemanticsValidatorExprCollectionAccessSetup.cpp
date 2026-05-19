#include "SemanticsValidator.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprCollectionAccessDispatchContext(
    const ExprCollectionDispatchSetup &dispatchSetup,
    bool shouldBuiltinValidateBareKeyValueContainsCall,
    bool shouldBuiltinValidateBareKeyValueAccessCall,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    const std::function<bool(const Expr &)> &resolveMapTarget,
    ExprCollectionAccessDispatchContext &contextOut) {
  contextOut = {};
  contextOut.isNamespacedVectorHelperCall =
      dispatchSetup.isNamespacedVectorHelperCall;
  contextOut.isNamespacedMapHelperCall =
      dispatchSetup.isNamespacedMapHelperCall;
  contextOut.namespacedHelper = dispatchSetup.namespacedHelper;
  contextOut.shouldBuiltinValidateBareKeyValueContainsCall =
      shouldBuiltinValidateBareKeyValueContainsCall;
  contextOut.shouldBuiltinValidateBareKeyValueAccessCall =
      shouldBuiltinValidateBareKeyValueAccessCall;
  contextOut.resolveArrayTarget = dispatchResolvers.resolveArrayTarget;
  contextOut.resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  contextOut.resolveSoaVectorTarget = dispatchResolvers.resolveSoaVectorTarget;
  contextOut.resolveStringTarget = dispatchResolvers.resolveStringTarget;
  contextOut.resolveMapTarget = resolveMapTarget;
  contextOut.hasResolvableKeyValueHelperPath = [&](const std::string &path) {
    return this->hasResolvableKeyValueHelperPath(path);
  };
  contextOut.isIndexedArgsPackKeyValueReceiverTarget = [&](const Expr &target) {
    return this->isIndexedArgsPackKeyValueReceiverTarget(target, dispatchResolvers);
  };
}

} // namespace primec::semantics

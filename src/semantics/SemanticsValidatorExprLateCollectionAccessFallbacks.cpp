#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprLateCollectionAccessFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprLateCollectionAccessFallbackContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.dispatchResolvers == nullptr ||
      context.isNonCollectionStructAccessTarget == nullptr) {
    return true;
  }

  ExprCollectionAccessValidationContext collectionAccessValidationContext;
  collectionAccessValidationContext.isStdNamespacedVectorAccessCall =
      context.isStdNamespacedVectorAccessCall;
  collectionAccessValidationContext.shouldAllowStdAccessCompatibilityFallback =
      context.shouldAllowStdAccessCompatibilityFallback;
  collectionAccessValidationContext.hasStdNamespacedVectorAccessDefinition =
      context.hasStdNamespacedVectorAccessDefinition;
  collectionAccessValidationContext.isStdNamespacedMapAccessCall =
      context.isStdNamespacedMapAccessCall;
  collectionAccessValidationContext.hasStdNamespacedMapAccessDefinition =
      context.hasStdNamespacedMapAccessDefinition;
  collectionAccessValidationContext.shouldBuiltinValidateBareMapAccessCall =
      context.shouldBuiltinValidateBareMapAccessCall;
  collectionAccessValidationContext.resolveArgsPackAccessTarget =
      context.dispatchResolvers->resolveArgsPackAccessTarget;
  collectionAccessValidationContext.resolveVectorTarget =
      context.dispatchResolvers->resolveVectorTarget;
  collectionAccessValidationContext.resolveExperimentalVectorValueTarget =
      context.dispatchResolvers->resolveExperimentalVectorValueTarget;
  collectionAccessValidationContext.resolveArrayTarget =
      context.dispatchResolvers->resolveArrayTarget;
  collectionAccessValidationContext.resolveStringTarget =
      context.dispatchResolvers->resolveStringTarget;
  collectionAccessValidationContext.resolveMapKeyType =
      [&](const Expr &target, std::string &mapKeyTypeOut) {
        return this->resolveMapKeyType(target, *context.dispatchResolvers,
                                       mapKeyTypeOut);
      };
  collectionAccessValidationContext.resolveExperimentalMapTarget =
      context.dispatchResolvers->resolveExperimentalMapTarget;
  collectionAccessValidationContext.isIndexedArgsPackMapReceiverTarget =
      [&](const Expr &target) {
        return this->isIndexedArgsPackMapReceiverTarget(target,
                                                        *context.dispatchResolvers);
      };
  collectionAccessValidationContext.isNamedArgsPackMethodAccessCall =
      [&](const Expr &target) {
        return this->isNamedArgsPackMethodAccessCall(target,
                                                     *context.dispatchResolvers);
      };
  collectionAccessValidationContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
      [&](const Expr &target) {
        return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
            target, *context.dispatchResolvers);
      };
  collectionAccessValidationContext.isMapLikeBareAccessReceiverTarget =
      [&](const Expr &target) {
        return this->isMapLikeBareAccessReceiver(target, params, locals,
                                                 *context.dispatchResolvers);
      };
  collectionAccessValidationContext.isNonCollectionStructAccessTarget =
      context.isNonCollectionStructAccessTarget;
  collectionAccessValidationContext.tryRewriteBareMapHelperCall =
      [&](const Expr &target, const std::string &helperName,
          Expr &rewrittenOut) {
        return this->tryRewriteBareMapHelperCall(target, helperName,
                                                 *context.dispatchResolvers,
                                                 rewrittenOut);
      };
  return validateExprCollectionAccessFallbacks(
      params, locals, expr, resolved, resolvedMethod,
      collectionAccessValidationContext, handledOut);
}

} // namespace primec::semantics

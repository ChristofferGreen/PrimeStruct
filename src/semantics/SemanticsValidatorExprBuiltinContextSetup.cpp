#include "SemanticsValidator.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprLateBuiltinContext(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprLateBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.tryBuiltinContext.getDirectMapHelperCompatibilityPath =
      [&](const Expr &target) {
        return this->directMapHelperCompatibilityPath(
            target, params, locals, dispatchResolverAdapters);
      };
  contextOut.tryBuiltinContext.isIndexedArgsPackMapReceiverTarget =
      [&](const Expr &target) {
        return this->isIndexedArgsPackMapReceiverTarget(target,
                                                        dispatchResolvers);
      };
  contextOut.resultFileBuiltinContext
      .isNamedArgsPackWrappedFileBuiltinAccessCall =
      [&](const Expr &target) {
        return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
            target, dispatchResolvers);
      };
  contextOut.resultFileBuiltinContext.isStringExpr =
      [&](const Expr &target) {
        return this->isStringExprForArgumentValidation(target,
                                                       dispatchResolvers);
      };
}

void SemanticsValidator::prepareExprNamedArgumentBuiltinContext(
    bool hasVectorHelperCallResolution,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprNamedArgumentBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.hasVectorHelperCallResolution = hasVectorHelperCallResolution;
  contextOut.isNamedArgsPackMethodAccessCall = [&](const Expr &target) {
    return this->isNamedArgsPackMethodAccessCall(target, dispatchResolvers);
  };
  contextOut.isNamedArgsPackWrappedFileBuiltinAccessCall =
      [&](const Expr &target) {
        return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
            target, dispatchResolvers);
      };
  contextOut.isArrayNamespacedVectorCountCompatibilityCall =
      [&](const Expr &target) {
        return this->isArrayNamespacedVectorCountCompatibilityCall(
            target, dispatchResolvers);
      };
}

} // namespace primec::semantics

#include "SemanticsValidator.h"
// soa-surface-audit: exempt
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprLateBuiltinContext(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprLateBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.tryBuiltinContext.getDirectKeyValueHelperCompatibilityPath =
      [&](const Expr &target) {
        return this->directKeyValueHelperCompatibilityPath(
            target, params, locals, dispatchResolverAdapters);
      };
  contextOut.tryBuiltinContext.isIndexedArgsPackKeyValueReceiverTarget =
      [&](const Expr &target) {
        return this->isIndexedArgsPackKeyValueReceiverTarget(target,
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

void SemanticsValidator::prepareExprCountCapacityBuiltinContext(
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprCountCapacityBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.resolveVectorTarget =
      [&](const Expr &target, std::string &elemTypeOut) {
        return dispatchResolvers.resolveVectorTarget(target, elemTypeOut);
      };
  contextOut.dispatchResolvers = &dispatchResolvers;
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

void SemanticsValidator::prepareExprLateMapSoaBuiltinContext(
    bool shouldBuiltinValidateBareKeyValueContainsCall,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprLateMapSoaBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.shouldBuiltinValidateBareKeyValueContainsCall =
      shouldBuiltinValidateBareKeyValueContainsCall;
  contextOut.resolveVectorTarget =
      [&](const Expr &target, std::string &elemTypeOut) {
        return dispatchResolvers.resolveVectorTarget(target, elemTypeOut);
      };
  contextOut.resolveSoaVectorTarget =
      [&](const Expr &target, std::string &elemTypeOut) {
        return dispatchResolvers.resolveSoaVectorTarget(target, elemTypeOut);
      };
  contextOut.dispatchResolvers = &dispatchResolvers;
}

void SemanticsValidator::prepareExprLateFallbackBuiltinContext(
    bool isStdNamespacedVectorAccessCall,
    bool shouldAllowStdAccessCompatibilityFallback,
    bool hasStdNamespacedVectorAccessDefinition,
    bool isStdNamespacedMapAccessCall,
    bool hasStdNamespacedKeyValueAccessDefinition,
    bool shouldBuiltinValidateBareKeyValueAccessCall,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprLateFallbackBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =
      isStdNamespacedVectorAccessCall;
  contextOut.collectionAccessFallbackContext
      .shouldAllowStdAccessCompatibilityFallback =
      shouldAllowStdAccessCompatibilityFallback;
  contextOut.collectionAccessFallbackContext.hasStdNamespacedVectorAccessDefinition =
      hasStdNamespacedVectorAccessDefinition;
  contextOut.collectionAccessFallbackContext.isStdNamespacedMapAccessCall =
      isStdNamespacedMapAccessCall;
  contextOut.collectionAccessFallbackContext.hasStdNamespacedKeyValueAccessDefinition =
      hasStdNamespacedKeyValueAccessDefinition;
  contextOut.collectionAccessFallbackContext.shouldBuiltinValidateBareKeyValueAccessCall =
      shouldBuiltinValidateBareKeyValueAccessCall;
  contextOut.collectionAccessFallbackContext.isNonCollectionStructAccessTarget =
      [this](const std::string &targetPath) {
        const size_t slash = targetPath.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
          return false;
        }
        const std::string receiverPath = targetPath.substr(0, slash);
        if (receiverPath == "/array" || receiverPath == "/vector" ||
            receiverPath == canonicalVectorCompatibilityPrefixOrFallback() ||
            receiverPath == "/soa" || receiverPath == "/map" ||
            receiverPath == "/string") {
          return false;
        }
        return structNames_.count(receiverPath) > 0;
      };
  contextOut.collectionAccessFallbackContext.dispatchResolvers =
      &dispatchResolvers;
  contextOut.dispatchResolvers = &dispatchResolvers;
}

void SemanticsValidator::prepareExprLateCallCompatibilityContext(
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprLateCallCompatibilityContext &contextOut) {
  contextOut = {};
  contextOut.dispatchResolvers = &dispatchResolvers;
}

} // namespace primec::semantics

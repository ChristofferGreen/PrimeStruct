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

void SemanticsValidator::prepareExprCountCapacityMapBuiltinContext(
    bool shouldBuiltinValidateBareMapCountCall,
    bool isNamespacedMapCountCall,
    bool isResolvedMapCountCall,
    bool shouldBuiltinValidateStdNamespacedVectorCapacityCall,
    bool isStdNamespacedVectorCapacityCall,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprCountCapacityMapBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.shouldBuiltinValidateBareMapCountCall =
      shouldBuiltinValidateBareMapCountCall;
  contextOut.isNamespacedMapCountCall = isNamespacedMapCountCall;
  contextOut.isResolvedMapCountCall = isResolvedMapCountCall;
  contextOut.shouldBuiltinValidateStdNamespacedVectorCapacityCall =
      shouldBuiltinValidateStdNamespacedVectorCapacityCall;
  contextOut.isStdNamespacedVectorCapacityCall =
      isStdNamespacedVectorCapacityCall;
  contextOut.resolveVectorTarget =
      [&](const Expr &target, std::string &elemTypeOut) {
        return dispatchResolvers.resolveVectorTarget(target, elemTypeOut);
      };
  contextOut.resolveMapTarget =
      [&](const Expr &target) {
        std::string keyType;
        std::string valueType;
        return dispatchResolvers.resolveMapTarget(target, keyType, valueType);
      };
  contextOut.dispatchResolverAdapters = &dispatchResolverAdapters;
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
    bool shouldBuiltinValidateBareMapContainsCall,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprLateMapSoaBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.shouldBuiltinValidateBareMapContainsCall =
      shouldBuiltinValidateBareMapContainsCall;
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
    bool hasStdNamespacedMapAccessDefinition,
    bool shouldBuiltinValidateBareMapAccessCall,
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
  contextOut.collectionAccessFallbackContext.hasStdNamespacedMapAccessDefinition =
      hasStdNamespacedMapAccessDefinition;
  contextOut.collectionAccessFallbackContext.shouldBuiltinValidateBareMapAccessCall =
      shouldBuiltinValidateBareMapAccessCall;
  contextOut.collectionAccessFallbackContext.isNonCollectionStructAccessTarget =
      [this](const std::string &targetPath) {
        const size_t slash = targetPath.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
          return false;
        }
        const std::string receiverPath = targetPath.substr(0, slash);
        if (receiverPath == "/array" || receiverPath == "/vector" ||
            receiverPath == "/std/collections/vector" ||
            receiverPath == "/soa_vector" || receiverPath == "/map" ||
            receiverPath == "/std/collections/map" ||
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

void SemanticsValidator::prepareExprLateMapAccessBuiltinContext(
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    bool shouldBuiltinValidateBareMapContainsCall,
    bool shouldBuiltinValidateBareMapTryAtCall,
    bool shouldBuiltinValidateBareMapAccessCall,
    ExprLateMapAccessBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.dispatchResolvers = &dispatchResolvers;
  contextOut.shouldBuiltinValidateBareMapContainsCall =
      shouldBuiltinValidateBareMapContainsCall;
  contextOut.shouldBuiltinValidateBareMapTryAtCall =
      shouldBuiltinValidateBareMapTryAtCall;
  contextOut.shouldBuiltinValidateBareMapAccessCall =
      shouldBuiltinValidateBareMapAccessCall;
}

} // namespace primec::semantics

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
    bool shouldBuiltinValidateStdNamespacedVectorCountCall,
    bool isStdNamespacedVectorCountCall,
    bool shouldBuiltinValidateBareMapCountCall,
    bool isNamespacedMapCountCall,
    bool isResolvedMapCountCall,
    bool shouldBuiltinValidateStdNamespacedVectorCapacityCall,
    bool isStdNamespacedVectorCapacityCall,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    ExprCountCapacityMapBuiltinContext &contextOut) {
  contextOut = {};
  contextOut.shouldBuiltinValidateStdNamespacedVectorCountCall =
      shouldBuiltinValidateStdNamespacedVectorCountCall;
  contextOut.isStdNamespacedVectorCountCall = isStdNamespacedVectorCountCall;
  contextOut.shouldBuiltinValidateBareMapCountCall =
      shouldBuiltinValidateBareMapCountCall;
  contextOut.isNamespacedMapCountCall = isNamespacedMapCountCall;
  contextOut.isResolvedMapCountCall = isResolvedMapCountCall;
  contextOut.shouldBuiltinValidateStdNamespacedVectorCapacityCall =
      shouldBuiltinValidateStdNamespacedVectorCapacityCall;
  contextOut.isStdNamespacedVectorCapacityCall =
      isStdNamespacedVectorCapacityCall;
  contextOut.resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  contextOut.resolveMapTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    if ((dispatchResolvers.resolveMapTarget != nullptr &&
         dispatchResolvers.resolveMapTarget(target, keyType, valueType)) ||
        (dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
         dispatchResolvers.resolveExperimentalMapTarget(target, keyType,
                                                       valueType))) {
      return true;
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(target));
    if ((defIt == defMap_.end() || defIt->second == nullptr) &&
        !target.name.empty() && target.name.find('/') == std::string::npos) {
      defIt = defMap_.find("/" + target.name);
    }
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferredTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
      if (returnsMapCollectionType(inferredTypeText)) {
        return true;
      }
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          returnsMapCollectionType(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
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
  contextOut.resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  contextOut.resolveSoaVectorTarget = dispatchResolvers.resolveSoaVectorTarget;
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
      [&](const std::string &targetPath) {
        const size_t methodSlash = targetPath.find_last_of('/');
        return methodSlash != std::string::npos && methodSlash > 0 &&
               structNames_.count(targetPath.substr(0, methodSlash)) > 0;
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

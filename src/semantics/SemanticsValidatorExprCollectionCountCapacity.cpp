#include "SemanticsValidator.h"

#include <string>

namespace primec::semantics {

bool SemanticsValidator::resolveExprCollectionCountCapacityTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprCollectionCountCapacityDispatchContext &context,
    bool &handledOut,
    std::string &resolved,
    bool &resolvedMethod,
    bool &usedMethodTarget,
    bool &hasMethodReceiverIndex,
    size_t &methodReceiverIndex) {
  handledOut = false;

  if (hasNamedArguments(expr.argNames) &&
      expr.args.size() == 1 &&
      defMap_.find(resolved) == defMap_.end() &&
      context.isNamespacedVectorHelperCall &&
      (context.namespacedHelper == "count" || context.namespacedHelper == "capacity")) {
    handledOut = true;
    bool isBuiltinMethod = false;
    std::string methodResolved;
    if (resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(),
                            context.namespacedHelper, methodResolved, isBuiltinMethod) &&
        !isBuiltinMethod && defMap_.find(methodResolved) != defMap_.end()) {
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
      resolved = methodResolved;
      resolvedMethod = false;
    }
    return true;
  }

  if (context.isDirectStdNamespacedVectorCountWrapperMapTarget) {
    handledOut = true;
    error_ = "template arguments required for /std/collections/vector/count";
    return false;
  }

  Expr rewrittenVectorHelperCall;
  if (context.tryRewriteBareVectorHelperCall != nullptr &&
      (context.tryRewriteBareVectorHelperCall("count", rewrittenVectorHelperCall) ||
       context.tryRewriteBareVectorHelperCall("capacity", rewrittenVectorHelperCall))) {
    handledOut = true;
    return validateExpr(params, locals, rewrittenVectorHelperCall);
  }

  if ((context.isStdNamespacedVectorCountCall && expr.args.size() == 1 &&
       context.resolveMapTarget != nullptr && context.resolveMapTarget(expr.args.front())) &&
      (defMap_.find("/std/collections/vector/count") == defMap_.end() ||
       hasImportedDefinitionPath("/std/collections/vector/count")) &&
      context.hasStdNamespacedVectorCountAliasDefinition) {
    handledOut = true;
    error_ = "count requires vector target";
    return false;
  }

  auto resolveCountMethod = [&](bool requireSingleArg) -> bool {
    if (hasNamedArguments(expr.argNames) ||
        (context.isStdNamespacedVectorCountCall &&
         !context.shouldBuiltinValidateStdNamespacedVectorCountCall) ||
        expr.args.empty()) {
      return false;
    }
    if (requireSingleArg && expr.args.size() != 1) {
      return false;
    }
    if (!requireSingleArg && expr.args.size() == 1) {
      return false;
    }
    if (context.prefersCanonicalVectorCountAliasDefinition ||
        (context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
         context.isArrayNamespacedVectorCountCompatibilityCall(expr))) {
      return false;
    }
    const bool isCountLike =
        isVectorBuiltinName(expr, "count") || context.isStdNamespacedMapCountCall ||
        context.isNamespacedMapCountCall || context.isUnnamespacedMapCountFallbackCall ||
        context.isResolvedMapCountCall;
    if (!isCountLike) {
      return false;
    }
    if (requireSingleArg) {
      if (!((defMap_.find(resolved) == defMap_.end() && !context.isStdNamespacedMapCountCall) ||
            (context.isNamespacedVectorCountCall && !context.isStdNamespacedVectorCountCall) ||
            context.isStdNamespacedMapCountCall || context.isNamespacedMapCountCall ||
            context.isUnnamespacedMapCountFallbackCall || context.isResolvedMapCountCall)) {
        return false;
      }
    } else {
      if (!(defMap_.find(resolved) != defMap_.end() || context.isStdNamespacedMapCountCall ||
            context.isNamespacedMapCountCall || context.isUnnamespacedMapCountFallbackCall ||
            context.isResolvedMapCountCall)) {
        return false;
      }
    }

    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    bool isBuiltinMethod = false;
    std::string methodResolved;
    if (context.isUnnamespacedMapCountFallbackCall &&
        !hasDeclaredDefinitionPath("/std/collections/map/count") &&
        !hasDeclaredDefinitionPath("/map/count") &&
        !hasImportedDefinitionPath("/std/collections/map/count") &&
        context.resolveMapTarget != nullptr &&
        context.resolveMapTarget(expr.args.front())) {
      methodResolved = "/std/collections/map/count";
      isBuiltinMethod = true;
    } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "count",
                                    methodResolved, isBuiltinMethod)) {
      (void)validateExpr(params, locals, expr.args.front());
      return false;
    }
    if (isBuiltinMethod && methodResolved == "/std/collections/map/count" &&
        !hasDeclaredDefinitionPath("/map/count") &&
        !hasImportedDefinitionPath("/std/collections/map/count") &&
        !hasDeclaredDefinitionPath("/std/collections/map/count") &&
        !context.shouldBuiltinValidateBareMapCountCall &&
        !(context.resolveMapTarget != nullptr &&
          context.resolveMapTarget(expr.args.front()))) {
      error_ = "unknown call target: /std/collections/map/count";
      return false;
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
      error_ = "unknown method: " + methodResolved;
      return false;
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  };

  if (resolveCountMethod(true)) {
    return true;
  }
  if (handledOut) {
    return false;
  }
  if (resolveCountMethod(false)) {
    return true;
  }
  if (handledOut) {
    return false;
  }

  auto resolveCapacityMethod = [&](bool requireSingleArg) -> bool {
    if (hasNamedArguments(expr.argNames) ||
        (context.isStdNamespacedVectorCapacityCall &&
         !context.shouldBuiltinValidateStdNamespacedVectorCapacityCall) ||
        !isVectorBuiltinName(expr, "capacity")) {
      return false;
    }
    if (requireSingleArg) {
      if (expr.args.size() != 1 ||
          !(defMap_.find(resolved) == defMap_.end() || context.isNamespacedVectorCapacityCall)) {
        return false;
      }
    } else {
      if (expr.args.empty() || expr.args.size() == 1 || defMap_.find(resolved) == defMap_.end()) {
        return false;
      }
      if (!context.isNamespacedVectorHelperCall) {
        return false;
      }
    }

    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    bool isBuiltinMethod = false;
    std::string methodResolved;
    if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "capacity",
                             methodResolved, isBuiltinMethod)) {
      (void)validateExpr(params, locals, expr.args.front());
      return false;
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
      if (requireSingleArg &&
          (context.isNonCollectionStructCapacityTarget == nullptr ||
           !context.isNonCollectionStructCapacityTarget(methodResolved))) {
        if (context.promoteCapacityToBuiltinValidation != nullptr) {
          context.promoteCapacityToBuiltinValidation(expr.args.front(), methodResolved,
                                                     isBuiltinMethod, false);
        }
      }
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
      error_ = "unknown method: " + methodResolved;
      return false;
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  };

  if (resolveCapacityMethod(false)) {
    return true;
  }
  if (handledOut) {
    return false;
  }
  if (resolveCapacityMethod(true)) {
    return true;
  }
  if (handledOut) {
    return false;
  }

  return true;
}

} // namespace primec::semantics

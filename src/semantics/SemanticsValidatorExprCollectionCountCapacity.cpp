#include "SemanticsValidator.h"
#include "SemanticsVectorCompatibilityHelpers.h"

#include <optional>
#include <string>
#include <utility>

namespace primec::semantics {

bool SemanticsValidator::resolveExprCollectionCountCapacityTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprCollectionCountCapacityDispatchContext &context,
    bool &handledOut,
    std::optional<Expr> &rewrittenExprOut,
    std::string &resolved,
    bool &resolvedMethod,
    bool &usedMethodTarget,
    bool &hasMethodReceiverIndex,
    size_t &methodReceiverIndex) {
  handledOut = false;
  rewrittenExprOut.reset();
  if (!expr.isMethodCall && defMap_.find(resolved) != defMap_.end()) {
    const size_t lastSlash = resolved.find_last_of('/');
    const size_t instantiationPos =
        resolved.find("__t", lastSlash == std::string::npos ? 0 : lastSlash + 1);
    if (instantiationPos != std::string::npos) {
      const std::string helperName =
          resolved.substr(lastSlash == std::string::npos ? 0 : lastSlash + 1,
                          instantiationPos - lastSlash - 1);
      if (helperName == "count" || helperName == "capacity") {
        return true;
      }
    }
  }

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

  Expr rewrittenVectorHelperCall;
  if (context.tryRewriteBareVectorHelperCall != nullptr &&
      (context.tryRewriteBareVectorHelperCall("count", rewrittenVectorHelperCall) ||
       context.tryRewriteBareVectorHelperCall("capacity", rewrittenVectorHelperCall))) {
    handledOut = true;
    rewrittenExprOut = std::move(rewrittenVectorHelperCall);
    return true;
  }
  {
    if (const std::string stdNamespacedVectorCountDiagnosticMessage =
        classifyStdNamespacedVectorCountDiagnosticMessage(
            false,
            context.isDirectStdNamespacedVectorCountWrapperMapTarget,
            isUndeclaredStdNamespacedVectorCompatibilityDirectCall(
                expr.isMethodCall,
                resolveCalleePath(expr),
                "count",
                hasDeclaredDefinitionPath("/std/collections/vector/count")),
            expr.args.size() == 1 &&
                context.resolveMapTarget != nullptr &&
                context.resolveMapTarget(expr.args.front()),
            isUnresolvableStdNamespacedVectorCompatibilityDirectCall(
                expr.isMethodCall,
                resolveCalleePath(expr),
                "count",
                hasDeclaredDefinitionPath("/std/collections/vector/count") ||
                    hasImportedDefinitionPath(
                        "/std/collections/vector/count")));
        !stdNamespacedVectorCountDiagnosticMessage.empty()) {
      handledOut = true;
      return failExprDiagnostic(expr,
                                stdNamespacedVectorCountDiagnosticMessage);
    }
  }
  const auto failRemovedRootedVectorDirectCall = [&]() -> bool {
    const std::string removedRootedVectorDirectCallDiagnostic =
        getRemovedRootedVectorDirectCallDiagnostic(expr);
    if (removedRootedVectorDirectCallDiagnostic.empty()) {
      return false;
    }
    return failExprDiagnostic(expr, removedRootedVectorDirectCallDiagnostic);
  };
  const auto normalizeInstantiatedCollectionMethodTarget =
      [&](std::string &methodResolved, bool isBuiltinMethod) {
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
        resolved.rfind(methodResolved + "__t", 0) == 0) {
      methodResolved = resolved;
    }
  };
  const auto tryResolveCollectionMethodFromSurface =
      [&](bool routesThroughMethodSurface, bool matchesSurfaceRoute,
          auto &&resolveMethodTarget) -> std::optional<bool> {
    if (!routesThroughMethodSurface || !matchesSurfaceRoute) {
      return std::nullopt;
    }
    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    const Expr &receiver = expr.args.front();
    bool isBuiltinMethod = false;
    std::string methodResolved;
    if (!resolveMethodTarget(receiver, isBuiltinMethod, methodResolved)) {
      return false;
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  };
  const auto resolveCountMethodTargetFromReceiver =
      [&](const Expr &receiver, bool &isBuiltinMethod,
          std::string &methodResolved) -> bool {
    const std::string stdlibMapCountMethodTarget =
        "/std/collections/map/count";
    if (context.isUnnamespacedMapCountFallbackCall &&
        !hasDeclaredDefinitionPath("/map/count") &&
        !hasDeclaredDefinitionPath("/std/collections/map/count") &&
        !hasImportedDefinitionPath("/std/collections/map/count") &&
        context.resolveMapTarget != nullptr &&
        context.resolveMapTarget(receiver)) {
      methodResolved = stdlibMapCountMethodTarget;
      isBuiltinMethod = true;
    } else if (resolveVectorHelperMethodTarget(params, locals, receiver, "count",
                                               methodResolved)) {
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      if (hasDeclaredDefinitionPath(methodResolved) ||
          hasImportedDefinitionPath(methodResolved)) {
        isBuiltinMethod = false;
      } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix,
                                      receiver, "count", methodResolved,
                                      isBuiltinMethod)) {
        if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
            expr.args.empty()) {
          (void)validateExpr(params, locals, receiver);
          return false;
        }
        if (context.resolveMapTarget != nullptr &&
            context.resolveMapTarget(receiver)) {
          methodResolved = stdlibMapCountMethodTarget;
          error_.clear();
          isBuiltinMethod = false;
        } else {
          std::string typeName;
          if (receiver.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding =
                    findParamBinding(params, receiver.name)) {
              typeName = paramBinding->typeName;
            } else if (auto it = locals.find(receiver.name);
                       it != locals.end()) {
              typeName = it->second.typeName;
            }
          }
          if (typeName.empty()) {
            typeName = inferPointerLikeCallReturnType(receiver, params, locals);
          }
          if (typeName.empty()) {
            if (isPointerExpr(receiver, params, locals)) {
              typeName = "Pointer";
            } else if (isPointerLikeExpr(receiver, params, locals)) {
              typeName = "Reference";
            }
          }
          if (typeName != "Pointer" && typeName != "Reference") {
            (void)validateExpr(params, locals, receiver);
            return false;
          }
          methodResolved = "/" + typeName + "/count";
          error_.clear();
          isBuiltinMethod = false;
        }
      }
    } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix,
                                    receiver, "count", methodResolved,
                                    isBuiltinMethod)) {
      if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
          expr.args.empty()) {
        (void)validateExpr(params, locals, receiver);
        return false;
      }
      if (context.resolveMapTarget != nullptr &&
          context.resolveMapTarget(receiver)) {
        methodResolved = stdlibMapCountMethodTarget;
        error_.clear();
        isBuiltinMethod = false;
      } else {
        std::string typeName;
        if (receiver.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding =
                  findParamBinding(params, receiver.name)) {
            typeName = paramBinding->typeName;
          } else if (auto it = locals.find(receiver.name);
                     it != locals.end()) {
            typeName = it->second.typeName;
          }
        }
        if (typeName.empty()) {
          typeName = inferPointerLikeCallReturnType(receiver, params, locals);
        }
        if (typeName.empty()) {
          if (isPointerExpr(receiver, params, locals)) {
            typeName = "Pointer";
          } else if (isPointerLikeExpr(receiver, params, locals)) {
            typeName = "Reference";
          }
        }
        if (typeName != "Pointer" && typeName != "Reference") {
          (void)validateExpr(params, locals, receiver);
          return false;
        }
        methodResolved = "/" + typeName + "/count";
        error_.clear();
        isBuiltinMethod = false;
      }
    }
    normalizeInstantiatedCollectionMethodTarget(methodResolved, isBuiltinMethod);
    if ((!expr.isMethodCall &&
         expr.args.size() == 1 &&
         receiver.kind == Expr::Kind::Name &&
         methodResolved == "/map/count" &&
         !hasImportedDefinitionPath("/count") &&
         !hasDeclaredDefinitionPath("/count") &&
         !hasDeclaredDefinitionPath("/std/collections/map/count") &&
         !hasImportedDefinitionPath("/std/collections/map/count")) ||
        (isBuiltinMethod &&
         methodResolved == stdlibMapCountMethodTarget &&
         !hasDeclaredDefinitionPath("/std/collections/map/count") &&
         !hasImportedDefinitionPath("/std/collections/map/count") &&
         !context.shouldBuiltinValidateBareMapCountCall)) {
      return failExprDiagnostic(expr,
                                "unknown call target: " +
                                    stdlibMapCountMethodTarget);
    }
    if (failRemovedRootedVectorDirectCall()) {
      return false;
    }
    if (!isBuiltinMethod && !hasDeclaredDefinitionPath(methodResolved) &&
        !hasImportedDefinitionPath(methodResolved)) {
      return failExprDiagnostic(expr,
                                "unknown method: " + methodResolved);
    }
    return true;
  };
  const bool routesThroughVectorCountMethodSurface =
      !(hasNamedArguments(expr.argNames) ||
        isUnimportedStdNamespacedVectorCompatibilityDirectCall(
            expr.isMethodCall,
            resolveCalleePath(expr),
            "count",
            hasImportedDefinitionPath("/std/collections/vector/count")) ||
        expr.args.empty()) &&
      !(context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
        context.isArrayNamespacedVectorCountCompatibilityCall(expr)) &&
      (isVectorBuiltinName(expr, "count") ||
       context.isStdNamespacedMapCountCall ||
       context.isNamespacedMapCountCall ||
       context.isUnnamespacedMapCountFallbackCall ||
       context.isResolvedMapCountCall);
  if (std::optional<bool> resolvedCountMethod =
          tryResolveCollectionMethodFromSurface(
              routesThroughVectorCountMethodSurface,
              expr.args.size() == 1 &&
                  ((defMap_.find(resolved) == defMap_.end() &&
                    !context.isStdNamespacedMapCountCall) ||
                   (!isStdNamespacedVectorCompatibilityDirectCall(
                        expr.isMethodCall, resolveCalleePath(expr), "count") &&
                    context.isNamespacedVectorHelperCall &&
                    context.namespacedHelper == "count" &&
                    isVectorBuiltinName(expr, "count") &&
                    expr.args.size() == 1 &&
                    !hasDefinitionPath(resolved) &&
                    !(context.isArrayNamespacedVectorCountCompatibilityCall !=
                          nullptr &&
                      context.isArrayNamespacedVectorCountCompatibilityCall(
                          expr))) ||
                   context.isStdNamespacedMapCountCall ||
                   context.isNamespacedMapCountCall ||
                   context.isUnnamespacedMapCountFallbackCall ||
                   context.isResolvedMapCountCall),
              resolveCountMethodTargetFromReceiver)) {
    return *resolvedCountMethod;
  }
  if (std::optional<bool> resolvedCountMethod =
          tryResolveCollectionMethodFromSurface(
              routesThroughVectorCountMethodSurface,
              expr.args.size() != 1 &&
                  (defMap_.find(resolved) != defMap_.end() ||
                   context.isStdNamespacedMapCountCall ||
                   context.isNamespacedMapCountCall ||
                   context.isUnnamespacedMapCountFallbackCall ||
                   context.isResolvedMapCountCall),
              resolveCountMethodTargetFromReceiver)) {
    return *resolvedCountMethod;
  }

  const auto resolveCapacityMethodTargetFromReceiver =
      [&](const Expr &receiver, bool &isBuiltinMethod,
          std::string &methodResolved) -> bool {
    if (resolveVectorHelperMethodTarget(params, locals, receiver, "capacity",
                                        methodResolved)) {
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      if (hasDeclaredDefinitionPath(methodResolved) ||
          hasImportedDefinitionPath(methodResolved)) {
        isBuiltinMethod = false;
      } else if (isStdNamespacedVectorCompatibilityHelperPath(
                     resolveCalleePath(expr), "capacity")) {
        methodResolved = "/std/collections/vector/capacity";
        isBuiltinMethod = true;
      } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix,
                                      receiver, "capacity", methodResolved,
                                      isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiver);
        return false;
      }
    } else if (isStdNamespacedVectorCompatibilityHelperPath(
                   resolveCalleePath(expr), "capacity")) {
      methodResolved = "/std/collections/vector/capacity";
      isBuiltinMethod = true;
    } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix,
                                    receiver, "capacity", methodResolved,
                                    isBuiltinMethod)) {
      (void)validateExpr(params, locals, receiver);
      return false;
    }
    normalizeInstantiatedCollectionMethodTarget(methodResolved, isBuiltinMethod);
    if (!isBuiltinMethod && !hasDeclaredDefinitionPath(methodResolved) &&
        !hasImportedDefinitionPath(methodResolved)) {
      if ((context.isNonCollectionStructCapacityTarget == nullptr ||
           !context.isNonCollectionStructCapacityTarget(methodResolved)) &&
          context.promoteCapacityToBuiltinValidation != nullptr) {
        context.promoteCapacityToBuiltinValidation(receiver, methodResolved,
                                                   isBuiltinMethod, false);
      }
    }
    if (!isBuiltinMethod && !hasDeclaredDefinitionPath(methodResolved) &&
        !hasImportedDefinitionPath(methodResolved)) {
      return failExprDiagnostic(expr,
                                "unknown method: " + methodResolved);
    }
    if (failRemovedRootedVectorDirectCall()) {
      return false;
    }
    return true;
  };
  const bool routesThroughVectorCapacityMethodSurface =
      !(hasNamedArguments(expr.argNames) ||
        isUnimportedStdNamespacedVectorCompatibilityDirectCall(
            expr.isMethodCall,
            resolveCalleePath(expr),
            "capacity",
            hasImportedDefinitionPath("/std/collections/vector/capacity")) ||
        !isVectorBuiltinName(expr, "capacity"));
  if (std::optional<bool> resolvedCapacityMethod =
          tryResolveCollectionMethodFromSurface(
              routesThroughVectorCapacityMethodSurface,
              !(expr.args.empty() || expr.args.size() == 1 ||
                defMap_.find(resolved) == defMap_.end()) &&
                  context.isNamespacedVectorHelperCall,
              resolveCapacityMethodTargetFromReceiver)) {
    return *resolvedCapacityMethod;
  }
  if (std::optional<bool> resolvedCapacityMethod =
          tryResolveCollectionMethodFromSurface(
              routesThroughVectorCapacityMethodSurface,
              expr.args.size() == 1 &&
                  (defMap_.find(resolved) == defMap_.end() ||
                   context.isNamespacedVectorCapacityCall),
              resolveCapacityMethodTargetFromReceiver)) {
    return *resolvedCapacityMethod;
  }

  return true;
}

} // namespace primec::semantics

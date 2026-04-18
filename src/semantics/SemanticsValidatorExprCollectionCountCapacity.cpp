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
    (void)failExprDiagnostic(expr, removedRootedVectorDirectCallDiagnostic);
    return true;
  };
  const auto failUnknownCollectionMethodTarget =
      [&](bool isBuiltinMethod, const std::string &methodResolved) {
    if (isBuiltinMethod || hasDeclaredDefinitionPath(methodResolved) ||
        hasImportedDefinitionPath(methodResolved)) {
      return false;
    }
    (void)failExprDiagnostic(expr, "unknown method: " + methodResolved);
    return true;
  };
  const auto tryResolveCollectionMethodTargetOrElse =
      [&](const Expr &receiver, const char *methodName,
          std::string &methodResolved, bool &isBuiltinMethod,
          auto &&handleResolveMiss) -> bool {
    if (resolveMethodTarget(params, locals, expr.namespacePrefix, receiver,
                            methodName, methodResolved, isBuiltinMethod)) {
      return true;
    }
    return handleResolveMiss(receiver, isBuiltinMethod, methodResolved);
  };
  const auto finalizeCollectionMethodTarget =
      [&](std::string &methodResolved, bool &isBuiltinMethod,
          auto &&beforeFailureChecks,
          auto &&failPrimary,
          auto &&failSecondary) -> bool {
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
        resolved.rfind(methodResolved + "__t", 0) == 0) {
      methodResolved = resolved;
    }
    if (!beforeFailureChecks(methodResolved, isBuiltinMethod)) {
      return false;
    }
    if (failPrimary(methodResolved, isBuiltinMethod)) {
      return false;
    }
    if (failSecondary(methodResolved, isBuiltinMethod)) {
      return false;
    }
    return true;
  };
  const auto tryResolveCollectionMethodTargetFromHelperRouteOrFinalize =
      [&](const Expr &receiver, const char *methodName,
          std::string &methodResolved, bool &isBuiltinMethod,
          auto &&handleVisibleHelperHit,
          auto &&handleHelperMiss,
          auto &&finalizeMethodTarget) -> bool {
    if (resolveVectorHelperMethodTarget(params, locals, receiver, methodName,
                                        methodResolved) &&
        ((methodResolved = preferVectorStdlibHelperPath(methodResolved)),
         (hasDeclaredDefinitionPath(methodResolved) ||
          hasImportedDefinitionPath(methodResolved)))) {
      isBuiltinMethod = false;
      if (!handleVisibleHelperHit(receiver, methodResolved, isBuiltinMethod)) {
        return false;
      }
    } else if (!handleHelperMiss(receiver, methodResolved, isBuiltinMethod)) {
      return false;
    }
    return finalizeMethodTarget(methodResolved, isBuiltinMethod);
  };
  const auto tryResolveCollectionMethodFromSurfaceRoutes =
      [&](bool routesThroughMethodSurface,
          bool matchesPrimarySurfaceRoute,
          bool matchesSecondarySurfaceRoute,
          auto &&resolveMethodTarget) -> std::optional<bool> {
    const auto tryResolveMatchingSurfaceRoute =
        [&](bool matchesSurfaceRoute) -> std::optional<bool> {
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
    if (std::optional<bool> resolvedMethod =
            tryResolveMatchingSurfaceRoute(matchesPrimarySurfaceRoute)) {
      return resolvedMethod;
    }
    return tryResolveMatchingSurfaceRoute(matchesSecondarySurfaceRoute);
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
    } else {
      return tryResolveCollectionMethodTargetFromHelperRouteOrFinalize(
          receiver, "count", methodResolved, isBuiltinMethod,
          [&](const Expr &, std::string &, bool &) { return true; },
          [&](const Expr &receiver, std::string &methodResolved,
              bool &isBuiltinMethod) {
            return tryResolveCollectionMethodTargetOrElse(
                receiver, "count", methodResolved, isBuiltinMethod,
                [&](const Expr &receiver, bool &isBuiltinMethod,
                    std::string &methodResolved) {
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
                      typeName =
                          inferPointerLikeCallReturnType(receiver, params, locals);
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
                  return true;
                });
          },
          [&](std::string &, bool &) { return true; });
    }
    return finalizeCollectionMethodTarget(
        methodResolved, isBuiltinMethod,
        [&](const std::string &methodResolved, bool isBuiltinMethod) {
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
          return true;
        },
        [&](const std::string &, bool) {
          return failRemovedRootedVectorDirectCall();
        },
        [&](const std::string &methodResolved, bool isBuiltinMethod) {
          return failUnknownCollectionMethodTarget(isBuiltinMethod,
                                                   methodResolved);
        });
  };
  if (std::optional<bool> resolvedCountMethod =
          tryResolveCollectionMethodFromSurfaceRoutes(
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
                   context.isResolvedMapCountCall),
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
              expr.args.size() != 1 &&
                  (defMap_.find(resolved) != defMap_.end() ||
                   context.isStdNamespacedMapCountCall ||
                   context.isNamespacedMapCountCall ||
                   context.isUnnamespacedMapCountFallbackCall ||
                   context.isResolvedMapCountCall),
              resolveCountMethodTargetFromReceiver)) {
    return *resolvedCountMethod;
  }

  if (std::optional<bool> resolvedCapacityMethod =
          tryResolveCollectionMethodFromSurfaceRoutes(
              !(hasNamedArguments(expr.argNames) ||
                isUnimportedStdNamespacedVectorCompatibilityDirectCall(
                    expr.isMethodCall,
                    resolveCalleePath(expr),
                    "capacity",
                    hasImportedDefinitionPath("/std/collections/vector/capacity")) ||
                !isVectorBuiltinName(expr, "capacity")),
              !(expr.args.empty() || expr.args.size() == 1 ||
                defMap_.find(resolved) == defMap_.end()) &&
                  context.isNamespacedVectorHelperCall,
              expr.args.size() == 1 &&
                  (defMap_.find(resolved) == defMap_.end() ||
                   context.isNamespacedVectorCapacityCall),
              [&](const Expr &receiver, bool &isBuiltinMethod,
                  std::string &methodResolved) -> bool {
                return tryResolveCollectionMethodTargetFromHelperRouteOrFinalize(
                    receiver, "capacity", methodResolved, isBuiltinMethod,
                    [&](const Expr &, std::string &, bool &) {
                      if (isStdNamespacedVectorCompatibilityHelperPath(
                              resolveCalleePath(expr), "capacity")) {
                        methodResolved = "/std/collections/vector/capacity";
                        isBuiltinMethod = true;
                      }
                      return true;
                    },
                    [&](const Expr &receiver, std::string &methodResolved,
                        bool &isBuiltinMethod) {
                      if (isStdNamespacedVectorCompatibilityHelperPath(
                              resolveCalleePath(expr), "capacity")) {
                        methodResolved = "/std/collections/vector/capacity";
                        isBuiltinMethod = true;
                        return true;
                      }
                      return tryResolveCollectionMethodTargetOrElse(
                          receiver, "capacity", methodResolved, isBuiltinMethod,
                          [&](const Expr &receiver, bool &, std::string &) {
                            (void)validateExpr(params, locals, receiver);
                            return false;
                          });
                    },
                    [&](std::string &methodResolved, bool &isBuiltinMethod) {
                      return finalizeCollectionMethodTarget(
                          methodResolved, isBuiltinMethod,
                          [&](std::string &methodResolved, bool &isBuiltinMethod) {
                            if (!isBuiltinMethod &&
                                !hasDeclaredDefinitionPath(methodResolved) &&
                                !hasImportedDefinitionPath(methodResolved)) {
                              if ((context.isNonCollectionStructCapacityTarget ==
                                       nullptr ||
                                   !context.isNonCollectionStructCapacityTarget(
                                       methodResolved)) &&
                                  context.promoteCapacityToBuiltinValidation !=
                                      nullptr) {
                                context.promoteCapacityToBuiltinValidation(
                                    receiver, methodResolved, isBuiltinMethod,
                                    false);
                              }
                            }
                            return true;
                          },
                          [&](const std::string &methodResolved,
                              bool isBuiltinMethod) {
                            return failUnknownCollectionMethodTarget(
                                isBuiltinMethod, methodResolved);
                          },
                          [&](const std::string &, bool) {
                            return failRemovedRootedVectorDirectCall();
                          });
                    });
              })) {
    return *resolvedCapacityMethod;
  }

  return true;
}

} // namespace primec::semantics

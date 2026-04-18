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
  const bool routesThroughMapCountCallSurface =
      context.isStdNamespacedMapCountCall ||
      context.isNamespacedMapCountCall ||
      context.isUnnamespacedMapCountFallbackCall ||
      context.isResolvedMapCountCall;
  const bool isArrayNamespacedVectorCountCompatibilityActive =
      context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
      context.isArrayNamespacedVectorCountCompatibilityCall(expr);
  const bool routesThroughNamespacedVectorCountFallback =
      !isStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall, resolveCalleePath(expr), "count") &&
      context.isNamespacedVectorHelperCall &&
      context.namespacedHelper == "count" &&
      isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
      !hasDefinitionPath(resolved) &&
      !isArrayNamespacedVectorCountCompatibilityActive;
  const bool hasResolvedCountDefinitionTarget =
      defMap_.find(resolved) != defMap_.end();
  const bool matchesSingleArgCountRouteShape =
      expr.args.size() == 1 &&
      ((!hasResolvedCountDefinitionTarget &&
        !context.isStdNamespacedMapCountCall) ||
       routesThroughNamespacedVectorCountFallback ||
       routesThroughMapCountCallSurface);
  const bool matchesMultiArgCountRouteShape =
      expr.args.size() != 1 &&
      (hasResolvedCountDefinitionTarget ||
       routesThroughMapCountCallSurface);
  const bool matchesCountRouteArgShape =
      matchesSingleArgCountRouteShape ||
      matchesMultiArgCountRouteShape;
  const bool routesThroughCountMethodSurface =
      isVectorBuiltinName(expr, "count") || routesThroughMapCountCallSurface;
  const bool violatesCountMethodSurfacePreconditions =
      hasNamedArguments(expr.argNames) ||
      isUnimportedStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "count",
          hasImportedDefinitionPath("/std/collections/vector/count")) ||
      expr.args.empty();
  const bool matchesCountMethodSurfaceRoute =
      !violatesCountMethodSurfacePreconditions &&
      !isArrayNamespacedVectorCountCompatibilityActive &&
      routesThroughCountMethodSurface && matchesCountRouteArgShape;
  if (matchesCountMethodSurfaceRoute) {
    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    const Expr &receiver = expr.args.front();
    const bool resolvesMapCountReceiver =
        context.resolveMapTarget != nullptr &&
        context.resolveMapTarget(receiver);
    const std::string bareCountTargetPath = "/count";
    const std::string bareMapCountTargetPath = "/map/count";
    const std::string stdlibMapCountTargetPath =
        "/std/collections/map/count";
    const bool lacksVisibleBareCountDefinition =
        !hasImportedDefinitionPath(bareCountTargetPath) &&
        !hasDeclaredDefinitionPath(bareCountTargetPath);
    const bool lacksVisibleStdlibMapCountDefinition =
        !hasDeclaredDefinitionPath(stdlibMapCountTargetPath) &&
        !hasImportedDefinitionPath(stdlibMapCountTargetPath);
    const bool routesThroughStdlibMapCountFallback =
        context.isUnnamespacedMapCountFallbackCall &&
        !hasDeclaredDefinitionPath(bareMapCountTargetPath) &&
        lacksVisibleStdlibMapCountDefinition &&
        resolvesMapCountReceiver;
    bool isBuiltinMethod = false;
    std::string methodResolved;
    {
                if (routesThroughStdlibMapCountFallback) {
                  methodResolved = stdlibMapCountTargetPath;
                  isBuiltinMethod = true;
                } else {
                  if (resolveVectorHelperMethodTarget(
                          params, locals, receiver, "count", methodResolved) &&
                      ((methodResolved =
                            preferVectorStdlibHelperPath(methodResolved)),
                       (hasDeclaredDefinitionPath(methodResolved) ||
                        hasImportedDefinitionPath(methodResolved)))) {
                    isBuiltinMethod = false;
                  } else {
                    if (resolveMethodTarget(
                            params, locals, expr.namespacePrefix, receiver,
                            "count", methodResolved, isBuiltinMethod)) {
                      // Method target resolved directly.
                    } else {
                      if (!(expr.hasBodyArguments ||
                            !expr.bodyArguments.empty()) ||
                          expr.args.empty()) {
                        (void)validateExpr(params, locals, receiver);
                        return false;
                      }
                      if (resolvesMapCountReceiver) {
                        methodResolved = stdlibMapCountTargetPath;
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
                          typeName = inferPointerLikeCallReturnType(
                              receiver, params, locals);
                        }
                        if (typeName.empty()) {
                          if (isPointerExpr(receiver, params, locals)) {
                            typeName = "Pointer";
                          } else if (isPointerLikeExpr(receiver, params,
                                                        locals)) {
                            typeName = "Reference";
                          }
                        }
                        if (typeName != "Pointer" &&
                            typeName != "Reference") {
                          (void)validateExpr(params, locals, receiver);
                          return false;
                        }
                        methodResolved = "/" + typeName + "/count";
                        error_.clear();
                        isBuiltinMethod = false;
                      }
                    }
                  }
                }
                if (!isBuiltinMethod &&
                    defMap_.find(methodResolved) == defMap_.end() &&
                    resolved.rfind(methodResolved + "__t", 0) == 0) {
                  methodResolved = resolved;
                }
                const bool targetsBareMapCountMethod =
                    methodResolved == bareMapCountTargetPath;
                const bool targetsStdlibMapCountMethod =
                    methodResolved == stdlibMapCountTargetPath;
                const bool rejectsDirectBareMapCountTarget =
                    !expr.isMethodCall && expr.args.size() == 1 &&
                    receiver.kind == Expr::Kind::Name &&
                    targetsBareMapCountMethod &&
                    lacksVisibleBareCountDefinition &&
                    lacksVisibleStdlibMapCountDefinition;
                const bool rejectsBuiltinStdlibMapCountTarget =
                    isBuiltinMethod && targetsStdlibMapCountMethod &&
                    lacksVisibleStdlibMapCountDefinition &&
                    !context.shouldBuiltinValidateBareMapCountCall;
                const std::string stdlibMapCountUnknownTargetDiagnostic =
                    "unknown call target: /std/collections/map/count";
                if (rejectsDirectBareMapCountTarget ||
                    rejectsBuiltinStdlibMapCountTarget) {
                  return failExprDiagnostic(
                      expr, stdlibMapCountUnknownTargetDiagnostic);
                }
                {
                  const std::string removedRootedVectorDirectCallDiagnostic =
                      getRemovedRootedVectorDirectCallDiagnostic(expr);
                  if (!removedRootedVectorDirectCallDiagnostic.empty()) {
                    (void)failExprDiagnostic(
                        expr, removedRootedVectorDirectCallDiagnostic);
                    return false;
                  }
                }
                if (!(isBuiltinMethod ||
                      hasDeclaredDefinitionPath(methodResolved) ||
                      hasImportedDefinitionPath(methodResolved))) {
                  (void)failExprDiagnostic(expr,
                                           "unknown method: " + methodResolved);
                  return false;
                }
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  }

  if (!(hasNamedArguments(expr.argNames) ||
        isUnimportedStdNamespacedVectorCompatibilityDirectCall(
            expr.isMethodCall,
            resolveCalleePath(expr),
            "capacity",
            hasImportedDefinitionPath("/std/collections/vector/capacity")) ||
        !isVectorBuiltinName(expr, "capacity")) &&
      ((!(expr.args.empty() || expr.args.size() == 1 ||
          defMap_.find(resolved) == defMap_.end()) &&
        context.isNamespacedVectorHelperCall) ||
       (expr.args.size() == 1 &&
        (defMap_.find(resolved) == defMap_.end() ||
         context.isNamespacedVectorCapacityCall)))) {
    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    const Expr &receiver = expr.args.front();
    bool isBuiltinMethod = false;
    std::string methodResolved;
    {
                if (resolveVectorHelperMethodTarget(
                        params, locals, receiver, "capacity",
                        methodResolved) &&
                    ((methodResolved =
                          preferVectorStdlibHelperPath(methodResolved)),
                     (hasDeclaredDefinitionPath(methodResolved) ||
                      hasImportedDefinitionPath(methodResolved)))) {
                  isBuiltinMethod = false;
                  if (isStdNamespacedVectorCompatibilityHelperPath(
                          resolveCalleePath(expr), "capacity")) {
                    methodResolved = "/std/collections/vector/capacity";
                    isBuiltinMethod = true;
                  }
                } else {
                  if (isStdNamespacedVectorCompatibilityHelperPath(
                          resolveCalleePath(expr), "capacity")) {
                    methodResolved = "/std/collections/vector/capacity";
                    isBuiltinMethod = true;
                  } else if (resolveMethodTarget(
                                 params, locals, expr.namespacePrefix, receiver,
                                 "capacity", methodResolved,
                                 isBuiltinMethod)) {
                    // Method target resolved directly.
                  } else {
                    (void)validateExpr(params, locals, receiver);
                    return false;
                  }
                }
                if (!isBuiltinMethod &&
                    defMap_.find(methodResolved) == defMap_.end() &&
                    resolved.rfind(methodResolved + "__t", 0) == 0) {
                  methodResolved = resolved;
                }
                if (!isBuiltinMethod &&
                    !hasDeclaredDefinitionPath(methodResolved) &&
                    !hasImportedDefinitionPath(methodResolved)) {
                  if ((context.isNonCollectionStructCapacityTarget == nullptr ||
                       !context.isNonCollectionStructCapacityTarget(
                           methodResolved)) &&
                      context.promoteCapacityToBuiltinValidation != nullptr) {
                    context.promoteCapacityToBuiltinValidation(
                        receiver, methodResolved, isBuiltinMethod, false);
                  }
                }
                if (!(isBuiltinMethod ||
                      hasDeclaredDefinitionPath(methodResolved) ||
                      hasImportedDefinitionPath(methodResolved))) {
                  (void)failExprDiagnostic(expr,
                                           "unknown method: " + methodResolved);
                  return false;
                }
                {
                  const std::string removedRootedVectorDirectCallDiagnostic =
                      getRemovedRootedVectorDirectCallDiagnostic(expr);
                  if (!removedRootedVectorDirectCallDiagnostic.empty()) {
                    (void)failExprDiagnostic(
                        expr, removedRootedVectorDirectCallDiagnostic);
                    return false;
                  }
                }
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  }

  return true;
}

} // namespace primec::semantics

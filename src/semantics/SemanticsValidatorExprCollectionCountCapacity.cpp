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
  const bool routesThroughStdNamespacedMapCountSurface =
      context.isStdNamespacedMapCountCall;
  const bool routesThroughNamespacedMapCountSurface =
      context.isNamespacedMapCountCall;
  const bool routesThroughUnnamespacedMapCountFallbackSurface =
      context.isUnnamespacedMapCountFallbackCall;
  const bool routesThroughResolvedMapCountSurface =
      context.isResolvedMapCountCall;
  const bool routesThroughMapCountCallSurface =
      routesThroughStdNamespacedMapCountSurface ||
      routesThroughNamespacedMapCountSurface ||
      routesThroughUnnamespacedMapCountFallbackSurface ||
      routesThroughResolvedMapCountSurface;
  const bool isSingleArgCountCall = expr.args.size() == 1;
  const bool isMultiArgCountCall = !isSingleArgCountCall;
  const bool routesThroughVectorBuiltinCountSurface =
      isVectorBuiltinName(expr, "count");
  const bool routesThroughNamespacedVectorCountHelperSurface =
      context.isNamespacedVectorHelperCall &&
      context.namespacedHelper == "count";
  const bool isArrayNamespacedVectorCountCompatibilityActive =
      context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
      context.isArrayNamespacedVectorCountCompatibilityCall(expr);
  const bool routesThroughNamespacedVectorCountFallback =
      !isStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall, resolveCalleePath(expr), "count") &&
      routesThroughNamespacedVectorCountHelperSurface &&
      routesThroughVectorBuiltinCountSurface && isSingleArgCountCall &&
      !hasDefinitionPath(resolved) &&
      !isArrayNamespacedVectorCountCompatibilityActive;
  const bool hasResolvedCountDefinitionTarget =
      defMap_.find(resolved) != defMap_.end();
  const bool allowsUnresolvedSingleArgCountRoute =
      !hasResolvedCountDefinitionTarget &&
      !routesThroughStdNamespacedMapCountSurface;
  const bool matchesSingleArgCountRouteShape =
      isSingleArgCountCall &&
      (allowsUnresolvedSingleArgCountRoute ||
       routesThroughNamespacedVectorCountFallback ||
       routesThroughMapCountCallSurface);
  const bool matchesMultiArgCountRouteShape =
      isMultiArgCountCall &&
      (hasResolvedCountDefinitionTarget ||
       routesThroughMapCountCallSurface);
  const bool matchesCountRouteArgShape =
      matchesSingleArgCountRouteShape ||
      matchesMultiArgCountRouteShape;
  const bool routesThroughCountMethodSurface =
      routesThroughVectorBuiltinCountSurface ||
      routesThroughMapCountCallSurface;
  const bool countMethodSurfaceHasNamedArguments =
      hasNamedArguments(expr.argNames);
  const bool
      routesThroughUnimportedStdNamespacedVectorCountCompatibilityDirectCall =
          isUnimportedStdNamespacedVectorCompatibilityDirectCall(
              expr.isMethodCall,
              resolveCalleePath(expr),
              "count",
              hasImportedDefinitionPath("/std/collections/vector/count"));
  const bool countMethodSurfaceHasNoArguments = expr.args.empty();
  const bool violatesCountMethodSurfacePreconditions =
      countMethodSurfaceHasNamedArguments ||
      routesThroughUnimportedStdNamespacedVectorCountCompatibilityDirectCall ||
      countMethodSurfaceHasNoArguments;
  const bool allowsCountMethodSurfacePreconditions =
      !violatesCountMethodSurfacePreconditions;
  const bool allowsCountMethodSurfaceCompatibility =
      !isArrayNamespacedVectorCountCompatibilityActive;
  const bool allowsCountMethodSurfaceRouteShape =
      routesThroughCountMethodSurface && matchesCountRouteArgShape;
  const bool matchesCountMethodSurfaceRoute =
      allowsCountMethodSurfacePreconditions &&
      allowsCountMethodSurfaceCompatibility &&
      allowsCountMethodSurfaceRouteShape;
  if (matchesCountMethodSurfaceRoute) {
    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    const Expr &receiver = expr.args.front();
    const bool isDirectNamedCountReceiverCall =
        !expr.isMethodCall && isSingleArgCountCall &&
        receiver.kind == Expr::Kind::Name;
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
    const bool allowsStdlibMapCountFallbackRoute =
        !hasDeclaredDefinitionPath(bareMapCountTargetPath) &&
        lacksVisibleStdlibMapCountDefinition &&
        resolvesMapCountReceiver;
    const bool routesThroughStdlibMapCountFallback =
        routesThroughUnnamespacedMapCountFallbackSurface &&
        allowsStdlibMapCountFallbackRoute;
    const bool countResolveMissLacksBodyArguments =
        !(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
        expr.args.empty();
    bool isBuiltinMethod = false;
    std::string methodResolved;
    {
                if (routesThroughStdlibMapCountFallback) {
                  methodResolved = stdlibMapCountTargetPath;
                  isBuiltinMethod = true;
                } else {
                  const bool resolvedCountHelperMethodTarget =
                      resolveVectorHelperMethodTarget(
                          params, locals, receiver, "count", methodResolved);
                  if (resolvedCountHelperMethodTarget) {
                    methodResolved = preferVectorStdlibHelperPath(methodResolved);
                  }
                  const bool hasVisibleCountHelperMethodTarget =
                      resolvedCountHelperMethodTarget &&
                      (hasDeclaredDefinitionPath(methodResolved) ||
                       hasImportedDefinitionPath(methodResolved));
                  if (hasVisibleCountHelperMethodTarget) {
                    isBuiltinMethod = false;
                  } else {
                    const bool resolvedCountMethodTargetDirectly =
                        resolveMethodTarget(
                            params, locals, expr.namespacePrefix, receiver,
                            "count", methodResolved, isBuiltinMethod);
                    if (resolvedCountMethodTargetDirectly) {
                      // Method target resolved directly.
                    } else {
                      if (countResolveMissLacksBodyArguments) {
                        (void)validateExpr(params, locals, receiver);
                        return false;
                      }
                      const bool usesCountResolveMissMapFallback =
                          resolvesMapCountReceiver;
                      std::string countResolveMissTargetPath;
                      if (usesCountResolveMissMapFallback) {
                        countResolveMissTargetPath = stdlibMapCountTargetPath;
                      } else {
                        std::string typeName;
                        const BindingInfo *countResolveMissReceiverBinding =
                            nullptr;
                        if (receiver.kind == Expr::Kind::Name) {
                          if (const BindingInfo *paramBinding =
                                  findParamBinding(params, receiver.name)) {
                            countResolveMissReceiverBinding = paramBinding;
                          } else if (auto it = locals.find(receiver.name);
                                     it != locals.end()) {
                            countResolveMissReceiverBinding = &it->second;
                          }
                        }
                        if (countResolveMissReceiverBinding != nullptr) {
                          typeName = countResolveMissReceiverBinding->typeName;
                        }
                        const bool needsCountReceiverTypeFromCallReturn =
                            typeName.empty();
                        if (needsCountReceiverTypeFromCallReturn) {
                          typeName = inferPointerLikeCallReturnType(
                              receiver, params, locals);
                        }
                        const bool needsCountReceiverTypeFromPointerFallback =
                            typeName.empty();
                        if (needsCountReceiverTypeFromPointerFallback) {
                          const bool
                              resolvesCountReceiverTypeFromPointerExpr =
                                  isPointerExpr(receiver, params, locals);
                          const bool
                              resolvesCountReceiverTypeFromPointerLikeExpr =
                                  !resolvesCountReceiverTypeFromPointerExpr &&
                                  isPointerLikeExpr(receiver, params, locals);
                          const char *countResolveMissPointerTypeName =
                              resolvesCountReceiverTypeFromPointerExpr
                                  ? "Pointer"
                                  : (resolvesCountReceiverTypeFromPointerLikeExpr
                                         ? "Reference"
                                         : nullptr);
                          if (countResolveMissPointerTypeName != nullptr) {
                            typeName = countResolveMissPointerTypeName;
                          }
                        }
                        const bool resolvesPointerLikeCountReceiverType =
                            typeName == "Pointer" || typeName == "Reference";
                        if (!resolvesPointerLikeCountReceiverType) {
                          (void)validateExpr(params, locals, receiver);
                          return false;
                        }
                        countResolveMissTargetPath =
                            "/" + typeName + "/count";
                      }
                      methodResolved = countResolveMissTargetPath;
                      error_.clear();
                      isBuiltinMethod = false;
                    }
                  }
                }
                if (!isBuiltinMethod &&
                    defMap_.find(methodResolved) == defMap_.end() &&
                    resolved.rfind(methodResolved + "__t", 0) == 0) {
                  methodResolved = resolved;
                }
                if ((isDirectNamedCountReceiverCall &&
                     methodResolved == bareMapCountTargetPath &&
                     lacksVisibleBareCountDefinition &&
                     lacksVisibleStdlibMapCountDefinition) ||
                    (isBuiltinMethod &&
                     methodResolved == stdlibMapCountTargetPath &&
                     lacksVisibleStdlibMapCountDefinition &&
                     !context.shouldBuiltinValidateBareMapCountCall)) {
                  return failExprDiagnostic(
                      expr, "unknown call target: " + stdlibMapCountTargetPath);
                }
                if (const std::string removedRootedVectorDirectCallDiagnostic =
                        getRemovedRootedVectorDirectCallDiagnostic(expr);
                    !removedRootedVectorDirectCallDiagnostic.empty()) {
                  (void)failExprDiagnostic(
                      expr, removedRootedVectorDirectCallDiagnostic);
                  return false;
                }
                if (!isBuiltinMethod &&
                    !hasDeclaredDefinitionPath(methodResolved) &&
                    !hasImportedDefinitionPath(methodResolved)) {
                  (void)failExprDiagnostic(expr,
                                           "unknown method: " + methodResolved);
                  return false;
                }
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  }

  const bool capacityMethodSurfaceHasNamedArguments =
      hasNamedArguments(expr.argNames);
  const bool
      routesThroughUnimportedStdNamespacedVectorCapacityCompatibilityDirectCall =
          isUnimportedStdNamespacedVectorCompatibilityDirectCall(
              expr.isMethodCall,
              resolveCalleePath(expr),
              "capacity",
              hasImportedDefinitionPath("/std/collections/vector/capacity"));
  const bool capacityMethodSurfaceUsesNonVectorBuiltinName =
      !isVectorBuiltinName(expr, "capacity");
  const bool violatesCapacityMethodSurfacePreconditions =
      capacityMethodSurfaceHasNamedArguments ||
      routesThroughUnimportedStdNamespacedVectorCapacityCompatibilityDirectCall ||
      capacityMethodSurfaceUsesNonVectorBuiltinName;
  const bool allowsCapacityMethodSurfacePreconditions =
      !violatesCapacityMethodSurfacePreconditions;
  const bool routesThroughNamespacedVectorCapacityHelperSurface =
      context.isNamespacedVectorHelperCall;
  const bool routesThroughNamespacedVectorCapacityCallSurface =
      context.isNamespacedVectorCapacityCall;
  const bool capacityMethodSurfaceHasNoArguments =
      expr.args.empty();
  const bool capacityMethodSurfaceUsesSingleArgument =
      expr.args.size() == 1;
  const bool capacityMethodSurfaceLacksResolvedDefinitionTarget =
      defMap_.find(resolved) == defMap_.end();
  const bool matchesCapacityMethodSurfaceRouteShape =
      ((!(capacityMethodSurfaceHasNoArguments ||
          capacityMethodSurfaceUsesSingleArgument ||
          capacityMethodSurfaceLacksResolvedDefinitionTarget) &&
        routesThroughNamespacedVectorCapacityHelperSurface) ||
       (capacityMethodSurfaceUsesSingleArgument &&
        (capacityMethodSurfaceLacksResolvedDefinitionTarget ||
         routesThroughNamespacedVectorCapacityCallSurface)));
  if (allowsCapacityMethodSurfacePreconditions &&
      matchesCapacityMethodSurfaceRouteShape) {
    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    const Expr &receiver = expr.args.front();
    bool isBuiltinMethod = false;
    std::string methodResolved;
    {
                const std::string stdlibVectorCapacityTargetPath =
                    "/std/collections/vector/capacity";
                const auto assignStdlibVectorCapacityCompatibilityTarget =
                    [&]() {
                      methodResolved = stdlibVectorCapacityTargetPath;
                      isBuiltinMethod = true;
                    };
                const bool usesStdNamespacedCapacityCompatibilityHelper =
                    isStdNamespacedVectorCompatibilityHelperPath(
                        resolveCalleePath(expr), "capacity");
                const bool resolvedVisibleCapacityHelperMethodTarget =
                    resolveVectorHelperMethodTarget(
                        params, locals, receiver, "capacity",
                        methodResolved) &&
                    ((methodResolved =
                          preferVectorStdlibHelperPath(methodResolved)),
                     (hasDeclaredDefinitionPath(methodResolved) ||
                      hasImportedDefinitionPath(methodResolved)));
                if (resolvedVisibleCapacityHelperMethodTarget) {
                  isBuiltinMethod = false;
                }
                if (usesStdNamespacedCapacityCompatibilityHelper) {
                  assignStdlibVectorCapacityCompatibilityTarget();
                } else if (!resolvedVisibleCapacityHelperMethodTarget &&
                           !resolveMethodTarget(
                               params, locals, expr.namespacePrefix, receiver,
                               "capacity", methodResolved, isBuiltinMethod)) {
                  (void)validateExpr(params, locals, receiver);
                  return false;
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
                  (void)failExprDiagnostic(expr,
                                           "unknown method: " + methodResolved);
                  return false;
                }
                if (const std::string removedRootedVectorDirectCallDiagnostic =
                        getRemovedRootedVectorDirectCallDiagnostic(expr);
                    !removedRootedVectorDirectCallDiagnostic.empty()) {
                  (void)failExprDiagnostic(
                      expr, removedRootedVectorDirectCallDiagnostic);
                  return false;
                }
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  }

  return true;
}

} // namespace primec::semantics

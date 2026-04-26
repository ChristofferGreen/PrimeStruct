#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

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
  const auto lacksVisibleMethodTargetPath =
      [&](const std::string &methodTargetPath) {
        return !hasDeclaredDefinitionPath(methodTargetPath) &&
               !hasImportedDefinitionPath(methodTargetPath);
      };
  const auto lacksVisibleResolvedMethodTarget =
      [&](const std::string &methodTargetPath, bool isBuiltinMethod) {
        return !isBuiltinMethod &&
               lacksVisibleMethodTargetPath(methodTargetPath);
      };
  const auto failUnknownMethodTarget =
      [&](const std::string &methodTargetPath) {
        return failExprDiagnostic(expr, "unknown method: " + methodTargetPath);
      };
  const auto failInvisibleResolvedMethodTarget =
      [&](const std::string &methodTargetPath, bool isBuiltinMethod) {
        if (lacksVisibleResolvedMethodTarget(methodTargetPath,
                                             isBuiltinMethod)) {
          return failUnknownMethodTarget(methodTargetPath);
        }
        return true;
      };
  const auto failUnknownCallTarget =
      [&](const std::string &callTargetPath) {
        return failExprDiagnostic(expr, "unknown call target: " + callTargetPath);
      };
  const auto failRemovedRootedVectorDirectCall =
      [&]() {
        if (const std::string removedRootedVectorDirectCallDiagnostic =
                getRemovedRootedVectorDirectCallDiagnostic(expr);
            !removedRootedVectorDirectCallDiagnostic.empty()) {
          return failExprDiagnostic(expr,
                                    removedRootedVectorDirectCallDiagnostic);
        }
        return true;
      };
  const auto promoteUnknownCapacityMethodToBuiltinValidation =
      [&](const Expr &receiverExpr,
          std::string &methodTargetPath,
          bool isBuiltinMethod) {
        const bool lacksVisibleCapacityMethodTargetBeforePromotion =
            lacksVisibleResolvedMethodTarget(methodTargetPath, isBuiltinMethod);
        const bool allowsCapacityBuiltinValidationPromotion =
            (context.isNonCollectionStructCapacityTarget == nullptr ||
             !context.isNonCollectionStructCapacityTarget(methodTargetPath)) &&
            context.promoteCapacityToBuiltinValidation != nullptr;
        if (lacksVisibleCapacityMethodTargetBeforePromotion &&
            allowsCapacityBuiltinValidationPromotion) {
          context.promoteCapacityToBuiltinValidation(
              receiverExpr, methodTargetPath, isBuiltinMethod, false);
        }
      };
  const auto reusesResolvedMethodMonomorphizedTarget =
      [&](const std::string &methodTargetPath, bool isBuiltinMethod) {
        return !isBuiltinMethod &&
               defMap_.find(methodTargetPath) == defMap_.end() &&
               resolved.rfind(methodTargetPath + "__t", 0) == 0;
      };
  const auto markMethodTargetUsage =
      [&]() {
        usedMethodTarget = true;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
      };
  const auto resolveVisiblePreferredVectorHelperMethodTarget =
      [&](const Expr &receiverExpr,
          const char *helperName,
          std::string &methodTargetPath) {
        const bool resolvedVectorHelperMethodTarget =
            resolveVectorHelperMethodTarget(
                params, locals, receiverExpr, helperName, methodTargetPath);
        if (resolvedVectorHelperMethodTarget) {
          methodTargetPath = preferVectorStdlibHelperPath(methodTargetPath);
        }
        return resolvedVectorHelperMethodTarget &&
               !lacksVisibleMethodTargetPath(methodTargetPath);
      };
  const auto isCountOrCapacityHelperName =
      [](const std::string &helperName) {
        return helperName == "count" || helperName == "count_ref" ||
               helperName == "capacity";
      };
  const auto isCountLikeHelperName =
      [](const std::string &helperName) {
        return helperName == "count" || helperName == "count_ref";
      };
  const auto isResolvedCountOrCapacityHelperInstantiation =
      [&]() {
        if (expr.isMethodCall || defMap_.find(resolved) == defMap_.end()) {
          return false;
        }
        const size_t lastSlash = resolved.find_last_of('/');
        const size_t generatedSuffix =
            resolved.find("__",
                          lastSlash == std::string::npos ? 0 : lastSlash + 1);
        if (generatedSuffix == std::string::npos) {
          return false;
        }
        const std::string helperName =
            resolved.substr(lastSlash == std::string::npos ? 0 : lastSlash + 1,
                            generatedSuffix - lastSlash - 1);
        return isCountOrCapacityHelperName(helperName);
      };
  const auto canonicalizeDirectExperimentalSoaWrapperCountHelperCall =
      [&]() -> std::string {
        if (expr.isMethodCall || expr.args.size() != 1) {
          return {};
        }
        const size_t lastSlash = resolved.find_last_of('/');
        const size_t specializationSuffix =
            resolved.find("__t", lastSlash == std::string::npos ? 0
                                                                : lastSlash + 1);
        std::string canonicalResolved = resolved;
        if (specializationSuffix != std::string::npos) {
          canonicalResolved.erase(specializationSuffix);
        }
        if (canonicalResolved.rfind(
                "/std/collections/experimental_soa_vector/SoaVector__", 0) != 0) {
          return {};
        }
        if (canonicalResolved.ends_with("/count")) {
          return "/std/collections/soa_vector/count";
        }
        if (canonicalResolved.ends_with("/count_ref")) {
          return "/std/collections/soa_vector/count_ref";
        }
        return {};
      };
  const auto canonicalizeDirectSoaCountHelperResolved =
      [&]() -> std::string {
        if (expr.isMethodCall || expr.args.size() != 1) {
          return {};
        }
        const size_t lastSlash = resolved.find_last_of('/');
        const size_t specializationSuffix =
            resolved.find("__t", lastSlash == std::string::npos ? 0
                                                                : lastSlash + 1);
        std::string canonicalResolved = resolved;
        if (specializationSuffix != std::string::npos) {
          canonicalResolved.erase(specializationSuffix);
        }
        if (canonicalResolved == "/std/collections/soa_vector/count" ||
            canonicalResolved == "/std/collections/soa_vector/count_ref") {
          return canonicalResolved;
        }
        return {};
      };
  if (isResolvedCountOrCapacityHelperInstantiation()) {
    handledOut = true;
    return true;
  }
  if (const std::string canonicalDirectSoaCountPath =
          canonicalizeDirectSoaCountHelperResolved();
      canonicalDirectSoaCountPath == "/std/collections/soa_vector/count_ref") {
    handledOut = true;
    resolved = canonicalDirectSoaCountPath;
    resolvedMethod = false;
    return true;
  }
  if (const std::string canonicalDirectExperimentalSoaCountPath =
          canonicalizeDirectExperimentalSoaWrapperCountHelperCall();
      canonicalDirectExperimentalSoaCountPath ==
      "/std/collections/soa_vector/count_ref") {
    handledOut = true;
    resolved = canonicalDirectExperimentalSoaCountPath;
    resolvedMethod = false;
    return true;
  }
  const bool routesThroughNamespacedCountOrCapacityHelperSurface =
      context.isNamespacedVectorHelperCall &&
      isCountOrCapacityHelperName(context.namespacedHelper);
  const bool matchesNamedArgumentCountOrCapacityHelperFastPath =
      hasNamedArguments(expr.argNames) &&
      expr.args.size() == 1 &&
      defMap_.find(resolved) == defMap_.end() &&
      routesThroughNamespacedCountOrCapacityHelperSurface;
  const auto resolvesNamedArgumentCountOrCapacityHelperTarget =
      [&](std::string &resolvedMethodTarget, bool &resolvedBuiltinMethod) {
        return resolveMethodTarget(params,
                                   locals,
                                   expr.namespacePrefix,
                                   expr.args.front(),
                                   context.namespacedHelper,
                                   resolvedMethodTarget,
                                   resolvedBuiltinMethod) &&
               !resolvedBuiltinMethod &&
               defMap_.find(resolvedMethodTarget) != defMap_.end();
      };
  if (isResolvedCountOrCapacityHelperInstantiation()) {
    handledOut = true;
    return true;
  }

  const auto applyNamedArgumentCountOrCapacityHelperFastPath =
      [&]() {
        if (!matchesNamedArgumentCountOrCapacityHelperFastPath) {
          return false;
        }
        handledOut = true;
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (resolvesNamedArgumentCountOrCapacityHelperTarget(methodResolved,
                                                             isBuiltinMethod)) {
          markMethodTargetUsage();
          resolved = methodResolved;
          resolvedMethod = false;
        }
        return true;
      };
  if (applyNamedArgumentCountOrCapacityHelperFastPath()) {
    return true;
  }
  const auto tryResolveBareSimpleCountForwardingMethod =
      [&]() {
        if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
            expr.args.size() != 1 || !expr.namespacePrefix.empty() ||
            !isSimpleCallName(expr, "count") ||
            hasNamedArguments(expr.argNames) ||
            hasDeclaredDefinitionPath("/count") ||
            hasImportedDefinitionPath("/count")) {
          return false;
        }
        const Expr &receiver = expr.args.front();
        std::string visibleCountHelperTarget;
        const bool resolvesVisibleCollectionCountHelper =
            resolveVisiblePreferredVectorHelperMethodTarget(
                receiver, "count", visibleCountHelperTarget) &&
            (visibleCountHelperTarget == "/vector/count" ||
             visibleCountHelperTarget == "/std/collections/vector/count" ||
             visibleCountHelperTarget == "/soa_vector/count" ||
             visibleCountHelperTarget == "/std/collections/soa_vector/count" ||
             visibleCountHelperTarget == "/soa_vector/count_ref" ||
             visibleCountHelperTarget ==
                 "/std/collections/soa_vector/count_ref");
        const bool receiverLooksLikeBuiltinCountTarget =
            resolvesVisibleCollectionCountHelper ||
            (context.resolveMapTarget != nullptr &&
             context.resolveMapTarget(receiver)) ||
            inferExprReturnKind(receiver, params, locals) == ReturnKind::Array ||
            inferExprReturnKind(receiver, params, locals) == ReturnKind::String;
        if (receiverLooksLikeBuiltinCountTarget) {
          return false;
        }
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiver,
                                 "count", methodResolved, isBuiltinMethod) ||
            isBuiltinMethod ||
            lacksVisibleMethodTargetPath(methodResolved)) {
          return false;
        }
        handledOut = true;
        markMethodTargetUsage();
        resolved = methodResolved;
        resolvedMethod = false;
        return true;
      };
  if (tryResolveBareSimpleCountForwardingMethod()) {
    return true;
  }

  Expr rewrittenVectorHelperCall;
  const auto tryRewriteBareNamedVectorHelperCall =
      [&](const char *helperName) {
        return context.tryRewriteBareVectorHelperCall != nullptr &&
               context.tryRewriteBareVectorHelperCall(
                   helperName, rewrittenVectorHelperCall);
      };
  const auto tryRewriteBareVectorCountOrCapacityHelperCall =
      [&]() {
        return tryRewriteBareNamedVectorHelperCall("count") ||
               tryRewriteBareNamedVectorHelperCall("count_ref") ||
               tryRewriteBareNamedVectorHelperCall("capacity");
      };
  const auto applyBareVectorCountOrCapacityHelperRewrite =
      [&]() {
        if (!tryRewriteBareVectorCountOrCapacityHelperCall()) {
          return false;
        }
        handledOut = true;
        rewrittenExprOut = std::move(rewrittenVectorHelperCall);
        return true;
      };
  if (applyBareVectorCountOrCapacityHelperRewrite()) {
    return true;
  }
  const auto failStdNamespacedVectorCountDiagnosticIfPresent =
      [&]() {
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
        return true;
      };
  if (!failStdNamespacedVectorCountDiagnosticIfPresent()) {
    return false;
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
  const std::string countHelperName =
      [&]() -> std::string {
        const std::string resolvedPath = resolveCalleePath(expr);
        if (routesThroughStdNamespacedMapCountSurface ||
            routesThroughNamespacedMapCountSurface ||
            routesThroughResolvedMapCountSurface) {
          if (resolvedPath == "/std/collections/map/count_ref" ||
              resolved == "/std/collections/map/count_ref" ||
              resolved == "/map/count_ref") {
            return "count_ref";
          }
        }
        if (context.isNamespacedVectorHelperCall &&
            isCountLikeHelperName(context.namespacedHelper)) {
          return context.namespacedHelper;
        }
        if (isLegacyOrCanonicalSoaHelperPath(resolvedPath, "count_ref")) {
          return "count_ref";
        }
        return "count";
      }();
  const bool isSingleArgCountCall = expr.args.size() == 1;
  const bool isMultiArgCountCall = !isSingleArgCountCall;
  const bool routesThroughVectorBuiltinCountSurface =
      isVectorBuiltinName(expr, "count") || countHelperName == "count_ref";
  const bool routesThroughNamespacedVectorCountHelperSurface =
      context.isNamespacedVectorHelperCall &&
      isCountLikeHelperName(context.namespacedHelper);
  const bool isArrayNamespacedVectorCountCompatibilityActive =
      context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
      context.isArrayNamespacedVectorCountCompatibilityCall(expr);
  const bool matchesNamespacedVectorCountFallbackRouteShape =
      routesThroughNamespacedVectorCountHelperSurface &&
      routesThroughVectorBuiltinCountSurface && isSingleArgCountCall &&
      !hasDefinitionPath(resolved) &&
      !isArrayNamespacedVectorCountCompatibilityActive;
  const bool routesThroughNamespacedVectorCountFallback =
      !isStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall, resolveCalleePath(expr), countHelperName) &&
      matchesNamespacedVectorCountFallbackRouteShape;
  const bool hasResolvedCountDefinitionTarget =
      defMap_.find(resolved) != defMap_.end();
  const bool allowsUnresolvedSingleArgCountRoute =
      !hasResolvedCountDefinitionTarget &&
      !routesThroughStdNamespacedMapCountSurface;
  const bool matchesSingleArgCountRouteInputs =
      allowsUnresolvedSingleArgCountRoute ||
      routesThroughNamespacedVectorCountFallback ||
      routesThroughMapCountCallSurface;
  const bool matchesSingleArgCountRouteShape =
      isSingleArgCountCall &&
      matchesSingleArgCountRouteInputs;
  const bool matchesMultiArgCountRouteInputs =
      hasResolvedCountDefinitionTarget ||
      routesThroughMapCountCallSurface;
  const bool matchesMultiArgCountRouteShape =
      isMultiArgCountCall &&
      matchesMultiArgCountRouteInputs;
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
              countHelperName,
              hasImportedDefinitionPath(
                  countHelperName == "count_ref"
                      ? "/std/collections/soa_vector/count_ref"
                      : "/std/collections/vector/count"));
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
    markMethodTargetUsage();
    const Expr &receiver = expr.args.front();
    const bool isDirectNamedCountReceiverCall =
        !expr.isMethodCall && isSingleArgCountCall &&
        receiver.kind == Expr::Kind::Name;
    const bool resolvesMapCountReceiver =
        context.resolveMapTarget != nullptr &&
        context.resolveMapTarget(receiver);
    const bool requestsStdNamespacedVectorCountHelper =
        expr.namespacePrefix == "std/collections/vector" ||
        expr.namespacePrefix == "/std/collections/vector" ||
        expr.name.rfind("/std/collections/vector/", 0) == 0;
    const std::string explicitVectorCountTargetPath =
        requestsStdNamespacedVectorCountHelper
            ? "/std/collections/vector/" + countHelperName
            : "/vector/" + countHelperName;
    const std::string bareCountTargetPath = "/" + countHelperName;
    const std::string bareMapCountTargetPath =
        "/map/" + countHelperName;
    const std::string stdlibMapCountTargetPath =
        "/std/collections/map/" + countHelperName;
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
                  const bool hasVisibleCountHelperMethodTarget =
                      resolveVisiblePreferredVectorHelperMethodTarget(
                          receiver, countHelperName.c_str(), methodResolved);
                  const bool needsDirectCountMethodTargetResolution =
                      !hasVisibleCountHelperMethodTarget;
                  const bool resolvedCountMethodTargetDirectly =
                      needsDirectCountMethodTargetResolution &&
                      resolveMethodTarget(
                          params, locals, expr.namespacePrefix, receiver,
                          countHelperName.c_str(), methodResolved,
                          isBuiltinMethod);
                  const bool
                      redirectsCanonicalStdNamespacedVectorCountMethodToExplicitTarget =
                          resolvedCountMethodTargetDirectly &&
                          expr.isMethodCall &&
                          requestsStdNamespacedVectorCountHelper &&
                          resolvesMapCountReceiver &&
                          (methodResolved == bareMapCountTargetPath ||
                           methodResolved == stdlibMapCountTargetPath);
                  if (redirectsCanonicalStdNamespacedVectorCountMethodToExplicitTarget) {
                    methodResolved = explicitVectorCountTargetPath;
                    error_.clear();
                    isBuiltinMethod = false;
                  }
                  const bool failsCountMethodTargetResolution =
                      needsDirectCountMethodTargetResolution &&
                      !resolvedCountMethodTargetDirectly;
                  if (failsCountMethodTargetResolution) {
                    if (countResolveMissLacksBodyArguments) {
                      (void)validateExpr(params, locals, receiver);
                      return false;
                    }
                    const bool usesCountResolveMissMapFallback =
                        resolvesMapCountReceiver &&
                        !routesThroughNamespacedVectorCountHelperSurface;
                    std::string countResolveMissTargetPath;
                    if (usesCountResolveMissMapFallback) {
                      countResolveMissTargetPath = stdlibMapCountTargetPath;
                    } else if (routesThroughNamespacedVectorCountHelperSurface) {
                      countResolveMissTargetPath = explicitVectorCountTargetPath;
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
                        const bool resolvesCountReceiverTypeFromPointerExpr =
                            isPointerExpr(receiver, params, locals);
                        const bool resolvesCountReceiverTypeFromPointerLikeExpr =
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
                          "/" + typeName + "/" + countHelperName;
                    }
                    methodResolved = countResolveMissTargetPath;
                    error_.clear();
                    isBuiltinMethod = false;
                  }
                }
                if (reusesResolvedMethodMonomorphizedTarget(methodResolved,
                                                            isBuiltinMethod)) {
                  methodResolved = resolved;
                }
                const bool resolvesCanonicalDirectSoaCountHelper =
                    !expr.isMethodCall &&
                    expr.args.size() == 1 &&
                    (methodResolved == "/std/collections/soa_vector/count" ||
                     methodResolved == "/std/collections/soa_vector/count_ref");
                if (resolvesCanonicalDirectSoaCountHelper) {
                  resolved = methodResolved;
                  resolvedMethod = false;
                  return true;
                }
                if (!failRemovedRootedVectorDirectCall()) {
                  return false;
                }
                const bool failsCountUnknownTargetValidation =
                    (isDirectNamedCountReceiverCall &&
                     methodResolved == bareMapCountTargetPath &&
                     lacksVisibleBareCountDefinition &&
                     lacksVisibleStdlibMapCountDefinition) ||
                    (isDirectNamedCountReceiverCall &&
                     methodResolved == "/vector/count" &&
                     !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
                     !hasImportedDefinitionPath("/std/collections/vector/count")) ||
                    (isBuiltinMethod &&
                     methodResolved == stdlibMapCountTargetPath &&
                     lacksVisibleStdlibMapCountDefinition &&
                     !context.shouldBuiltinValidateBareMapCountCall);
                if (failsCountUnknownTargetValidation) {
                  return failUnknownCallTarget(methodResolved == "/vector/count"
                                                   ? "/std/collections/vector/count"
                                                   : stdlibMapCountTargetPath);
                }
                if (!failInvisibleResolvedMethodTarget(methodResolved,
                                                      isBuiltinMethod)) {
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
  const bool matchesHelperCapacityMethodSurfaceArgumentShape =
      !(capacityMethodSurfaceHasNoArguments ||
        capacityMethodSurfaceUsesSingleArgument);
  const bool matchesHelperCapacityMethodSurfaceRouteInputs =
      matchesHelperCapacityMethodSurfaceArgumentShape &&
      !capacityMethodSurfaceLacksResolvedDefinitionTarget;
  const bool matchesHelperCapacityMethodSurfaceRouteShape =
      matchesHelperCapacityMethodSurfaceRouteInputs &&
      routesThroughNamespacedVectorCapacityHelperSurface;
  const bool matchesSingleArgCapacityMethodSurfaceRouteInputs =
      capacityMethodSurfaceLacksResolvedDefinitionTarget ||
      routesThroughNamespacedVectorCapacityCallSurface;
  const bool matchesSingleArgCapacityMethodSurfaceRouteShape =
      capacityMethodSurfaceUsesSingleArgument &&
      matchesSingleArgCapacityMethodSurfaceRouteInputs;
  const bool routesThroughCapacityMethodSurface =
      matchesHelperCapacityMethodSurfaceRouteShape ||
      matchesSingleArgCapacityMethodSurfaceRouteShape;
  const bool matchesCapacityMethodSurfaceRoute =
      allowsCapacityMethodSurfacePreconditions &&
      routesThroughCapacityMethodSurface;
  if (matchesCapacityMethodSurfaceRoute) {
    handledOut = true;
    markMethodTargetUsage();
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
                const bool hasVisibleCapacityHelperMethodTarget =
                    resolveVisiblePreferredVectorHelperMethodTarget(
                        receiver, "capacity", methodResolved);
                const bool needsDirectCapacityMethodTargetResolution =
                    !usesStdNamespacedCapacityCompatibilityHelper &&
                    !hasVisibleCapacityHelperMethodTarget;
                const bool resolvedCapacityMethodTargetDirectly =
                    needsDirectCapacityMethodTargetResolution &&
                    resolveMethodTarget(
                        params, locals, expr.namespacePrefix, receiver,
                        "capacity", methodResolved, isBuiltinMethod);
                const bool failsCapacityMethodTargetResolution =
                    needsDirectCapacityMethodTargetResolution &&
                    !resolvedCapacityMethodTargetDirectly;
                if (usesStdNamespacedCapacityCompatibilityHelper) {
                  assignStdlibVectorCapacityCompatibilityTarget();
                } else if (failsCapacityMethodTargetResolution) {
                  (void)validateExpr(params, locals, receiver);
                  return false;
                }
                if (reusesResolvedMethodMonomorphizedTarget(methodResolved,
                                                            isBuiltinMethod)) {
                  methodResolved = resolved;
                }
                if (!failRemovedRootedVectorDirectCall()) {
                  return false;
                }
                if (!expr.isMethodCall && expr.args.size() == 1 &&
                    receiver.kind == Expr::Kind::Name &&
                    methodResolved == "/vector/capacity" &&
                    !hasDeclaredDefinitionPath("/std/collections/vector/capacity") &&
                    !hasImportedDefinitionPath("/std/collections/vector/capacity")) {
                  return failUnknownCallTarget(
                      "/std/collections/vector/capacity");
                }
                promoteUnknownCapacityMethodToBuiltinValidation(
                    receiver, methodResolved, isBuiltinMethod);
                if (!failInvisibleResolvedMethodTarget(methodResolved,
                                                      isBuiltinMethod)) {
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

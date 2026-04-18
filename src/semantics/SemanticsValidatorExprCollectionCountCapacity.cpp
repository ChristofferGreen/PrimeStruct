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
  auto failCollectionCountCapacityDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto hasResolvableDefinitionPath = [&](const std::string &path) {
    return hasDeclaredDefinitionPath(path) || hasImportedDefinitionPath(path);
  };
  const std::string removedRootedVectorDirectCallDiagnostic =
      getRemovedRootedVectorDirectCallDiagnostic(expr);
  auto isConcreteCountCapacityInstantiation = [&](const std::string &path) {
    if (defMap_.find(path) == defMap_.end()) {
      return false;
    }
    const size_t lastSlash = path.find_last_of('/');
    const size_t instantiationPos = path.find("__t", lastSlash == std::string::npos ? 0 : lastSlash + 1);
    if (instantiationPos == std::string::npos) {
      return false;
    }
    const std::string helperName =
        path.substr(lastSlash == std::string::npos ? 0 : lastSlash + 1, instantiationPos - lastSlash - 1);
    return helperName == "count" || helperName == "capacity";
  };
  if (!expr.isMethodCall && isConcreteCountCapacityInstantiation(resolved)) {
    return true;
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
  const bool callsStdNamespacedVectorCountHelper =
      isStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall, resolveCalleePath(expr), "count");
  const bool callsUnimportedStdNamespacedVectorCountHelper =
      isUnimportedStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "count",
          hasImportedDefinitionPath("/std/collections/vector/count"));
  const bool callsUndeclaredStdNamespacedVectorCountHelper =
      isUndeclaredStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "count",
          hasDeclaredDefinitionPath("/std/collections/vector/count"));
  const bool callsUnresolvableStdNamespacedVectorCountHelper =
      isUnresolvableStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "count",
          hasResolvableDefinitionPath("/std/collections/vector/count"));
  const bool resolvesStdNamespacedVectorCountMapTarget =
      expr.args.size() == 1 &&
      context.resolveMapTarget != nullptr &&
      context.resolveMapTarget(expr.args.front());
  const std::string stdNamespacedVectorCountTargetDiagnosticMessage =
      classifyStdNamespacedVectorCountDiagnosticMessage(
          false,
          context.isDirectStdNamespacedVectorCountWrapperMapTarget,
          callsUndeclaredStdNamespacedVectorCountHelper,
          resolvesStdNamespacedVectorCountMapTarget,
          callsUnresolvableStdNamespacedVectorCountHelper);
  if (!stdNamespacedVectorCountTargetDiagnosticMessage.empty()) {
    handledOut = true;
    return failCollectionCountCapacityDiagnostic(
        std::move(stdNamespacedVectorCountTargetDiagnosticMessage));
  }
  auto resolveCountMethod = [&](bool requireSingleArg) -> bool {
    if (hasNamedArguments(expr.argNames) ||
        callsUnimportedStdNamespacedVectorCountHelper || expr.args.empty()) {
      return false;
    }
    if (requireSingleArg && expr.args.size() != 1) {
      return false;
    }
    if (!requireSingleArg && expr.args.size() == 1) {
      return false;
    }
    if ((context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
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
    const auto matchesCountMethodCallShape = [&](bool requireSingleArg) {
      const bool resolvesExplicitCountMethodTarget =
          defMap_.find(resolved) != defMap_.end();
      const bool resolvesMapCountSurface =
          context.isStdNamespacedMapCountCall || context.isNamespacedMapCountCall ||
          context.isUnnamespacedMapCountFallbackCall || context.isResolvedMapCountCall;
      if (requireSingleArg) {
        return (!resolvesExplicitCountMethodTarget &&
                !context.isStdNamespacedMapCountCall) ||
               context.isNamespacedVectorCountCall ||
               resolvesMapCountSurface;
      }
      return resolvesExplicitCountMethodTarget || resolvesMapCountSurface;
    };
    if (!matchesCountMethodCallShape(requireSingleArg)) {
      return false;
    }

    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    bool isBuiltinMethod = false;
    std::string methodResolved;
    auto hasVisibleStdlibMapCountDefinition = [&]() {
      return hasDeclaredDefinitionPath("/std/collections/map/count") ||
             hasImportedDefinitionPath("/std/collections/map/count");
    };
    auto resolveBodyArgumentCountTarget = [&]() -> bool {
      if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) || expr.args.empty()) {
        return false;
      }
      const Expr &receiver = expr.args.front();
      if (context.resolveMapTarget != nullptr && context.resolveMapTarget(receiver)) {
        if (defMap_.count("/std/collections/map/count") > 0) {
          methodResolved = "/std/collections/map/count";
        } else {
          methodResolved = "/std/collections/map/count";
        }
        return true;
      }
      std::string typeName;
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
        } else if (auto it = locals.find(receiver.name); it != locals.end()) {
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
        return false;
      }
      methodResolved = "/" + typeName + "/count";
      return true;
    };
    const auto resolveCountMethodTarget = [&]() -> bool {
      if (resolveVectorHelperMethodTarget(params, locals, expr.args.front(), "count",
                                          methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (hasResolvableDefinitionPath(methodResolved)) {
          isBuiltinMethod = false;
          return true;
        }
      }
      if (resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "count",
                              methodResolved, isBuiltinMethod)) {
        return true;
      }
      if (!resolveBodyArgumentCountTarget()) {
        return false;
      }
      error_.clear();
      isBuiltinMethod = false;
      return true;
    };
    if (context.isUnnamespacedMapCountFallbackCall &&
        !hasDeclaredDefinitionPath("/std/collections/map/count") &&
        !hasDeclaredDefinitionPath("/map/count") &&
        !hasVisibleStdlibMapCountDefinition() &&
        context.resolveMapTarget != nullptr &&
        context.resolveMapTarget(expr.args.front())) {
      methodResolved = "/std/collections/map/count";
      isBuiltinMethod = true;
    } else if (!resolveCountMethodTarget()) {
      (void)validateExpr(params, locals, expr.args.front());
      return false;
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
        resolved.rfind(methodResolved + "__t", 0) == 0) {
      methodResolved = resolved;
    }
    if (!removedRootedVectorDirectCallDiagnostic.empty()) {
      return failCollectionCountCapacityDiagnostic(
          removedRootedVectorDirectCallDiagnostic);
    }
    if (!expr.isMethodCall &&
        expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Name &&
        methodResolved == "/map/count" &&
        !hasImportedDefinitionPath("/count") &&
        !hasDeclaredDefinitionPath("/count") &&
        !hasVisibleStdlibMapCountDefinition()) {
      return failCollectionCountCapacityDiagnostic("unknown call target: /std/collections/map/count");
    }
    if (isBuiltinMethod && methodResolved == "/std/collections/map/count" &&
        !hasVisibleStdlibMapCountDefinition() &&
        !context.shouldBuiltinValidateBareMapCountCall) {
      return failCollectionCountCapacityDiagnostic("unknown call target: /std/collections/map/count");
    }
    if (!isBuiltinMethod && !hasResolvableDefinitionPath(methodResolved)) {
      return failCollectionCountCapacityDiagnostic("unknown method: " + methodResolved);
    }
    resolved = methodResolved;
    resolvedMethod = isBuiltinMethod;
    return true;
  };
  const auto tryResolveCountMethod = [&]() -> std::optional<bool> {
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
    return std::nullopt;
  };

  if (std::optional<bool> resolvedCountMethod = tryResolveCountMethod()) {
    return *resolvedCountMethod;
  }

  auto resolveCapacityMethod = [&](bool requireSingleArg) -> bool {
    if (hasNamedArguments(expr.argNames) ||
        isUnimportedStdNamespacedVectorCompatibilityDirectCall(
            expr.isMethodCall,
            resolveCalleePath(expr),
            "capacity",
            hasImportedDefinitionPath("/std/collections/vector/capacity")) ||
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
    const auto resolveCapacityMethodTarget = [&]() -> bool {
      if (resolveVectorHelperMethodTarget(params, locals, expr.args.front(), "capacity",
                                          methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (hasResolvableDefinitionPath(methodResolved)) {
          isBuiltinMethod = false;
          return true;
        }
      }
      if (isStdNamespacedVectorCompatibilityHelperPath(resolveCalleePath(expr),
                                                       "capacity")) {
        methodResolved = "/std/collections/vector/capacity";
        isBuiltinMethod = true;
        return true;
      }
      return resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "capacity",
                                 methodResolved, isBuiltinMethod);
    };
    if (!resolveCapacityMethodTarget()) {
      (void)validateExpr(params, locals, expr.args.front());
      return false;
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
        resolved.rfind(methodResolved + "__t", 0) == 0) {
      methodResolved = resolved;
    }
    if (!removedRootedVectorDirectCallDiagnostic.empty()) {
      return failCollectionCountCapacityDiagnostic(
          removedRootedVectorDirectCallDiagnostic);
    }
    bool methodResolvedMissing =
        !isBuiltinMethod && !hasResolvableDefinitionPath(methodResolved);
    if (methodResolvedMissing) {
      if (requireSingleArg &&
          (context.isNonCollectionStructCapacityTarget == nullptr ||
           !context.isNonCollectionStructCapacityTarget(methodResolved))) {
        if (context.promoteCapacityToBuiltinValidation != nullptr) {
          context.promoteCapacityToBuiltinValidation(expr.args.front(), methodResolved,
                                                     isBuiltinMethod, false);
          methodResolvedMissing =
              !isBuiltinMethod && !hasResolvableDefinitionPath(methodResolved);
        }
      }
    }
    if (methodResolvedMissing) {
      return failCollectionCountCapacityDiagnostic("unknown method: " + methodResolved);
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

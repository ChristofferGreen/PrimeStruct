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
                hasResolvableDefinitionPath("/std/collections/vector/count")));
        !stdNamespacedVectorCountDiagnosticMessage.empty()) {
      handledOut = true;
      return failCollectionCountCapacityDiagnostic(
          stdNamespacedVectorCountDiagnosticMessage);
    }
  }
  auto resolveCountMethod = [&](bool requireSingleArg) -> bool {
    if (hasNamedArguments(expr.argNames) ||
        isUnimportedStdNamespacedVectorCompatibilityDirectCall(
            expr.isMethodCall,
            resolveCalleePath(expr),
            "count",
            hasImportedDefinitionPath("/std/collections/vector/count")) ||
        expr.args.empty()) {
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
    if (!(isVectorBuiltinName(expr, "count") ||
          context.isStdNamespacedMapCountCall ||
          context.isNamespacedMapCountCall ||
          context.isUnnamespacedMapCountFallbackCall ||
          context.isResolvedMapCountCall)) {
      return false;
    }
    if (!(requireSingleArg
              ? ((defMap_.find(resolved) == defMap_.end() &&
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
                 context.isResolvedMapCountCall)
              : (defMap_.find(resolved) != defMap_.end() ||
                 context.isStdNamespacedMapCountCall ||
                 context.isNamespacedMapCountCall ||
                 context.isUnnamespacedMapCountFallbackCall ||
                 context.isResolvedMapCountCall))) {
      return false;
    }

    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    const Expr &receiver = expr.args.front();
    bool isBuiltinMethod = false;
    std::string methodResolved;
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
    } else if (resolveVectorHelperMethodTarget(params, locals, expr.args.front(),
                                               "count", methodResolved)) {
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      if (hasResolvableDefinitionPath(methodResolved)) {
        isBuiltinMethod = false;
      } else if (!resolveMethodTarget(
                     params,
                     locals,
                     expr.namespacePrefix,
                     expr.args.front(),
                     "count",
                     methodResolved,
                     isBuiltinMethod)) {
        if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
            expr.args.empty()) {
          (void)validateExpr(params, locals, expr.args.front());
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
            (void)validateExpr(params, locals, expr.args.front());
            return false;
          }
          methodResolved = "/" + typeName + "/count";
          error_.clear();
          isBuiltinMethod = false;
        }
      }
    } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix,
                                    expr.args.front(), "count", methodResolved,
                                    isBuiltinMethod)) {
      if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
          expr.args.empty()) {
        (void)validateExpr(params, locals, expr.args.front());
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
          (void)validateExpr(params, locals, expr.args.front());
          return false;
        }
        methodResolved = "/" + typeName + "/count";
        error_.clear();
        isBuiltinMethod = false;
      }
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
        resolved.rfind(methodResolved + "__t", 0) == 0) {
      methodResolved = resolved;
    }
    if ((!expr.isMethodCall &&
         expr.args.size() == 1 &&
         expr.args.front().kind == Expr::Kind::Name &&
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
      return failCollectionCountCapacityDiagnostic("unknown call target: " +
                                                  stdlibMapCountMethodTarget);
    }
    const std::string removedRootedVectorDirectCallDiagnostic =
        getRemovedRootedVectorDirectCallDiagnostic(expr);
    if (!removedRootedVectorDirectCallDiagnostic.empty()) {
      return failCollectionCountCapacityDiagnostic(
          removedRootedVectorDirectCallDiagnostic);
    }
    if (!isBuiltinMethod && !hasResolvableDefinitionPath(methodResolved)) {
      return failCollectionCountCapacityDiagnostic("unknown method: " +
                                                  methodResolved);
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
    if (resolveVectorHelperMethodTarget(params, locals, expr.args.front(),
                                        "capacity", methodResolved)) {
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      if (hasResolvableDefinitionPath(methodResolved)) {
        isBuiltinMethod = false;
      } else if (isStdNamespacedVectorCompatibilityHelperPath(
                     resolveCalleePath(expr), "capacity")) {
        methodResolved = "/std/collections/vector/capacity";
        isBuiltinMethod = true;
      } else if (!resolveMethodTarget(
                     params,
                     locals,
                     expr.namespacePrefix,
                     expr.args.front(),
                     "capacity",
                     methodResolved,
                     isBuiltinMethod)) {
        (void)validateExpr(params, locals, expr.args.front());
        return false;
      }
    } else if (isStdNamespacedVectorCompatibilityHelperPath(
                   resolveCalleePath(expr), "capacity")) {
      methodResolved = "/std/collections/vector/capacity";
      isBuiltinMethod = true;
    } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix,
                                    expr.args.front(), "capacity",
                                    methodResolved, isBuiltinMethod)) {
      (void)validateExpr(params, locals, expr.args.front());
      return false;
    }
    if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
        resolved.rfind(methodResolved + "__t", 0) == 0) {
      methodResolved = resolved;
    }
    if (!isBuiltinMethod && !hasResolvableDefinitionPath(methodResolved)) {
      if (requireSingleArg &&
          (context.isNonCollectionStructCapacityTarget == nullptr ||
           !context.isNonCollectionStructCapacityTarget(methodResolved)) &&
          context.promoteCapacityToBuiltinValidation != nullptr) {
        context.promoteCapacityToBuiltinValidation(expr.args.front(), methodResolved,
                                                   isBuiltinMethod, false);
      }
    }
    if (!isBuiltinMethod && !hasResolvableDefinitionPath(methodResolved)) {
      return failCollectionCountCapacityDiagnostic("unknown method: " + methodResolved);
    }
    const std::string removedRootedVectorDirectCallDiagnostic =
        getRemovedRootedVectorDirectCallDiagnostic(expr);
    if (!removedRootedVectorDirectCallDiagnostic.empty()) {
      return failCollectionCountCapacityDiagnostic(
          removedRootedVectorDirectCallDiagnostic);
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

#include "SemanticsValidator.h"

#include <string_view>

namespace primec::semantics {

bool SemanticsValidator::prepareExprMethodCompatibilitySetup(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprDispatchBootstrap &dispatchBootstrap,
    bool hasVectorHelperCallResolution,
    const std::string &vectorHelperCallResolvedPath,
    size_t vectorHelperCallReceiverIndex,
    std::string &resolved,
    ExprMethodCompatibilitySetup &setupOut) {
  setupOut = {};
  auto failMethodCompatibilityDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (hasVectorHelperCallResolution) {
    resolved = vectorHelperCallResolvedPath;
    setupOut.usedMethodTarget = true;
    setupOut.hasMethodReceiverIndex = true;
    setupOut.methodReceiverIndex = vectorHelperCallReceiverIndex;
  }

  const std::string methodReflectionTarget =
      describeMethodReflectionTarget(params, locals, expr);
  if (!methodReflectionTarget.empty()) {
    if (methodReflectionTarget == "meta.object" ||
        methodReflectionTarget == "meta.table") {
      return failMethodCompatibilityDiagnostic(
          "runtime reflection objects/tables are unsupported: " +
          methodReflectionTarget);
    } else if (isReflectionMetadataQueryName(expr.name)) {
      return failMethodCompatibilityDiagnostic(
          "reflection metadata queries are compile-time only and not yet "
          "implemented: " +
          methodReflectionTarget);
    } else {
      return failMethodCompatibilityDiagnostic(
          "unsupported reflection metadata query: " + methodReflectionTarget);
    }
  }
  if (defMap_.count(resolved) == 0) {
    if (isReflectionMetadataQueryPath(resolved)) {
      return failMethodCompatibilityDiagnostic(
          "reflection metadata queries are compile-time only and not yet "
          "implemented: " +
          resolved);
    }
    if (isRuntimeReflectionPath(resolved)) {
      return failMethodCompatibilityDiagnostic(
          "runtime reflection objects/tables are unsupported: " + resolved);
    }
    if (resolved.rfind("/meta/", 0) == 0) {
      const std::string queryName = resolved.substr(6);
      if (!queryName.empty() && queryName.find('/') == std::string::npos) {
        return failMethodCompatibilityDiagnostic(
            "unsupported reflection metadata query: " + resolved);
      }
    }
  }
  if (!expr.isMethodCall &&
      isArrayNamespacedVectorCountCompatibilityCall(
          expr, dispatchBootstrap.dispatchResolvers)) {
    return failMethodCompatibilityDiagnostic("unknown call target: /array/count");
  }
  if (!expr.isMethodCall &&
      isArrayNamespacedVectorAccessCompatibilityCall(
          expr, dispatchBootstrap.dispatchResolvers)) {
    std::string compatibilityPath = resolveCalleePath(expr);
    if (compatibilityPath.empty()) {
      compatibilityPath = expr.name;
      if (!compatibilityPath.empty() && compatibilityPath.front() != '/') {
        compatibilityPath.insert(compatibilityPath.begin(), '/');
      }
    }
    return failMethodCompatibilityDiagnostic("unknown call target: " +
                                             compatibilityPath);
  }
  if (!expr.isMethodCall) {
    const std::string removedVectorCompatibilityPath =
        getDirectVectorHelperCompatibilityPath(expr);
    if (!removedVectorCompatibilityPath.empty()) {
      return failMethodCompatibilityDiagnostic("unknown call target: " +
                                               removedVectorCompatibilityPath);
    }
    const std::string removedMapCompatibilityPath = directMapHelperCompatibilityPath(
        expr, params, locals, dispatchBootstrap.dispatchResolverAdapters);
    if (!removedMapCompatibilityPath.empty()) {
      return failMethodCompatibilityDiagnostic("unknown call target: " +
                                               removedMapCompatibilityPath);
    }
  }

  auto isKnownCollectionTarget = [this, &dispatchBootstrap](
                                     const Expr &targetExpr) -> bool {
    std::string elemType;
    return dispatchBootstrap.dispatchResolvers.resolveVectorTarget(targetExpr,
                                                                   elemType) ||
           dispatchBootstrap.dispatchResolvers.resolveArrayTarget(targetExpr,
                                                                  elemType) ||
           dispatchBootstrap.dispatchResolvers.resolveStringTarget(targetExpr) ||
           dispatchBootstrap.resolveMapTarget(targetExpr);
  };
  setupOut.promoteCapacityToBuiltinValidation =
      [isKnownCollectionTarget](const Expr &targetExpr, std::string &resolvedOut,
                                bool &isBuiltinMethodOut,
                                bool requireKnownCollection) {
        if (requireKnownCollection && !isKnownCollectionTarget(targetExpr)) {
          return;
        }
        // Route unresolved capacity() calls through builtin validation so
        // non-vector targets emit deterministic vector-target diagnostics.
        resolvedOut = "/std/collections/vector/capacity";
        isBuiltinMethodOut = true;
      };
  setupOut.isNonCollectionStructCapacityTarget =
      [this](const std::string &resolvedPath) -> bool {
    constexpr std::string_view suffix = "/capacity";
    if (resolvedPath.size() <= suffix.size() ||
        resolvedPath.compare(resolvedPath.size() - suffix.size(),
                             suffix.size(), suffix) != 0) {
      return false;
    }
    const std::string receiverPath =
        resolvedPath.substr(0, resolvedPath.size() - suffix.size());
    if (receiverPath == "/array" || receiverPath == "/vector" ||
        receiverPath == "/map" || receiverPath == "/string") {
      return false;
    }
    return structNames_.count(receiverPath) > 0;
  };
  setupOut.unavailableMethodDiagnostic =
      [this](const std::string &resolvedPath) -> std::string {
    if (resolvedPath == "/std/gfx/experimental/Device/create_pipeline") {
      return "experimental gfx entry point not implemented yet: "
             "Device.create_pipeline([vertex_type] type, ...)";
    }
    return soaUnavailableMethodDiagnostic(resolvedPath);
  };
  return true;
}

} // namespace primec::semantics

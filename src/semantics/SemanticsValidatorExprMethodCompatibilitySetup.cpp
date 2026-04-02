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
      error_ = "runtime reflection objects/tables are unsupported: " +
               methodReflectionTarget;
    } else if (isReflectionMetadataQueryName(expr.name)) {
      error_ =
          "reflection metadata queries are compile-time only and not yet "
          "implemented: " +
          methodReflectionTarget;
    } else {
      error_ =
          "unsupported reflection metadata query: " + methodReflectionTarget;
    }
    return false;
  }
  if (defMap_.count(resolved) == 0) {
    if (isReflectionMetadataQueryPath(resolved)) {
      error_ =
          "reflection metadata queries are compile-time only and not yet "
          "implemented: " +
          resolved;
      return false;
    }
    if (isRuntimeReflectionPath(resolved)) {
      error_ = "runtime reflection objects/tables are unsupported: " + resolved;
      return false;
    }
    if (resolved.rfind("/meta/", 0) == 0) {
      const std::string queryName = resolved.substr(6);
      if (!queryName.empty() && queryName.find('/') == std::string::npos) {
        error_ = "unsupported reflection metadata query: " + resolved;
        return false;
      }
    }
  }
  if (!expr.isMethodCall &&
      isArrayNamespacedVectorCountCompatibilityCall(
          expr, dispatchBootstrap.dispatchResolvers)) {
    error_ = "unknown call target: /array/count";
    return false;
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
    error_ = "unknown call target: " + compatibilityPath;
    return false;
  }
  if (!expr.isMethodCall) {
    const std::string removedVectorCompatibilityPath =
        getDirectVectorHelperCompatibilityPath(expr);
    if (!removedVectorCompatibilityPath.empty()) {
      error_ = "unknown call target: " + removedVectorCompatibilityPath;
      return false;
    }
    const std::string removedMapCompatibilityPath = directMapHelperCompatibilityPath(
        expr, params, locals, dispatchBootstrap.dispatchResolverAdapters);
    if (!removedMapCompatibilityPath.empty()) {
      error_ = "unknown call target: " + removedMapCompatibilityPath;
      return false;
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
        resolvedOut = "/vector/capacity";
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
    return soaUnavailableMethodDiagnostic(
        resolvedPath,
        hasDeclaredDefinitionPath("/soa_vector/ref") ||
            hasImportedDefinitionPath("/soa_vector/ref"));
  };
  return true;
}

} // namespace primec::semantics

#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

std::string_view fileMethodLeaf(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view normalizeFileMethodLeaf(std::string_view methodName) {
  if (methodName == "readByte") {
    return "read_byte";
  }
  if (methodName == "writeLine") {
    return "write_line";
  }
  if (methodName == "writeByte") {
    return "write_byte";
  }
  if (methodName == "writeBytes") {
    return "write_bytes";
  }
  return methodName;
}

bool isFileBuiltinMethodLeaf(std::string_view methodName) {
  methodName = normalizeFileMethodLeaf(methodName);
  return methodName == "write" || methodName == "write_line" ||
         methodName == "write_byte" || methodName == "read_byte" ||
         methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

} // namespace

bool SemanticsValidator::validateExprMethodCallTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprMethodResolutionContext &context,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    std::string &resolved,
    bool &resolvedMethod,
    bool &usedMethodTarget,
    bool &hasMethodReceiverIndex,
    size_t &methodReceiverIndex) {
  if (!expr.isMethodCall) {
    return true;
  }

  auto failMethodResolutionDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
      };
  const auto isValueSurfaceAccessMethodName = [](std::string_view helperName) {
    return helperName == "at" || helperName == "at_unsafe";
  };
  const auto isCanonicalMapAccessMethodName = [&](std::string_view helperName) {
    return isValueSurfaceAccessMethodName(helperName) ||
           helperName == "size" ||
           helperName == "at_ref" || helperName == "at_unsafe_ref";
  };

  const auto &resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  const auto &resolveMapTargetWithTypes = dispatchResolvers.resolveMapTarget;
  const auto &resolveExperimentalMapTarget = dispatchResolvers.resolveExperimentalMapTarget;
  const std::string normalizedMethodName =
      normalizeCollectionMethodName(expr.name);
  auto resolveMapTarget = [&](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (resolveMapTargetWithTypes(target, keyType, valueType) ||
        resolveExperimentalMapTarget(target, keyType, valueType)) {
      return true;
    }
    std::string inferredTypeText;
    return inferQueryExprTypeText(target, params, locals, inferredTypeText) &&
           returnsMapCollectionType(inferredTypeText);
  };
  const bool isVectorCompatibilityMethod =
      isVectorCompatibilityHelperName(normalizedMethodName);
  if (expr.namespacePrefix.empty() &&
      !expr.args.empty() &&
      isVectorCompatibilityMethod) {
    std::string elemType;
    const bool isVectorReceiver = resolveVectorTarget(expr.args.front(), elemType);
    const std::string canonicalVectorHelperPath =
        "/std/collections/vector/" + normalizedMethodName;
    if (isVectorReceiver &&
        (hasDefinitionFamilyPath(canonicalVectorHelperPath) ||
         hasImportedDefinitionPath(canonicalVectorHelperPath))) {
      if (normalizedMethodName == "count" && expr.args.size() != 1) {
        return failMethodResolutionDiagnostic("argument count mismatch for builtin count");
      }
      if (normalizedMethodName == "capacity" && expr.args.size() != 1) {
        return failMethodResolutionDiagnostic("argument count mismatch for builtin capacity");
      }
      if ((normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
          expr.args.size() == 2) {
        const ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
        if (indexKind != ReturnKind::Int &&
            indexKind != ReturnKind::Int64 &&
            indexKind != ReturnKind::UInt64) {
          return failMethodResolutionDiagnostic(normalizedMethodName + " requires integer index");
        }
      }
    }
  }
  auto rejectBuiltinStringCountShadowOnMapAccessReceiver = [&](const std::string &resolvedPath) -> bool {
    if (resolvedPath != "/string/count" || expr.args.empty()) {
      return false;
    }
    const Expr &receiverExpr = expr.args.front();
    if (receiverExpr.kind != Expr::Kind::Call || !receiverExpr.isMethodCall) {
      return false;
    }
    if (inferExprReturnKind(receiverExpr, params, locals) == ReturnKind::String) {
      return false;
    }
    std::string accessHelperName;
    if (!getBuiltinArrayAccessName(receiverExpr, accessHelperName) ||
        !isCanonicalMapAccessMethodName(accessHelperName)) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(receiverExpr);
    if (accessReceiver == nullptr) {
      return false;
    }
    std::string mapValueType;
    if (!this->resolveMapValueType(*accessReceiver, dispatchResolvers, mapValueType) ||
        normalizeBindingTypeName(mapValueType) == "string") {
      return false;
    }
    return failMethodResolutionDiagnostic("argument type mismatch for /string/count parameter values: expected string");
  };
  const std::string removedMapMethodPath =
      this->mapNamespacedMethodCompatibilityPath(expr, params, locals, dispatchResolverAdapters);
  if (!removedMapMethodPath.empty()) {
    return failMethodResolutionDiagnostic("unknown method: " + removedMapMethodPath);
  }
  if (context.hasVectorHelperCallResolution) {
    resolvedMethod = false;
    return true;
  }
  if (expr.args.empty()) {
    return failMethodResolutionDiagnostic("method call missing receiver");
  }
  usedMethodTarget = true;
  hasMethodReceiverIndex = true;
  methodReceiverIndex = 0;
  bool isBuiltinMethod = false;
  const bool hasBlockArgs = expr.hasBodyArguments || !expr.bodyArguments.empty();
  auto shouldRewriteExperimentalVectorCompatibilityMethodTargetToCanonical =
      [&](std::string_view methodTarget) {
        const std::string canonicalVectorCompatibilityMethodTarget =
            "/std/collections/vector/" + expr.name;
        return (methodTarget.rfind("/std/collections/experimental_vector/", 0) == 0 ||
                methodTarget.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) &&
               (hasImportedDefinitionPath(canonicalVectorCompatibilityMethodTarget) ||
                defMap_.count(canonicalVectorCompatibilityMethodTarget) > 0);
  };
  std::string vectorMethodTarget;
  bool resolvedCanonicalVectorCompatibilityMethod = false;
  if (isVectorCompatibilityMethod &&
      expr.namespacePrefix != "vector" &&
      expr.namespacePrefix != "/vector" &&
      expr.namespacePrefix != "std/collections/vector" &&
      expr.namespacePrefix != "/std/collections/vector" &&
      resolveVectorHelperMethodTarget(params, locals, expr.args.front(), normalizedMethodName,
                                      vectorMethodTarget)) {
    if (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
        normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string preferredVectorMethodTarget =
            preferredBareVectorHelperTarget(normalizedMethodName);
        if ((normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
            preferredVectorMethodTarget.rfind("/vector/", 0) == 0 &&
            (hasDeclaredDefinitionPath(preferredVectorMethodTarget) ||
             hasImportedDefinitionPath(preferredVectorMethodTarget))) {
          resolved = preferredVectorMethodTarget;
          isBuiltinMethod = false;
        } else {
          resolved = "/std/collections/vector/" + normalizedMethodName;
          isBuiltinMethod = normalizedMethodName == "count" ||
                            normalizedMethodName == "capacity";
          resolvedCanonicalVectorCompatibilityMethod = true;
        }
      }
    }
    if (!isBuiltinMethod && !resolvedCanonicalVectorCompatibilityMethod) {
      if (!hasImportedDefinitionPath(vectorMethodTarget) &&
          defMap_.count(vectorMethodTarget) == 0 &&
          shouldRewriteExperimentalVectorCompatibilityMethodTargetToCanonical(vectorMethodTarget)) {
        vectorMethodTarget = "/std/collections/vector/" + expr.name;
      }
      if (hasImportedDefinitionPath(vectorMethodTarget) ||
          defMap_.count(vectorMethodTarget) > 0) {
        resolved = vectorMethodTarget;
        isBuiltinMethod = false;
      } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(),
                                      expr.name, resolved, isBuiltinMethod)) {
        if (hasBlockArgs &&
            resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
          isBuiltinMethod = false;
        } else {
          return false;
        }
      }
    }
  } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), expr.name, resolved,
                                  isBuiltinMethod)) {
    std::string collectionMethodTarget;
    const bool resolvedVisibleCollectionMethod =
        (expr.name == "get" || expr.name == "get_ref" ||
         expr.name == "ref" || expr.name == "ref_ref" ||
         expr.name == "to_aos" || expr.name == "to_aos_ref") &&
        resolveVectorHelperMethodTarget(params, locals, expr.args.front(), expr.name,
                                        collectionMethodTarget) &&
        hasImportedDefinitionPath(collectionMethodTarget);
    bool promotedCapacityToBuiltinValidation = false;
    if (resolvedVisibleCollectionMethod) {
      resolved = collectionMethodTarget;
      isBuiltinMethod = false;
    } else if (isVectorBuiltinName(expr, "capacity") &&
        isStdNamespacedVectorCompatibilityHelperPath(resolveCalleePath(expr),
                                                     "capacity")) {
      context.promoteCapacityToBuiltinValidation(expr.args.front(), resolved,
                                                 isBuiltinMethod, true);
      promotedCapacityToBuiltinValidation = isBuiltinMethod;
    }
    auto resolveInferredMapMethodFallback = [&]() -> bool {
      const std::string helperName = expr.name;
      const bool requestsExplicitVectorHelperNamespace =
          expr.namespacePrefix == "vector" || expr.namespacePrefix == "/vector" ||
          expr.namespacePrefix == "std/collections/vector" ||
          expr.namespacePrefix == "/std/collections/vector" ||
          helperName.rfind("/vector/", 0) == 0 ||
          helperName.rfind("/std/collections/vector/", 0) == 0;
      if (requestsExplicitVectorHelperNamespace) {
        return false;
      }
      if (!(helperName == "count" || helperName == "count_ref" ||
            helperName == "size" ||
            helperName == "contains" || helperName == "contains_ref" ||
            helperName == "tryAt" || helperName == "tryAt_ref" ||
            helperName == "at" || helperName == "at_ref" ||
            helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
            helperName == "insert" || helperName == "insert_ref") ||
          !resolveMapTarget(expr.args.front())) {
        return false;
      }
      const std::string canonicalMapMethodTarget =
          "/std/collections/map/" + helperName;
      const std::string aliasMapMethodTarget =
          "/map/" + helperName;
      if (hasDeclaredDefinitionPath(canonicalMapMethodTarget) ||
          hasImportedDefinitionPath(canonicalMapMethodTarget)) {
        resolved = canonicalMapMethodTarget;
      } else if (hasDeclaredDefinitionPath(aliasMapMethodTarget) ||
                 hasImportedDefinitionPath(aliasMapMethodTarget)) {
        resolved = aliasMapMethodTarget;
      } else {
        resolved = canonicalMapMethodTarget;
      }
      if (resolved.rfind("/std/collections/map/", 0) == 0 &&
          (shouldBuiltinValidateCurrentMapWrapperHelper(helperName) ||
           hasImportedDefinitionPath(resolved))) {
        isBuiltinMethod = true;
      } else {
        isBuiltinMethod = defMap_.count(resolved) == 0 &&
                          !hasImportedDefinitionPath(resolved);
      }
      return true;
    };
    if (resolvedVisibleCollectionMethod) {
    } else if (promotedCapacityToBuiltinValidation) {
    } else if (resolveInferredMapMethodFallback()) {
    } else if (hasBlockArgs &&
               resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
      isBuiltinMethod = false;
    } else {
      return false;
    }
  } else if (rejectBuiltinStringCountShadowOnMapAccessReceiver(resolved)) {
    return failMethodResolutionDiagnostic(error_);
  } else if (hasBlockArgs) {
    const std::string pointerLikeType =
        inferPointerLikeCallReturnType(expr.args.front(), params, locals);
    if (!pointerLikeType.empty()) {
      resolved = "/" + pointerLikeType + "/" + normalizeCollectionMethodName(expr.name);
      isBuiltinMethod = false;
    }
  }
  if (!isBuiltinMethod && isVectorCompatibilityMethod && !resolved.empty() &&
      shouldRewriteExperimentalVectorCompatibilityMethodTargetToCanonical(resolved)) {
    resolved = "/std/collections/vector/" + expr.name;
  }
  if (resolved == "/std/collections/vector/capacity") {
    isBuiltinMethod = true;
  } else if (resolved == "/std/collections/vector/count" &&
             !expr.args.empty()) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType)) {
      isBuiltinMethod = true;
    }
  }
  bool keepBuiltinIndexedArgsPackMapMethod = false;
  keepBuiltinIndexedArgsPackMapMethod = resolveMapTarget(expr.args.front());
  if (expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr.args.front(), accessName) && expr.args.front().args.size() == 2) {
      if (const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(expr.args.front())) {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        keepBuiltinIndexedArgsPackMapMethod =
            keepBuiltinIndexedArgsPackMapMethod ||
            (resolveArgsPackElementTypeForExpr(*accessReceiver, params, locals, elemType) &&
             extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
      }
    }
  }
  auto hasVisibleStdlibMapMethodDefinition = [&](const std::string &path) {
    return hasImportedDefinitionPath(path) || hasDeclaredDefinitionPath(path);
  };
  auto isMissingStdlibMapMethodDefinition = [&](const std::string &path) {
    return (path == "/std/collections/map/count" ||
            path == "/std/collections/map/count_ref" ||
            path == "/std/collections/map/contains" ||
            path == "/std/collections/map/contains_ref" ||
            path == "/std/collections/map/tryAt" ||
            path == "/std/collections/map/tryAt_ref" ||
            path == "/std/collections/map/at" ||
            path == "/std/collections/map/at_ref" ||
            path == "/std/collections/map/at_unsafe" ||
            path == "/std/collections/map/at_unsafe_ref" ||
            path == "/std/collections/map/insert" ||
            path == "/std/collections/map/insert_ref") &&
           !hasVisibleStdlibMapMethodDefinition(path);
  };
  if (isMissingStdlibMapMethodDefinition(resolved) &&
      !keepBuiltinIndexedArgsPackMapMethod) {
    isBuiltinMethod = false;
  }
  if (expr.isMethodCall &&
      expr.namespacePrefix.empty() &&
      !expr.args.empty() &&
      (expr.name == "at" || expr.name == "at_unsafe")) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType)) {
      const std::string methodPath = preferredBareVectorHelperTarget(expr.name);
      if (!hasDeclaredDefinitionPath(methodPath) &&
          !hasImportedDefinitionPath(methodPath)) {
        return failMethodResolutionDiagnostic("unknown method: " + methodPath);
      }
    }
  }
  auto isStdlibFileWriteFacadeResolvedPath = [&](const std::string &path) {
    return path == "/File/write" || path == "/File/write_line" ||
           path == "/std/file/File/write" || path == "/std/file/File/write_line";
  };
  if (!isBuiltinMethod && isStdlibFileWriteFacadeResolvedPath(resolved) &&
      expr.args.size() > 10 &&
      (expr.name == "write" || expr.name == "write_line") &&
      !hasDeclaredDefinitionPath(resolved)) {
    resolved = "/file/" + expr.name;
    isBuiltinMethod = true;
  }
  if (!isBuiltinMethod &&
      (resolved.rfind("/std/file/File/", 0) == 0 ||
       resolved.rfind("/File/", 0) == 0) &&
      !hasDeclaredDefinitionPath(resolved) &&
      !hasImportedDefinitionPath(resolved)) {
    const std::string_view helperName = fileMethodLeaf(resolved);
    if (isFileBuiltinMethodLeaf(helperName)) {
      resolved = "/file/" + std::string(normalizeFileMethodLeaf(helperName));
      isBuiltinMethod = true;
    }
  }
  if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
      isVectorBuiltinName(expr, "capacity")) {
    context.promoteCapacityToBuiltinValidation(expr.args.front(), resolved, isBuiltinMethod, true);
  }
  if (!isBuiltinMethod &&
      resolved.rfind("/std/collections/vector/count", 0) == 0 &&
      expr.args.size() != 1) {
    if (hasNamedArguments(expr.argNames)) {
      return failMethodResolutionDiagnostic(
          "named arguments not supported for builtin calls");
    }
    return failMethodResolutionDiagnostic(
        "argument count mismatch for builtin count");
  }
  if (!isBuiltinMethod &&
      resolved.rfind("/std/collections/vector/capacity", 0) == 0 &&
      expr.args.size() != 1) {
    if (hasNamedArguments(expr.argNames)) {
      return failMethodResolutionDiagnostic(
          "named arguments not supported for builtin calls");
    }
    return failMethodResolutionDiagnostic(
        "argument count mismatch for builtin capacity");
  }
  if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end()) {
    const std::string canonicalSoaGetPath =
        canonicalizeLegacySoaGetHelperPath(resolved);
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
        isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref")) {
      isBuiltinMethod = true;
    }
  }
  if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
      !hasImportedDefinitionPath(resolved) && !hasBlockArgs) {
    if (const std::string diagnostic = context.unavailableMethodDiagnostic(resolved);
        !diagnostic.empty()) {
      return failMethodResolutionDiagnostic(diagnostic);
    } else {
      return failMethodResolutionDiagnostic("unknown method: " + resolved);
    }
  }
  resolvedMethod = isBuiltinMethod;
  return true;
}

} // namespace primec::semantics

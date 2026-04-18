#include "SemanticsValidator.h"
#include "SemanticsVectorCompatibilityHelpers.h"

#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

bool isExperimentalVectorCompatibilityMethodTarget(std::string_view methodTarget) {
  return methodTarget.rfind("/std/collections/experimental_vector/", 0) == 0 ||
         methodTarget.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
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
           helperName == "at_ref" || helperName == "at_unsafe_ref";
  };

  const auto &resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  const auto &resolveMapTargetWithTypes = dispatchResolvers.resolveMapTarget;
  const auto &resolveExperimentalMapTarget = dispatchResolvers.resolveExperimentalMapTarget;
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
  auto isFileBinding = [&](const BindingInfo &binding) {
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType == "File") {
      return true;
    }
    if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
        !binding.typeTemplateArg.empty()) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(binding.typeTemplateArg, base, argText)) {
        return false;
      }
      return normalizeBindingTypeName(base) == "File";
    }
    return false;
  };
  auto isFileReceiverExpr = [&](const Expr &target) {
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return isFileBinding(*paramBinding);
    }
    auto it = locals.find(target.name);
    return it != locals.end() && isFileBinding(it->second);
  };

  const bool isVectorCompatibilityMethod =
      isVectorCompatibilityHelperName(expr.name);
  const std::string canonicalVectorCompatibilityMethodTarget =
      "/std/collections/vector/" + expr.name;
  const bool hasVisibleCanonicalVectorCompatibilityMethodTarget =
      hasImportedDefinitionPath(canonicalVectorCompatibilityMethodTarget) ||
      defMap_.count(canonicalVectorCompatibilityMethodTarget) > 0;
  auto rewriteExperimentalVectorCompatibilityMethodTargetToCanonical =
      [&](std::string &methodTarget) {
        if (isExperimentalVectorCompatibilityMethodTarget(methodTarget) &&
            hasVisibleCanonicalVectorCompatibilityMethodTarget) {
          methodTarget = canonicalVectorCompatibilityMethodTarget;
        }
      };
  if (expr.namespacePrefix.empty() &&
      !expr.args.empty() &&
      isVectorCompatibilityMethod) {
    std::string elemType;
    const bool isVectorReceiver = resolveVectorTarget(expr.args.front(), elemType);
    if (isVectorReceiver &&
        hasDeclaredDefinitionPath("/std/collections/vector/" + expr.name) &&
        !hasImportedDefinitionPath("/std/collections/vector/" + expr.name)) {
      if (expr.name == "count" && expr.args.size() != 1) {
        return failMethodResolutionDiagnostic("argument count mismatch for builtin count");
      }
      if (expr.name == "capacity" && expr.args.size() != 1) {
        return failMethodResolutionDiagnostic("argument count mismatch for builtin capacity");
      }
      if ((expr.name == "at" || expr.name == "at_unsafe") &&
          expr.args.size() == 2) {
        const ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
        if (indexKind != ReturnKind::Int &&
            indexKind != ReturnKind::Int64 &&
            indexKind != ReturnKind::UInt64) {
          return failMethodResolutionDiagnostic(expr.name + " requires integer index");
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
  if (expr.args.size() > 10 &&
      (expr.name == "write" || expr.name == "write_line") &&
      isFileReceiverExpr(expr.args.front())) {
    return failMethodResolutionDiagnostic(
        "stdlib File write/write_line currently support up to nine values; broader arities await [args<T>] runtime support");
  }
  usedMethodTarget = true;
  hasMethodReceiverIndex = true;
  methodReceiverIndex = 0;
  bool isBuiltinMethod = false;
  const bool hasBlockArgs = expr.hasBodyArguments || !expr.bodyArguments.empty();
  std::string vectorMethodTarget;
  if (isVectorCompatibilityMethod &&
      expr.namespacePrefix != "vector" &&
      expr.namespacePrefix != "/vector" &&
      expr.namespacePrefix != "std/collections/vector" &&
      expr.namespacePrefix != "/std/collections/vector" &&
      resolveVectorHelperMethodTarget(params, locals, expr.args.front(), expr.name,
                                      vectorMethodTarget)) {
    const bool vectorMethodTargetMissing =
        !hasImportedDefinitionPath(vectorMethodTarget) &&
        defMap_.count(vectorMethodTarget) == 0;
    if (vectorMethodTargetMissing) {
      rewriteExperimentalVectorCompatibilityMethodTargetToCanonical(vectorMethodTarget);
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
  } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), expr.name, resolved,
                                  isBuiltinMethod)) {
    auto resolveInferredMapMethodFallback = [&]() -> bool {
      const std::string helperName = expr.name;
      if (!(helperName == "count" || helperName == "count_ref" ||
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
    if (resolveInferredMapMethodFallback()) {
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
  if (!isBuiltinMethod && isVectorCompatibilityMethod &&
      !resolved.empty()) {
    rewriteExperimentalVectorCompatibilityMethodTargetToCanonical(resolved);
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
  auto bareVectorAccessMethodMissingSamePathHelper = [&]() -> std::string {
    if (!expr.isMethodCall || !expr.namespacePrefix.empty() || expr.args.empty()) {
      return "";
    }
    if (expr.name != "at" && expr.name != "at_unsafe") {
      return "";
    }
    std::string elemType;
    if (!resolveVectorTarget(expr.args.front(), elemType)) {
      return "";
    }
    const std::string methodPath = preferredBareVectorHelperTarget(expr.name);
    return hasDeclaredDefinitionPath(methodPath) || hasImportedDefinitionPath(methodPath)
               ? ""
               : methodPath;
  };
  if (const std::string missingSamePathHelper =
          bareVectorAccessMethodMissingSamePathHelper();
      !missingSamePathHelper.empty()) {
    return failMethodResolutionDiagnostic("unknown method: " + missingSamePathHelper);
  }
  auto isStdlibFileWriteFacadeResolvedPath = [&](const std::string &path) {
    return path == "/File/write" || path == "/File/write_line" ||
           path == "/std/file/File/write" || path == "/std/file/File/write_line";
  };
  if (!isBuiltinMethod && isStdlibFileWriteFacadeResolvedPath(resolved) &&
      expr.args.size() > 10) {
    return failMethodResolutionDiagnostic(
        "stdlib File write/write_line currently support up to nine values; broader arities await [args<T>] runtime support");
  }
  if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
      isVectorBuiltinName(expr, "capacity")) {
    context.promoteCapacityToBuiltinValidation(expr.args.front(), resolved, isBuiltinMethod, true);
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

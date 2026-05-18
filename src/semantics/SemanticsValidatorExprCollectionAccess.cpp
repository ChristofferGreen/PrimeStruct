#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

bool isValueSurfaceAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_unsafe";
}

bool isCanonicalKeyValueAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

bool isSoaAccessHelperName(const std::string &helperName) {
  return helperName == "get" || helperName == "get_ref" ||
         helperName == "ref" || helperName == "ref_ref";
}

bool isSoaReceiverStructPath(const std::string &structPath) {
  return structPath == "/soa" "_vector" ||
         structPath == "/std/collections/" "soa" "_vector" ||
         structPath.rfind("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0;
}

const StdlibSurfaceMetadata *collectionAccessKeyValueHelperMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      collectionAccessKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

std::string canonicalKeyValueHelperNamespaceLocal() {
  const StdlibSurfaceMetadata *metadata =
      collectionAccessKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return "";
  }
  std::string namespacePath(metadata->canonicalPath);
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.erase(namespacePath.begin());
  }
  return namespacePath;
}

bool resolveCanonicalKeyValueHelperNameFromSpelling(
    std::string path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      collectionAccessKeyValueHelperMetadata();
  if (metadata == nullptr || path.empty()) {
    return false;
  }
  if (path.find('/') != std::string::npos) {
    if (path.front() != '/') {
      path.insert(path.begin(), '/');
    }
    const std::string canonicalPrefix =
        std::string(metadata->canonicalPath) + "/";
    if (path.rfind(canonicalPrefix, 0) != 0) {
      return false;
    }
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, path);
  if (memberName.empty()) {
    return false;
  }
  helperNameOut.assign(memberName);
  return true;
}

std::string canonicalStdlibKeyValueAccessPathForHelper(
    const std::string &helperName) {
  if (helperName == "at" || helperName == "at_ref" ||
      helperName == "at_unsafe" || helperName == "at_unsafe_ref") {
    return canonicalKeyValueHelperPathLocal(helperName);
  }
  return "";
}

bool isCanonicalKeyValueContainsResolvedPath(const std::string &path) {
  std::string helperName;
  return resolveCanonicalKeyValueHelperNameFromSpelling(path, helperName) &&
         (helperName == "contains" || helperName == "contains_ref");
}

bool isCanonicalKeyValueAccessResolvedPath(const std::string &path) {
  std::string helperName;
  return resolveCanonicalKeyValueHelperNameFromSpelling(path, helperName) &&
         isCanonicalKeyValueAccessHelperName(helperName);
}

std::string canonicalStdlibKeyValueContainsPathForResolvedMethod(
    const std::string &methodResolved) {
  std::string helperName;
  if (resolveCanonicalKeyValueHelperNameFromSpelling(methodResolved, helperName) &&
      (helperName == "contains" || helperName == "contains_ref")) {
    return canonicalKeyValueHelperPathLocal(helperName);
  }
  return canonicalKeyValueHelperPathLocal("contains");
}

} // namespace

bool SemanticsValidator::resolveExprCollectionAccessTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprCollectionAccessDispatchContext &context,
    bool &handledOut,
    std::string &resolved,
    bool &resolvedMethod,
    bool &usedMethodTarget,
    bool &hasMethodReceiverIndex,
    size_t &methodReceiverIndex) {
  handledOut = false;

  auto failCollectionAccessTargetDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto explicitCallPath = [](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        candidate.name.empty()) {
      return {};
    }
    if (candidate.name.front() == '/') {
      return candidate.name;
    }
    std::string prefix = candidate.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    if (prefix.empty()) {
      return "/" + candidate.name;
    }
    return prefix + "/" + candidate.name;
  };
  auto isLocalRootMapAliasReceiverCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    if (resolvedCandidate == "/map" ||
        resolvedCandidate.rfind("/map__", 0) == 0) {
      return true;
    }
    const std::string explicitCandidate = explicitCallPath(candidate);
    return explicitCandidate == "/map" ||
           explicitCandidate.rfind("/map__", 0) == 0;
  };

  std::string accessHelperName;
  auto getCanonicalMapAccessHelperNameForDispatch =
      [&](const Expr &candidate, std::string &helperNameOut) -> bool {
        helperNameOut.clear();
        if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
            candidate.args.size() != 2 || candidate.name.empty()) {
          return false;
        }
        if (getBuiltinArrayAccessName(candidate, helperNameOut)) {
          return true;
        }
        const std::string resolvedPath = resolveCalleePath(candidate);
        std::string resolvedKeyValueHelperName;
        if (resolveCanonicalKeyValueHelperNameFromSpelling(
                resolvedPath, resolvedKeyValueHelperName) &&
            (resolvedKeyValueHelperName == "at_ref" ||
             resolvedKeyValueHelperName == "at_unsafe_ref")) {
          helperNameOut = resolvedKeyValueHelperName;
          return true;
        }
        std::string normalizedName = candidate.name;
        if (!normalizedName.empty() && normalizedName.front() == '/') {
          normalizedName.erase(normalizedName.begin());
        }
        if (resolveCanonicalKeyValueHelperNameFromSpelling(
                normalizedName, resolvedKeyValueHelperName) &&
            isCanonicalKeyValueAccessHelperName(resolvedKeyValueHelperName)) {
          helperNameOut = resolvedKeyValueHelperName;
          return true;
        }
        std::string namespacePrefix = candidate.namespacePrefix;
        if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
          namespacePrefix.erase(namespacePrefix.begin());
        }
        if (!namespacePrefix.empty()) {
          if (namespacePrefix == canonicalKeyValueHelperNamespaceLocal() &&
              isCanonicalKeyValueAccessHelperName(candidate.name)) {
            helperNameOut = candidate.name;
            return true;
          }
          return false;
        }
        if (candidate.name.find('/') == std::string::npos &&
            isCanonicalKeyValueAccessHelperName(candidate.name)) {
          helperNameOut = candidate.name;
          return true;
        }
        return false;
      };
  const bool hasBuiltinAccessSpelling =
      !expr.isMethodCall &&
      getCanonicalMapAccessHelperNameForDispatch(expr, accessHelperName);
  const bool isStdNamespacedVectorAccessCall =
      hasBuiltinAccessSpelling && !expr.isMethodCall &&
      isValueSurfaceAccessHelperName(accessHelperName) &&
      isStdNamespacedVectorCompatibilityHelperPath(resolveCalleePath(expr),
                                                   accessHelperName);
  const bool isBuiltinAccessName =
      hasBuiltinAccessSpelling &&
      !isStdNamespacedVectorAccessCall;
  const bool isNamespacedVectorAccessCall =
      !isStdNamespacedVectorAccessCall && isBuiltinAccessName &&
      context.isNamespacedVectorHelperCall &&
      isValueSurfaceAccessHelperName(context.namespacedHelper) &&
      defMap_.find(resolved) == defMap_.end();
  const bool isNamespacedMapAccessCall =
      isBuiltinAccessName && context.isNamespacedMapHelperCall &&
      isCanonicalKeyValueAccessHelperName(context.namespacedHelper) &&
      defMap_.find(resolved) == defMap_.end();
  const std::string explicitRemovedMethodPath =
      explicitRemovedCollectionMethodPath(expr.name, expr.namespacePrefix);
  const bool preservesExplicitRemovedArrayAccessMethod =
      explicitRemovedMethodPath.rfind("/array/", 0) == 0 &&
      hasDefinitionPath(explicitRemovedMethodPath);
  auto resolveDirectSoaReceiver = [&](const Expr &target,
                                      std::string &elemTypeOut) -> bool {
    return this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
        target, params, locals, context.resolveSoaVectorTarget, elemTypeOut);
  };
  std::string explicitCanonicalMapAccessHelperName;
  const bool explicitCanonicalMapAccessCall =
      resolveCanonicalKeyValueHelperNameFromSpelling(
          explicitCallPath(expr), explicitCanonicalMapAccessHelperName) &&
      isCanonicalKeyValueAccessHelperName(explicitCanonicalMapAccessHelperName);

  if ((hasBuiltinAccessSpelling ||
       isCanonicalKeyValueAccessHelperName(explicitCanonicalMapAccessHelperName)) &&
      !expr.isMethodCall &&
      explicitCanonicalMapAccessCall && expr.args.size() == 2 &&
      isCanonicalKeyValueAccessHelperName(hasBuiltinAccessSpelling
                                         ? accessHelperName
                                         : explicitCanonicalMapAccessHelperName)) {
    const std::string helperPath = canonicalStdlibKeyValueAccessPathForHelper(
        hasBuiltinAccessSpelling ? accessHelperName
                                 : explicitCanonicalMapAccessHelperName);
    auto helperIt = defMap_.find(helperPath);
    if ((helperIt != defMap_.end() && helperIt->second != nullptr) ||
        hasDefinitionFamilyPath(helperPath) ||
        hasDeclaredDefinitionPath(helperPath) ||
        hasImportedDefinitionPath(helperPath)) {
      return true;
    }
    handledOut = true;
    usedMethodTarget = true;
    hasMethodReceiverIndex = true;
    methodReceiverIndex = 0;
    resolved = helperPath;
    resolvedMethod = helperIt == defMap_.end() || helperIt->second == nullptr;
    return true;
  }

  if (isBuiltinAccessName &&
      !(isStdNamespacedVectorAccessCall && hasNamedArguments(expr.argNames)) &&
      (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorAccessCall ||
       isNamespacedMapAccessCall)) {
    handledOut = true;
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    const bool usesBuiltinVectorSurfaceMethodSemantics =
        accessHelperName == "count" || accessHelperName == "capacity" ||
        accessHelperName == "at" || accessHelperName == "at_unsafe";
    auto hasVisiblePreferredVectorAccessHelper = [&]() {
      if (!isValueSurfaceAccessHelperName(accessHelperName)) {
        return false;
      }
      const std::string preferredVectorAccessTarget =
          preferredBareVectorHelperTarget(accessHelperName);
      return isRootedVectorHelperPath(preferredVectorAccessTarget) &&
             (hasDeclaredDefinitionPath(preferredVectorAccessTarget) ||
              hasImportedDefinitionPath(preferredVectorAccessTarget));
    };
    auto canonicalVectorAccessHelperTarget = [&]() {
      return canonicalVectorCompatibilityHelperPathOrFallback(
          accessHelperName);
    };
    auto canonicalMapAccessHelperTarget = [&]() {
      return canonicalStdlibKeyValueAccessPathForHelper(accessHelperName);
    };
    auto hasVisibleCanonicalVectorAccessHelper = [&]() {
      const std::string canonicalTarget = canonicalVectorAccessHelperTarget();
      const bool isStdlibVectorAccessWrapperDefinition =
          currentValidationState_.context.definitionPath.rfind("/std/collections/", 0) == 0 ||
          currentValidationState_.context.definitionPath.rfind("/std/image/", 0) == 0 ||
          currentValidationState_.context.definitionPath.rfind("/std/ui/", 0) == 0;
      return hasDeclaredDefinitionPath(canonicalTarget) ||
             hasImportedDefinitionPath(canonicalTarget) ||
             isStdlibVectorAccessWrapperDefinition;
    };
    auto isCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
      std::string elemType;
      return context.resolveVectorTarget(candidate, elemType) ||
             context.resolveArrayTarget(candidate, elemType) ||
             context.resolveStringTarget(candidate) ||
             context.resolveMapTarget(candidate);
    };
    const bool probePositionalReorderedReceiver =
        !hasNamedArgs && expr.args.size() > 1 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name &&
          !isCollectionAccessReceiverExpr(expr.args.front())));
    const bool hasAlternativeCollectionReceiver =
        probePositionalReorderedReceiver &&
        std::any_of(expr.args.begin() + 1, expr.args.end(), [&](const Expr &candidate) {
          std::string elemType;
          return context.resolveVectorTarget(candidate, elemType) ||
                 context.resolveArrayTarget(candidate, elemType) ||
                 context.resolveStringTarget(candidate) ||
                 context.resolveMapTarget(candidate);
        });
    bool failedReceiverProbe = false;
    auto tryResolveReceiverIndex = [&](size_t receiverIndex,
                                       bool skipResolvedResult) -> bool {
      if (receiverIndex >= expr.args.size()) {
        return false;
      }
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string elemType;
      if (!(context.resolveVectorTarget(receiverCandidate, elemType) ||
            context.resolveArrayTarget(receiverCandidate, elemType) ||
            context.resolveStringTarget(receiverCandidate) ||
            context.resolveMapTarget(receiverCandidate))) {
        return false;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      bool resolvedVectorAccessMethod = false;
      bool resolvedCanonicalMapAccessMethod = false;
      std::string ignoredVectorElemType;
      const bool receiverSupportsBuiltinVectorSurfaceSemantics =
          usesBuiltinVectorSurfaceMethodSemantics &&
          (context.resolveVectorTarget(receiverCandidate, ignoredVectorElemType) ||
           context.resolveSoaVectorTarget(receiverCandidate, ignoredVectorElemType));
      const bool receiverIsBuiltinVectorAccessTarget =
          isValueSurfaceAccessHelperName(accessHelperName) &&
          context.resolveVectorTarget(receiverCandidate, ignoredVectorElemType);
      const bool receiverHasVisibleCanonicalVectorAccessHelper =
          receiverIsBuiltinVectorAccessTarget &&
          hasVisibleCanonicalVectorAccessHelper();
      if (receiverIsBuiltinVectorAccessTarget) {
        methodResolved = canonicalVectorAccessHelperTarget();
        if (receiverHasVisibleCanonicalVectorAccessHelper) {
          resolvedVectorAccessMethod = true;
        } else if (!hasVisiblePreferredVectorAccessHelper()) {
          (void)failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
          failedReceiverProbe = true;
          return true;
        }
      }
      if (explicitCanonicalMapAccessCall &&
          isCanonicalKeyValueAccessHelperName(accessHelperName) &&
          context.resolveMapTarget(receiverCandidate)) {
        methodResolved = canonicalMapAccessHelperTarget();
        isBuiltinMethod = true;
        resolvedCanonicalMapAccessMethod = true;
        if (!hasDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          (void)failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
          failedReceiverProbe = true;
          return true;
        }
      }
      if (!preservesExplicitRemovedArrayAccessMethod &&
          isValueSurfaceAccessHelperName(accessHelperName) &&
          (!receiverIsBuiltinVectorAccessTarget ||
           !receiverHasVisibleCanonicalVectorAccessHelper) &&
          (!receiverSupportsBuiltinVectorSurfaceSemantics ||
           hasVisiblePreferredVectorAccessHelper() ||
           (receiverIsBuiltinVectorAccessTarget &&
            !receiverHasVisibleCanonicalVectorAccessHelper)) &&
          resolveVectorHelperMethodTarget(params, locals, receiverCandidate,
                                          accessHelperName, methodResolved)) {
        if (isLegacyExperimentalVectorCompatibilityPath(methodResolved) &&
            !hasDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          const std::string canonicalVectorMethodTarget =
              canonicalVectorAccessHelperTarget();
          if (hasDefinitionFamilyPath(canonicalVectorMethodTarget) ||
              hasDefinitionPath(canonicalVectorMethodTarget) ||
              hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
              hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
            methodResolved = canonicalVectorMethodTarget;
          }
        }
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (hasDefinitionPath(methodResolved) ||
            hasDeclaredDefinitionPath(methodResolved) ||
            hasImportedDefinitionPath(methodResolved)) {
          isBuiltinMethod = false;
          resolvedVectorAccessMethod = true;
        }
      }
      if (!resolvedVectorAccessMethod && !resolvedCanonicalMapAccessMethod &&
          !resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, accessHelperName,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        failedReceiverProbe = true;
        return true;
      }
      if (!isBuiltinMethod &&
          isLegacyExperimentalVectorCompatibilityTypePath(methodResolved) &&
          !hasDefinitionPath(methodResolved) &&
          !hasDeclaredDefinitionPath(methodResolved) &&
          !hasImportedDefinitionPath(methodResolved)) {
        const std::string canonicalVectorMethodTarget =
            canonicalVectorAccessHelperTarget();
        if (hasDefinitionFamilyPath(canonicalVectorMethodTarget) ||
            hasDefinitionPath(canonicalVectorMethodTarget) ||
            hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
            hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
          methodResolved = canonicalVectorMethodTarget;
        }
      }
      if (!isBuiltinMethod && !resolvedVectorAccessMethod &&
          !hasDefinitionPath(methodResolved) &&
          !hasDeclaredDefinitionPath(methodResolved) &&
          !hasImportedDefinitionPath(methodResolved)) {
        (void)failCollectionAccessTargetDiagnostic("unknown method: " + methodResolved);
        failedReceiverProbe = true;
        return true;
      }
      if (skipResolvedResult) {
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = receiverIndex;
      return true;
    };
    bool resolvedReceiver = false;
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          hasValuesNamedReceiver = true;
          if (tryResolveReceiverIndex(i, false)) {
            resolvedReceiver = true;
            break;
          }
        }
      }
      if (!resolvedReceiver && !hasValuesNamedReceiver) {
        resolvedReceiver = tryResolveReceiverIndex(0, false);
      }
      if (!resolvedReceiver && !hasValuesNamedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (tryResolveReceiverIndex(i, false)) {
            resolvedReceiver = true;
            break;
          }
        }
      }
    } else {
      resolvedReceiver = tryResolveReceiverIndex(0, hasAlternativeCollectionReceiver);
    }
    if (!resolvedReceiver && probePositionalReorderedReceiver) {
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (tryResolveReceiverIndex(i, false)) {
          break;
        }
      }
    }
    if (failedReceiverProbe) {
      return false;
    }
    if (!hasMethodReceiverIndex && !expr.args.empty() &&
        (expr.args.front().kind == Expr::Kind::Name || expr.args.front().kind == Expr::Kind::Call)) {
      bool isBuiltinMethod = false;
      std::string methodResolved;
      bool resolvedVectorAccessMethod = false;
      bool resolvedCanonicalMapAccessMethod = false;
      std::string ignoredVectorElemType;
      const bool receiverSupportsBuiltinVectorSurfaceSemantics =
          usesBuiltinVectorSurfaceMethodSemantics &&
          (context.resolveVectorTarget(expr.args.front(), ignoredVectorElemType) ||
           context.resolveSoaVectorTarget(expr.args.front(), ignoredVectorElemType));
      const bool receiverIsBuiltinVectorAccessTarget =
          isValueSurfaceAccessHelperName(accessHelperName) &&
          context.resolveVectorTarget(expr.args.front(), ignoredVectorElemType);
      const bool receiverHasVisibleCanonicalVectorAccessHelper =
          receiverIsBuiltinVectorAccessTarget &&
          hasVisibleCanonicalVectorAccessHelper();
      if (receiverIsBuiltinVectorAccessTarget) {
        methodResolved = canonicalVectorAccessHelperTarget();
        if (receiverHasVisibleCanonicalVectorAccessHelper) {
          resolvedVectorAccessMethod = true;
        } else if (!hasVisiblePreferredVectorAccessHelper()) {
          return failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
        }
      }
      if (explicitCanonicalMapAccessCall &&
          isCanonicalKeyValueAccessHelperName(accessHelperName) &&
          context.resolveMapTarget(expr.args.front())) {
        methodResolved = canonicalMapAccessHelperTarget();
        isBuiltinMethod = true;
        resolvedCanonicalMapAccessMethod = true;
        if (!hasDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          return failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
        }
      }
      if (!preservesExplicitRemovedArrayAccessMethod &&
          isValueSurfaceAccessHelperName(accessHelperName) &&
          (!receiverIsBuiltinVectorAccessTarget ||
           !receiverHasVisibleCanonicalVectorAccessHelper) &&
          (!receiverSupportsBuiltinVectorSurfaceSemantics ||
           hasVisiblePreferredVectorAccessHelper() ||
           (receiverIsBuiltinVectorAccessTarget &&
            !receiverHasVisibleCanonicalVectorAccessHelper)) &&
          resolveVectorHelperMethodTarget(params, locals, expr.args.front(),
                                          accessHelperName, methodResolved)) {
        if (isLegacyExperimentalVectorCompatibilityPath(methodResolved) &&
            !hasDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          const std::string canonicalVectorMethodTarget =
              canonicalVectorAccessHelperTarget();
          if (hasDefinitionFamilyPath(canonicalVectorMethodTarget) ||
              hasDefinitionPath(canonicalVectorMethodTarget) ||
              hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
              hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
            methodResolved = canonicalVectorMethodTarget;
          }
        }
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (hasDefinitionPath(methodResolved) ||
            hasDeclaredDefinitionPath(methodResolved) ||
            hasImportedDefinitionPath(methodResolved)) {
          isBuiltinMethod = false;
          resolvedVectorAccessMethod = true;
        }
      }
      if (resolvedVectorAccessMethod ||
          resolvedCanonicalMapAccessMethod ||
          resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), accessHelperName,
                              methodResolved, isBuiltinMethod)) {
        if (!isBuiltinMethod &&
            isLegacyExperimentalVectorCompatibilityTypePath(methodResolved) &&
            !hasDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          const std::string canonicalVectorMethodTarget =
              canonicalVectorAccessHelperTarget();
          if (hasDefinitionFamilyPath(canonicalVectorMethodTarget) ||
              hasDefinitionPath(canonicalVectorMethodTarget) ||
              hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
              hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
            methodResolved = canonicalVectorMethodTarget;
          }
        }
        if (isBuiltinMethod) {
          usedMethodTarget = true;
          hasMethodReceiverIndex = true;
          methodReceiverIndex = 0;
          resolved = methodResolved;
          resolvedMethod = true;
        } else {
          const size_t methodSlash = methodResolved.find_last_of('/');
          const bool hasStructReceiver =
              methodSlash != std::string::npos && methodSlash > 0 &&
              structNames_.count(methodResolved.substr(0, methodSlash)) > 0;
          if (hasStructReceiver) {
            usedMethodTarget = true;
            hasMethodReceiverIndex = true;
            methodReceiverIndex = 0;
            if (!resolvedVectorAccessMethod &&
                !hasDefinitionPath(methodResolved) &&
                !hasDeclaredDefinitionPath(methodResolved) &&
                !hasImportedDefinitionPath(methodResolved)) {
              return failCollectionAccessTargetDiagnostic("unknown method: " + methodResolved);
            }
            resolved = methodResolved;
            resolvedMethod = false;
          }
        }
      } else {
        std::string receiverStructPath;
        if (expr.args.front().kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, expr.args.front().name)) {
            receiverStructPath = resolveTypePath(paramBinding->typeName, expr.args.front().namespacePrefix);
          } else if (auto it = locals.find(expr.args.front().name); it != locals.end()) {
            receiverStructPath = resolveTypePath(it->second.typeName, expr.args.front().namespacePrefix);
          }
        } else if (expr.args.front().kind == Expr::Kind::Call) {
          receiverStructPath = inferStructReturnPath(expr.args.front(), params, locals);
          if (receiverStructPath.empty()) {
            auto defIt = defMap_.find(resolveCalleePath(expr.args.front()));
            if (defIt != defMap_.end() && defIt->second != nullptr) {
              BindingInfo inferredReturn;
              if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
                receiverStructPath = resolveTypePath(inferredReturn.typeName, expr.args.front().namespacePrefix);
              }
            }
          }
          if (receiverStructPath.empty()) {
            const std::string resolvedReceiver = resolveCalleePath(expr.args.front());
            if (structNames_.count(resolvedReceiver) > 0) {
              receiverStructPath = resolvedReceiver;
            }
          }
        }
        if (!receiverStructPath.empty() && receiverStructPath.front() != '/') {
          receiverStructPath.insert(receiverStructPath.begin(), '/');
        }
        if (!receiverStructPath.empty() && structNames_.count(receiverStructPath) > 0) {
          if (isSoaAccessHelperName(accessHelperName) &&
              isSoaReceiverStructPath(receiverStructPath)) {
            usedMethodTarget = true;
            hasMethodReceiverIndex = true;
            methodReceiverIndex = 0;
            resolved =
                preferredSoaHelperTargetForCollectionType(accessHelperName,
                                                          "/soa" "_vector");
            resolvedMethod = true;
            return true;
          }
          if (isLegacyExperimentalVectorCompatibilityTypePath(
                  receiverStructPath)) {
            const std::string canonicalVectorMethodTarget =
                canonicalVectorAccessHelperTarget();
            if (hasDefinitionFamilyPath(canonicalVectorMethodTarget) ||
                hasDefinitionPath(canonicalVectorMethodTarget) ||
                hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
                hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
              usedMethodTarget = true;
              hasMethodReceiverIndex = true;
              methodReceiverIndex = 0;
              resolved = canonicalVectorMethodTarget;
              resolvedMethod = false;
              return true;
            }
          }
          return failCollectionAccessTargetDiagnostic("unknown method: " + receiverStructPath + "/" +
                                                      accessHelperName);
        }
      }
    }
    return true;
  }

  if ((isSimpleCallName(expr, "get") || isSimpleCallName(expr, "get_ref") ||
       isSimpleCallName(expr, "ref") || isSimpleCallName(expr, "ref_ref")) &&
      expr.args.size() == 2 && defMap_.find(resolved) == defMap_.end()) {
    handledOut = true;
    bool failedReceiverProbe = false;
    auto tryResolveReceiverIndex = [&](size_t receiverIndex) {
      if (receiverIndex >= expr.args.size()) {
        return false;
      }
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, expr.name, methodTarget)) {
        methodTarget = preferVectorStdlibHelperPath(methodTarget);
        if (hasDeclaredDefinitionPath(methodTarget) || hasImportedDefinitionPath(methodTarget)) {
          usedMethodTarget = true;
          resolved = methodTarget;
          resolvedMethod = false;
          hasMethodReceiverIndex = true;
          methodReceiverIndex = receiverIndex;
          return true;
        }
      }
      std::string elemType;
      if (!resolveDirectSoaReceiver(receiverCandidate, elemType)) {
        const std::string canonicalGetPath =
            canonicalizeLegacySoaGetHelperPath(expr.name);
        if (receiverCandidate.kind == Expr::Kind::Call &&
            (isLegacyOrCanonicalSoaHelperPath(canonicalGetPath, "get") ||
             isLegacyOrCanonicalSoaHelperPath(canonicalGetPath, "get_ref"))) {
          usedMethodTarget = true;
          resolved = canonicalGetPath;
          resolvedMethod = false;
          hasMethodReceiverIndex = true;
          methodReceiverIndex = receiverIndex;
          return true;
        }
        return false;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        failedReceiverProbe = true;
        return true;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        const std::string canonicalGetPath =
            canonicalizeLegacySoaGetHelperPath(methodResolved);
        const std::string canonicalRefPath =
            canonicalizeLegacySoaRefHelperPath(methodResolved);
        const bool isCanonicalSoaAccessHelper =
            isLegacyOrCanonicalSoaHelperPath(canonicalGetPath, "get") ||
            isLegacyOrCanonicalSoaHelperPath(canonicalGetPath, "get_ref") ||
            isCanonicalSoaRefLikeHelperPath(canonicalRefPath);
        if (isCanonicalSoaAccessHelper) {
          resolved = methodResolved;
          resolvedMethod = false;
          hasMethodReceiverIndex = true;
          methodReceiverIndex = receiverIndex;
          return true;
        }
        (void)failCollectionAccessTargetDiagnostic("unknown method: " + methodResolved);
        failedReceiverProbe = true;
        return true;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = receiverIndex;
      return true;
    };

    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    bool resolvedReceiver = false;
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          hasValuesNamedReceiver = true;
          if (tryResolveReceiverIndex(i)) {
            resolvedReceiver = true;
            break;
          }
        }
      }
      if (!resolvedReceiver && !hasValuesNamedReceiver) {
        resolvedReceiver = tryResolveReceiverIndex(0);
      }
      if (!resolvedReceiver && !hasValuesNamedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (tryResolveReceiverIndex(i)) {
            break;
          }
        }
      }
    } else {
      (void)tryResolveReceiverIndex(0);
    }

    if (failedReceiverProbe) {
      return false;
    }
    return true;
  }

  if (!expr.isMethodCall && !expr.args.empty() &&
      defMap_.find(resolved) == defMap_.end() &&
      !isSimpleCallName(expr, "to_soa") && !isSimpleCallName(expr, "to" "_aos") &&
      !isSimpleCallName(expr, "to" "_aos_ref") &&
      !isSimpleCallName(expr, "contains") &&
      !getBuiltinArrayAccessName(expr, accessHelperName)) {
    handledOut = true;
    const Expr &receiverCandidate = expr.args.front();
    std::string elemType;
    if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiverCandidate,
            params,
            locals,
            resolveDirectSoaReceiver,
            elemType)) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        return failCollectionAccessTargetDiagnostic("unknown method: " + methodResolved);
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
    }
    return true;
  }

  if (!expr.isMethodCall && expr.args.size() == 2 &&
      defMap_.find(resolved) == defMap_.end() &&
      (isSimpleCallName(expr, "contains") ||
       getCanonicalMapAccessHelperNameForDispatch(expr, accessHelperName))) {
    handledOut = true;
    const Expr &receiverCandidate = expr.args.front();
    if (context.resolveMapTarget(receiverCandidate) &&
        !isLocalRootMapAliasReceiverCall(receiverCandidate)) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      std::string canonicalKeyValueMethodHelperName;
      const bool resolvedCanonicalKeyValueHelper =
          resolveCanonicalKeyValueHelperNameFromSpelling(
              methodResolved, canonicalKeyValueMethodHelperName);
      if (expr.name == "contains" &&
          resolvedCanonicalKeyValueHelper &&
          (canonicalKeyValueMethodHelperName == "contains" ||
           canonicalKeyValueMethodHelperName == "contains_ref") &&
          !hasImportedDefinitionPath("/contains") &&
          !hasDeclaredDefinitionPath("/contains") &&
          !hasImportedDefinitionPath(
              canonicalStdlibKeyValueContainsPathForResolvedMethod(methodResolved)) &&
          !hasDeclaredDefinitionPath(
              canonicalStdlibKeyValueContainsPathForResolvedMethod(methodResolved))) {
        return failCollectionAccessTargetDiagnostic(
            "unknown call target: " +
            canonicalStdlibKeyValueContainsPathForResolvedMethod(methodResolved));
      }
      if (isBuiltinMethod) {
        const bool resolvedDeclaredCanonicalMapHelper =
            resolvedCanonicalKeyValueHelper &&
            hasDeclaredDefinitionPath(
                canonicalKeyValueHelperPathLocal(canonicalKeyValueMethodHelperName));
        const bool resolvedCanonicalContainsHelper =
            resolvedCanonicalKeyValueHelper &&
            (canonicalKeyValueMethodHelperName == "contains" ||
             canonicalKeyValueMethodHelperName == "contains_ref");
        const bool resolvedCanonicalAccessHelper =
            resolvedCanonicalKeyValueHelper &&
            isCanonicalKeyValueAccessHelperName(canonicalKeyValueMethodHelperName);
        if (resolvedDeclaredCanonicalMapHelper &&
            !(resolvedCanonicalContainsHelper &&
              context.shouldBuiltinValidateBareMapContainsCall) &&
            !(resolvedCanonicalAccessHelper &&
              context.shouldBuiltinValidateBareMapAccessCall) &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate)) {
          isBuiltinMethod = false;
        }
      }
      if (isBuiltinMethod) {
        if (isCanonicalKeyValueContainsResolvedPath(methodResolved) &&
            !context.shouldBuiltinValidateBareMapContainsCall &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate) &&
            !hasImportedDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved)) {
          return failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
        }
        if (isCanonicalKeyValueAccessResolvedPath(methodResolved) &&
            !context.shouldBuiltinValidateBareMapAccessCall &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate) &&
            !hasImportedDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved)) {
          return failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
        }
      } else if (defMap_.find(methodResolved) == defMap_.end() &&
                 !context.hasResolvableMapHelperPath(methodResolved)) {
        return failCollectionAccessTargetDiagnostic("unknown method: " + methodResolved);
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics

#include "SemanticsValidator.h"

#include <algorithm>
#include <string>
#include <vector>

namespace primec::semantics {
namespace {

void appendUniqueReceiverIndex(std::vector<size_t> &receiverIndices, size_t index, size_t limit) {
  if (index >= limit) {
    return;
  }
  for (size_t existing : receiverIndices) {
    if (existing == index) {
      return;
    }
  }
  receiverIndices.push_back(index);
}

bool isValueSurfaceAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_unsafe";
}

bool isCanonicalMapAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

bool isStdNamespacedCanonicalMapAccessPath(const std::string &path) {
  return path == "/std/collections/map/at" ||
         path == "/std/collections/map/at_ref" ||
         path == "/std/collections/map/at_unsafe" ||
         path == "/std/collections/map/at_unsafe_ref";
}

bool isMapCanonicalAccessPath(const std::string &path) {
  return path == "/map/at" || path == "/map/at_ref" ||
         path == "/map/at_unsafe" || path == "/map/at_unsafe_ref";
}

std::string canonicalStdlibMapAccessPathForHelper(const std::string &helperName) {
  if (helperName == "at" || helperName == "at_ref" ||
      helperName == "at_unsafe" || helperName == "at_unsafe_ref") {
    return "/std/collections/map/" + helperName;
  }
  return "";
}

std::string canonicalStdlibMapContainsPathForResolvedMethod(const std::string &methodResolved) {
  return methodResolved == "/std/collections/map/contains_ref" ||
                 methodResolved == "/map/contains_ref"
             ? "/std/collections/map/contains_ref"
             : "/std/collections/map/contains";
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
        if (resolvedPath == "/map/at_ref" ||
            resolvedPath == "/std/collections/map/at_ref") {
          helperNameOut = "at_ref";
          return true;
        }
        if (resolvedPath == "/map/at_unsafe_ref" ||
            resolvedPath == "/std/collections/map/at_unsafe_ref") {
          helperNameOut = "at_unsafe_ref";
          return true;
        }
        std::string namespacePrefix = candidate.namespacePrefix;
        if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
          namespacePrefix.erase(namespacePrefix.begin());
        }
        if ((namespacePrefix == "map" ||
             namespacePrefix == "std/collections/map") &&
            isCanonicalMapAccessHelperName(candidate.name)) {
          helperNameOut = candidate.name;
          return true;
        }
        if (candidate.name.find('/') == std::string::npos &&
            isCanonicalMapAccessHelperName(candidate.name)) {
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
      resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
  const bool isStdNamespacedMapAccessCall =
      hasBuiltinAccessSpelling && !expr.isMethodCall &&
      isStdNamespacedCanonicalMapAccessPath(resolveCalleePath(expr));
  const bool prefersExplicitDirectMapAccessAliasDefinition =
      !expr.isMethodCall &&
      (((context.isNamespacedMapHelperCall &&
         isCanonicalMapAccessHelperName(context.namespacedHelper)) ||
        (((expr.namespacePrefix == "map") || (expr.namespacePrefix == "/map")) &&
         isCanonicalMapAccessHelperName(expr.name)))) &&
      defMap_.find("/map/" +
                   (isCanonicalMapAccessHelperName(expr.name) ? expr.name
                                                              : context.namespacedHelper)) !=
          defMap_.end();
  if (prefersExplicitDirectMapAccessAliasDefinition) {
    resolved = "/map/" + (isCanonicalMapAccessHelperName(expr.name) ? expr.name
                                                                    : context.namespacedHelper);
  }
  auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "map/at" && normalized != "map/at_ref" &&
        normalized != "map/at_unsafe" &&
        normalized != "map/at_unsafe_ref") {
      std::string namespacePrefix = candidate.namespacePrefix;
      if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
        namespacePrefix.erase(namespacePrefix.begin());
      }
      if (namespacePrefix == "map" &&
          isCanonicalMapAccessHelperName(normalized)) {
        normalized = "map/" + normalized;
      }
    }
    if (normalized != "map/at" && normalized != "map/at_ref" &&
        normalized != "map/at_unsafe" &&
        normalized != "map/at_unsafe_ref") {
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (resolvedPath == "/map/at" || resolvedPath == "/map/at_ref" ||
          resolvedPath == "/map/at_unsafe" ||
          resolvedPath == "/map/at_unsafe_ref") {
        normalized = resolvedPath.substr(1);
      } else {
        return false;
      }
    }
    return defMap_.find("/" + normalized) == defMap_.end();
  };
  const bool isResolvedMapAccessCall =
      !expr.isMethodCall && isMapCanonicalAccessPath(resolved) &&
      !isMapNamespacedAccessCompatibilityCall(expr);
  const bool isBuiltinAccessName =
      hasBuiltinAccessSpelling &&
      !isStdNamespacedVectorAccessCall &&
      !isStdNamespacedMapAccessCall && !isResolvedMapAccessCall;
  const bool isNamespacedVectorAccessCall =
      !isStdNamespacedVectorAccessCall && isBuiltinAccessName &&
      context.isNamespacedVectorHelperCall &&
      isValueSurfaceAccessHelperName(context.namespacedHelper) &&
      defMap_.find(resolved) == defMap_.end();
  const bool isNamespacedMapAccessCall =
      isBuiltinAccessName && context.isNamespacedMapHelperCall &&
      isCanonicalMapAccessHelperName(context.namespacedHelper) &&
      !prefersExplicitDirectMapAccessAliasDefinition &&
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

  if (isBuiltinAccessName &&
      !(isStdNamespacedVectorAccessCall && hasNamedArguments(expr.argNames)) &&
      (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorAccessCall ||
       isNamespacedMapAccessCall)) {
    handledOut = true;
    std::vector<size_t> receiverIndices;
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
        }
      }
    } else {
      appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
    }

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
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
      }
    }
    const bool hasAlternativeCollectionReceiver =
        probePositionalReorderedReceiver &&
        std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
          if (index == 0 || index >= expr.args.size()) {
            return false;
          }
          const Expr &candidate = expr.args[index];
          std::string elemType;
          return context.resolveVectorTarget(candidate, elemType) ||
                 context.resolveArrayTarget(candidate, elemType) ||
                 context.resolveStringTarget(candidate) ||
                 context.resolveMapTarget(candidate);
        });
    for (size_t receiverIndex : receiverIndices) {
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string elemType;
      if (!(context.resolveVectorTarget(receiverCandidate, elemType) ||
            context.resolveArrayTarget(receiverCandidate, elemType) ||
            context.resolveStringTarget(receiverCandidate) ||
            context.resolveMapTarget(receiverCandidate))) {
        continue;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      bool resolvedVectorAccessMethod = false;
      if (!preservesExplicitRemovedArrayAccessMethod &&
          isValueSurfaceAccessHelperName(accessHelperName) &&
          resolveVectorHelperMethodTarget(params, locals, receiverCandidate,
                                          accessHelperName, methodResolved)) {
        if (methodResolved.rfind("/std/collections/experimental_vector/", 0) == 0 &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          const std::string canonicalVectorMethodTarget =
              "/std/collections/vector/" + accessHelperName;
          if (hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
              hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
            methodResolved = canonicalVectorMethodTarget;
          } else {
            const std::string aliasVectorMethodTarget =
                "/vector/" + accessHelperName;
            if (hasDeclaredDefinitionPath(aliasVectorMethodTarget) ||
                hasImportedDefinitionPath(aliasVectorMethodTarget)) {
              methodResolved = aliasVectorMethodTarget;
            }
          }
        }
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (hasDeclaredDefinitionPath(methodResolved) ||
            hasImportedDefinitionPath(methodResolved)) {
          isBuiltinMethod = false;
          resolvedVectorAccessMethod = true;
        }
      }
      if (!resolvedVectorAccessMethod &&
          !resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, accessHelperName,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (!isBuiltinMethod &&
          methodResolved.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 &&
          !hasDeclaredDefinitionPath(methodResolved) &&
          !hasImportedDefinitionPath(methodResolved)) {
        const std::string canonicalVectorMethodTarget =
            "/std/collections/vector/" + accessHelperName;
        if (hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
            hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
          methodResolved = canonicalVectorMethodTarget;
        } else {
          const std::string aliasVectorMethodTarget = "/vector/" + accessHelperName;
          if (hasDeclaredDefinitionPath(aliasVectorMethodTarget) ||
              hasImportedDefinitionPath(aliasVectorMethodTarget)) {
            methodResolved = aliasVectorMethodTarget;
          }
        }
      }
      if (isCanonicalMapAccessHelperName(accessHelperName) &&
          context.resolveMapTarget(receiverCandidate) &&
          defMap_.find("/map/" + accessHelperName) != defMap_.end()) {
        methodResolved = "/map/" + accessHelperName;
        isBuiltinMethod = false;
      }
      if (!isBuiltinMethod &&
          !hasDeclaredDefinitionPath(methodResolved) &&
          !hasImportedDefinitionPath(methodResolved)) {
        return failCollectionAccessTargetDiagnostic("unknown method: " + methodResolved);
      }
      if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
        continue;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = receiverIndex;
      break;
    }
    if (!hasMethodReceiverIndex && !expr.args.empty() &&
        (expr.args.front().kind == Expr::Kind::Name || expr.args.front().kind == Expr::Kind::Call)) {
      bool isBuiltinMethod = false;
      std::string methodResolved;
      bool resolvedVectorAccessMethod = false;
      if (!preservesExplicitRemovedArrayAccessMethod &&
          isValueSurfaceAccessHelperName(accessHelperName) &&
          resolveVectorHelperMethodTarget(params, locals, expr.args.front(),
                                          accessHelperName, methodResolved)) {
        if (methodResolved.rfind("/std/collections/experimental_vector/", 0) == 0 &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          const std::string canonicalVectorMethodTarget =
              "/std/collections/vector/" + accessHelperName;
          if (hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
              hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
            methodResolved = canonicalVectorMethodTarget;
          } else {
            const std::string aliasVectorMethodTarget =
                "/vector/" + accessHelperName;
            if (hasDeclaredDefinitionPath(aliasVectorMethodTarget) ||
                hasImportedDefinitionPath(aliasVectorMethodTarget)) {
              methodResolved = aliasVectorMethodTarget;
            }
          }
        }
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (hasDeclaredDefinitionPath(methodResolved) ||
            hasImportedDefinitionPath(methodResolved)) {
          isBuiltinMethod = false;
          resolvedVectorAccessMethod = true;
        }
      }
      if (resolvedVectorAccessMethod ||
          resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), accessHelperName,
                              methodResolved, isBuiltinMethod)) {
        if (!isBuiltinMethod &&
            methodResolved.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !hasImportedDefinitionPath(methodResolved)) {
          const std::string canonicalVectorMethodTarget =
              "/std/collections/vector/" + accessHelperName;
          if (hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
              hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
            methodResolved = canonicalVectorMethodTarget;
          } else {
            const std::string aliasVectorMethodTarget = "/vector/" + accessHelperName;
            if (hasDeclaredDefinitionPath(aliasVectorMethodTarget) ||
                hasImportedDefinitionPath(aliasVectorMethodTarget)) {
              methodResolved = aliasVectorMethodTarget;
            }
          }
        }
        if (isCanonicalMapAccessHelperName(accessHelperName) &&
            context.resolveMapTarget(expr.args.front()) &&
            defMap_.find("/map/" + accessHelperName) != defMap_.end()) {
          methodResolved = "/map/" + accessHelperName;
          isBuiltinMethod = false;
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
            if (!hasDeclaredDefinitionPath(methodResolved) &&
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
          if (receiverStructPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
            const std::string canonicalVectorMethodTarget =
                "/std/collections/vector/" + accessHelperName;
            const std::string aliasVectorMethodTarget =
                "/vector/" + accessHelperName;
            if (hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
                hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
              usedMethodTarget = true;
              hasMethodReceiverIndex = true;
              methodReceiverIndex = 0;
              resolved = canonicalVectorMethodTarget;
              resolvedMethod = false;
              return true;
            }
            if (hasDeclaredDefinitionPath(aliasVectorMethodTarget) ||
                hasImportedDefinitionPath(aliasVectorMethodTarget)) {
              usedMethodTarget = true;
              hasMethodReceiverIndex = true;
              methodReceiverIndex = 0;
              resolved = aliasVectorMethodTarget;
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
    std::vector<size_t> receiverIndices;
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
        }
      }
    } else {
      appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
    }

    for (size_t receiverIndex : receiverIndices) {
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
          break;
        }
      }
      std::string elemType;
      if (!context.resolveSoaVectorTarget(receiverCandidate, elemType)) {
        continue;
      }
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
      methodReceiverIndex = receiverIndex;
      break;
    }
    return true;
  }

  if (!expr.isMethodCall && !expr.args.empty() &&
      defMap_.find(resolved) == defMap_.end() &&
      !isSimpleCallName(expr, "to_soa") && !isSimpleCallName(expr, "to_aos") &&
      !isSimpleCallName(expr, "to_aos_ref") &&
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
    if (context.resolveMapTarget(receiverCandidate)) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (expr.name == "contains" &&
          (methodResolved == "/map/contains" ||
           methodResolved == "/map/contains_ref" ||
           methodResolved == "/std/collections/map/contains" ||
           methodResolved == "/std/collections/map/contains_ref") &&
          !hasImportedDefinitionPath("/contains") &&
          !hasDeclaredDefinitionPath("/contains") &&
          !hasImportedDefinitionPath(
              canonicalStdlibMapContainsPathForResolvedMethod(methodResolved)) &&
          !hasDeclaredDefinitionPath(
              canonicalStdlibMapContainsPathForResolvedMethod(methodResolved))) {
        return failCollectionAccessTargetDiagnostic(
            "unknown call target: " +
            canonicalStdlibMapContainsPathForResolvedMethod(methodResolved));
      }
      if (isBuiltinMethod) {
        if (((methodResolved == "/std/collections/map/contains" &&
              hasDeclaredDefinitionPath("/std/collections/map/contains")) ||
             (methodResolved == "/std/collections/map/contains_ref" &&
              hasDeclaredDefinitionPath("/std/collections/map/contains_ref")) ||
             (methodResolved == "/std/collections/map/at" &&
              hasDeclaredDefinitionPath("/std/collections/map/at")) ||
             (methodResolved == "/std/collections/map/at_ref" &&
              hasDeclaredDefinitionPath("/std/collections/map/at_ref")) ||
             (methodResolved == "/std/collections/map/at_unsafe" &&
              hasDeclaredDefinitionPath("/std/collections/map/at_unsafe")) ||
             (methodResolved == "/std/collections/map/at_unsafe_ref" &&
              hasDeclaredDefinitionPath("/std/collections/map/at_unsafe_ref"))) &&
            !((methodResolved == "/std/collections/map/contains" ||
               methodResolved == "/std/collections/map/contains_ref") &&
              context.shouldBuiltinValidateBareMapContainsCall) &&
            !((methodResolved == "/std/collections/map/at" ||
               methodResolved == "/std/collections/map/at_ref" ||
               methodResolved == "/std/collections/map/at_unsafe" ||
               methodResolved == "/std/collections/map/at_unsafe_ref") &&
              context.shouldBuiltinValidateBareMapAccessCall) &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate)) {
          isBuiltinMethod = false;
        }
      }
      if (isBuiltinMethod) {
        if ((methodResolved == "/std/collections/map/contains" ||
             methodResolved == "/std/collections/map/contains_ref") &&
            !context.shouldBuiltinValidateBareMapContainsCall &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate) &&
            !hasImportedDefinitionPath(methodResolved) &&
            !hasDeclaredDefinitionPath(methodResolved)) {
          return failCollectionAccessTargetDiagnostic(
              "unknown call target: " + methodResolved);
        }
        if ((methodResolved == "/map/at" || methodResolved == "/map/at_unsafe" ||
             methodResolved == "/map/at_ref" ||
             methodResolved == "/map/at_unsafe_ref" ||
             methodResolved == "/std/collections/map/at" ||
             methodResolved == "/std/collections/map/at_ref" ||
             methodResolved == "/std/collections/map/at_unsafe" ||
             methodResolved == "/std/collections/map/at_unsafe_ref") &&
            !context.shouldBuiltinValidateBareMapAccessCall &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate) &&
            !hasImportedDefinitionPath(methodResolved.rfind("/std/collections/map/", 0) == 0
                                           ? methodResolved
                                           : canonicalStdlibMapAccessPathForHelper(
                                                 methodResolved.substr(std::string("/map/").size()))) &&
            !hasDeclaredDefinitionPath(methodResolved.rfind("/std/collections/map/", 0) == 0
                                           ? methodResolved
                                           : canonicalStdlibMapAccessPathForHelper(
                                                 methodResolved.substr(std::string("/map/").size())))) {
          return failCollectionAccessTargetDiagnostic(
              "unknown call target: " +
              std::string(methodResolved.rfind("/std/collections/map/", 0) == 0
                              ? methodResolved
                              : canonicalStdlibMapAccessPathForHelper(
                                    methodResolved.substr(std::string("/map/").size()))));
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

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
  const auto isCanonicalKeyValueAccessMethodName =
      [&](std::string_view helperName) {
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
    if (isIndexedArgsPackKeyValueReceiverTarget(target, dispatchResolvers)) {
      return true;
    }
    std::string inferredTypeText;
    return inferQueryExprTypeText(target, params, locals, inferredTypeText) &&
           returnsMapCollectionType(inferredTypeText);
  };
  const bool isVectorCompatibilityMethod =
      isVectorCompatibilityHelperName(normalizedMethodName);
  auto hasVisibleRootVectorAlias = [&](std::string_view helperName) {
    const std::string samePath = rootedVectorHelperPath(helperName);
    return hasDefinitionFamilyPath(samePath) ||
           hasDefinitionPath(samePath) ||
           hasDeclaredDefinitionPath(samePath) ||
           hasImportedDefinitionPath(samePath);
  };
  auto shouldPreferRootVectorAliasForMethodExtraArgs = [&]() {
    if (!expr.isMethodCall ||
        expr.args.size() <= 1 ||
        !(normalizedMethodName == "count" ||
          normalizedMethodName == "capacity") ||
        !hasVisibleRootVectorAlias(normalizedMethodName)) {
      return false;
    }
    for (size_t i = 1; i < expr.args.size(); ++i) {
      if (expr.args[i].kind == Expr::Kind::Call ||
          expr.args[i].kind == Expr::Kind::BoolLiteral) {
        return false;
      }
    }
    return true;
  };
  if (expr.namespacePrefix.empty() &&
      !expr.args.empty() &&
      isVectorCompatibilityMethod) {
    std::string elemType;
    const bool isVectorReceiver = resolveVectorTarget(expr.args.front(), elemType);
    const std::string canonicalVectorHelperPath =
        canonicalVectorCompatibilityHelperPathOrFallback(normalizedMethodName);
    if (isVectorReceiver &&
        (hasDefinitionFamilyPath(canonicalVectorHelperPath) ||
         hasDefinitionPath(canonicalVectorHelperPath) ||
         hasDeclaredDefinitionPath(canonicalVectorHelperPath) ||
         hasImportedDefinitionPath(canonicalVectorHelperPath))) {
      if (normalizedMethodName == "count" && expr.args.size() != 1 &&
          !shouldPreferRootVectorAliasForMethodExtraArgs()) {
        return failMethodResolutionDiagnostic("argument count mismatch for builtin count");
      }
      if (normalizedMethodName == "capacity" && expr.args.size() != 1 &&
          !shouldPreferRootVectorAliasForMethodExtraArgs()) {
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
  auto rejectBuiltinStringCountShadowOnKeyValueAccessReceiver = [&](const std::string &resolvedPath) -> bool {
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
        !isCanonicalKeyValueAccessMethodName(accessHelperName)) {
      return false;
    }
    auto canonicalKeyValueAccessReturnsString = [&](std::string helperName) {
      if (helperName != "at" && helperName != "at_unsafe" &&
          helperName != "at_ref" && helperName != "at_unsafe_ref") {
        return false;
      }
      const std::string helperPath =
          metadataBackedCanonicalMapHelperPath(helperName);
      auto defIt = defMap_.find(helperPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      BindingInfo inferredReturn;
      return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
             normalizeBindingTypeName(inferredReturn.typeName) == "string" &&
             inferredReturn.typeTemplateArg.empty();
    };
    if (canonicalKeyValueAccessReturnsString(accessHelperName)) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(receiverExpr);
    if (accessReceiver == nullptr) {
      return false;
    }
    std::string keyValueValueType;
    if (!this->resolveMapValueType(*accessReceiver, dispatchResolvers, keyValueValueType)) {
      if (accessReceiver->kind == Expr::Kind::Call) {
        const std::string receiverPath = resolveCalleePath(*accessReceiver);
        auto receiverDefIt = defMap_.find(receiverPath);
        if (receiverDefIt != defMap_.end() && receiverDefIt->second != nullptr) {
          BindingInfo receiverReturn;
          if (inferDefinitionReturnBinding(*receiverDefIt->second, receiverReturn)) {
            const std::string receiverReturnType =
                receiverReturn.typeTemplateArg.empty()
                    ? receiverReturn.typeName
                    : receiverReturn.typeName + "<" + receiverReturn.typeTemplateArg + ">";
            std::string keyType;
            (void)extractMapKeyValueTypesFromTypeText(
                receiverReturnType, keyType, keyValueValueType);
          }
        }
      }
      if (keyValueValueType.empty()) {
        return false;
      }
    }
    if (normalizeBindingTypeName(keyValueValueType) == "string") {
      return false;
    }
    return failMethodResolutionDiagnostic("argument type mismatch for /string/count parameter values: expected string");
  };
  const std::string removedMapMethodPath =
      this->keyValueNamespacedMethodCompatibilityPath(expr, params, locals, dispatchResolverAdapters);
  if (!removedMapMethodPath.empty()) {
    return failMethodResolutionDiagnostic("unknown method: " + removedMapMethodPath);
  }
  if (expr.args.empty()) {
    return failMethodResolutionDiagnostic("method call missing receiver");
  }
  usedMethodTarget = true;
  hasMethodReceiverIndex = true;
  methodReceiverIndex = 0;
  bool isBuiltinMethod = false;
  auto resolveIndexedArgsPackMapMethod = [&]() -> bool {
    if (!(normalizedMethodName == "count" ||
          normalizedMethodName == "count_ref" ||
          normalizedMethodName == "size" ||
          normalizedMethodName == "contains" ||
          normalizedMethodName == "contains_ref" ||
          normalizedMethodName == "tryAt" ||
          normalizedMethodName == "tryAt_ref" ||
          normalizedMethodName == "at" ||
          normalizedMethodName == "at_ref" ||
          normalizedMethodName == "at_unsafe" ||
          normalizedMethodName == "at_unsafe_ref" ||
          normalizedMethodName == "insert" ||
          normalizedMethodName == "insert_ref")) {
      return false;
    }
    const Expr &receiverExpr = expr.args.front();
    std::string elemType;
    bool borrowedReceiver = false;
    if (dispatchResolvers.resolveIndexedArgsPackElementType != nullptr &&
        dispatchResolvers.resolveIndexedArgsPackElementType(receiverExpr, elemType)) {
      const std::string unwrapped = unwrapReferencePointerTypeText(elemType);
      borrowedReceiver = !unwrapped.empty() &&
                         normalizeBindingTypeName(unwrapped) !=
                             normalizeBindingTypeName(elemType);
    } else if (dispatchResolvers.resolveWrappedIndexedArgsPackElementType != nullptr &&
               dispatchResolvers.resolveWrappedIndexedArgsPackElementType(receiverExpr, elemType)) {
      borrowedReceiver = false;
    } else if (dispatchResolvers.resolveDereferencedIndexedArgsPackElementType != nullptr &&
               dispatchResolvers.resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType)) {
      borrowedReceiver = false;
    } else {
      return false;
    }
    std::string keyType;
    std::string valueType;
    const std::string unwrappedElemType =
        normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType));
    const std::string mapElemType =
        unwrappedElemType.empty() ? elemType : unwrappedElemType;
    if (!extractMapKeyValueTypesFromTypeText(mapElemType, keyType, valueType)) {
      return false;
    }
    std::string helperName = normalizedMethodName;
    if (borrowedReceiver) {
      if (helperName == "count") {
        helperName = "count_ref";
      } else if (helperName == "contains") {
        helperName = "contains_ref";
      } else if (helperName == "tryAt") {
        helperName = "tryAt_ref";
      } else if (helperName == "at") {
        helperName = "at_ref";
      } else if (helperName == "at_unsafe") {
        helperName = "at_unsafe_ref";
      } else if (helperName == "insert") {
        helperName = "insert_ref";
      }
    }
    resolved = preferredKeyValueMethodTargetForCall(params, locals, receiverExpr,
                                              helperName);
    std::string resolvedKeyValueHelperName;
    isBuiltinMethod =
        resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
            resolved, resolvedKeyValueHelperName) &&
        (shouldBuiltinValidateCurrentMapWrapperHelper(
             resolvedKeyValueHelperName) ||
         hasImportedDefinitionPath(resolved));
    return true;
  };
  const bool hasIndexedArgsPackMapMethodTarget =
      resolveIndexedArgsPackMapMethod();
  if (context.hasVectorHelperCallResolution &&
      !hasIndexedArgsPackMapMethodTarget) {
    resolvedMethod = false;
    return true;
  }
  const bool hasBlockArgs = expr.hasBodyArguments || !expr.bodyArguments.empty();
  auto shouldRewriteExperimentalVectorCompatibilityMethodTargetToCanonical =
      [&](std::string_view methodTarget) {
        const std::string canonicalVectorCompatibilityMethodTarget =
            canonicalVectorCompatibilityHelperPathOrFallback(expr.name);
        return isLegacyExperimentalVectorCompatibilityPath(methodTarget) &&
               (hasImportedDefinitionPath(canonicalVectorCompatibilityMethodTarget) ||
                defMap_.count(canonicalVectorCompatibilityMethodTarget) > 0);
  };
  std::string vectorMethodTarget;
  bool resolvedCanonicalVectorCompatibilityMethod = false;
  if (hasIndexedArgsPackMapMethodTarget) {
  } else if (isVectorCompatibilityMethod &&
      expr.namespacePrefix != "vector" &&
      expr.namespacePrefix != "/vector" &&
      !isCanonicalVectorCompatibilityNamespace(expr.namespacePrefix) &&
      resolveVectorHelperMethodTarget(params, locals, expr.args.front(), normalizedMethodName,
                                      vectorMethodTarget)) {
    if (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
        normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string preferredVectorMethodTarget =
            preferredBareVectorHelperTarget(normalizedMethodName);
        if ((shouldPreferRootVectorAliasForMethodExtraArgs() ||
             normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
            isRootedVectorHelperPath(preferredVectorMethodTarget) &&
            (hasDeclaredDefinitionPath(preferredVectorMethodTarget) ||
             hasImportedDefinitionPath(preferredVectorMethodTarget))) {
          resolved = preferredVectorMethodTarget;
          isBuiltinMethod = false;
        } else {
          resolved = canonicalVectorCompatibilityHelperPathOrFallback(
              normalizedMethodName);
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
        vectorMethodTarget =
            canonicalVectorCompatibilityHelperPathOrFallback(expr.name);
      }
      if (hasImportedDefinitionPath(vectorMethodTarget) ||
          hasDefinitionFamilyPath(vectorMethodTarget)) {
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
         expr.name == "to" "_aos" || expr.name == "to" "_aos_ref") &&
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
    auto bindingTypeText = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    auto inferPointerLikeTargetTypeText =
        [&](const Expr &receiverExpr) -> std::string {
      if (receiverExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *binding =
                findParamBinding(params, receiverExpr.name)) {
          if ((binding->typeName == "Pointer" ||
               binding->typeName == "Reference") &&
              !binding->typeTemplateArg.empty()) {
            return binding->typeTemplateArg;
          }
        }
        auto localIt = locals.find(receiverExpr.name);
        if (localIt != locals.end() &&
            (localIt->second.typeName == "Pointer" ||
             localIt->second.typeName == "Reference") &&
            !localIt->second.typeTemplateArg.empty()) {
          return localIt->second.typeTemplateArg;
        }
        return "";
      }

      if (receiverExpr.kind != Expr::Kind::Call) {
        return "";
      }

      std::string builtinName;
      if (getBuiltinPointerName(receiverExpr, builtinName) &&
          builtinName == "location" && receiverExpr.args.size() == 1) {
        const Expr &target = receiverExpr.args.front();
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *binding = findParamBinding(params, target.name)) {
            return bindingTypeText(*binding);
          }
          auto localIt = locals.find(target.name);
          if (localIt != locals.end()) {
            return bindingTypeText(localIt->second);
          }
        }
        const ReturnKind targetKind = inferExprReturnKind(target, params, locals);
        return typeNameForReturnKind(targetKind);
      }

      const std::string receiverPath = resolveCalleePath(receiverExpr);
      auto defIt = defMap_.find(receiverPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return "";
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
          continue;
        }
        base = normalizeBindingTypeName(base);
        if ((base == "Pointer" || base == "Reference") && !arg.empty()) {
          return arg;
        }
      }
      return "";
    };
    auto resolveNamespacedVectorBodyArgumentTarget = [&]() -> bool {
      if (!hasBlockArgs) {
        return false;
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (!getNamespacedCollectionHelperName(
              expr, namespacedCollection, namespacedHelper) ||
          namespacedCollection != "vector" ||
          (namespacedHelper != "count" && namespacedHelper != "capacity")) {
        return false;
      }
      const std::string targetTypeText =
          inferPointerLikeTargetTypeText(expr.args.front());
      if (targetTypeText.empty()) {
        return false;
      }
      resolved = "/" + normalizeBindingTypeName(targetTypeText) + "/" +
                 namespacedHelper;
      isBuiltinMethod = false;
      return true;
    };
    auto resolveInferredMapMethodFallback = [&]() -> bool {
      const std::string helperName = expr.name;
      const bool requestsExplicitVectorHelperNamespace =
          expr.namespacePrefix == "vector" || expr.namespacePrefix == "/vector" ||
          isCanonicalVectorCompatibilityNamespace(expr.namespacePrefix) ||
          isRootedVectorHelperPath(helperName) ||
          isCanonicalVectorCompatibilityPath(helperName);
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
      resolved = preferredKeyValueMethodTargetForCall(params, locals, expr.args.front(),
                                                helperName);
      std::string canonicalKeyValueHelperName;
      if (resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
              resolved, canonicalKeyValueHelperName) &&
          (shouldBuiltinValidateCurrentMapWrapperHelper(
               canonicalKeyValueHelperName) ||
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
    } else if (resolveNamespacedVectorBodyArgumentTarget()) {
    } else if (hasBlockArgs &&
               resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
      isBuiltinMethod = false;
    } else {
      return false;
    }
  } else if (rejectBuiltinStringCountShadowOnKeyValueAccessReceiver(resolved)) {
    return failMethodResolutionDiagnostic(error_);
  } else if (hasBlockArgs) {
    std::string namespacedCollection;
    std::string namespacedHelper;
    if (getNamespacedCollectionHelperName(
            expr, namespacedCollection, namespacedHelper) &&
        namespacedCollection == "vector" &&
        (namespacedHelper == "count" || namespacedHelper == "capacity")) {
      std::string targetTypeText;
      if (expr.args.front().kind == Expr::Kind::Name) {
        if (const BindingInfo *binding =
                findParamBinding(params, expr.args.front().name)) {
          if ((binding->typeName == "Pointer" ||
               binding->typeName == "Reference") &&
              !binding->typeTemplateArg.empty()) {
            targetTypeText = binding->typeTemplateArg;
          }
        }
        auto localIt = locals.find(expr.args.front().name);
        if (targetTypeText.empty() && localIt != locals.end() &&
            (localIt->second.typeName == "Pointer" ||
             localIt->second.typeName == "Reference") &&
            !localIt->second.typeTemplateArg.empty()) {
          targetTypeText = localIt->second.typeTemplateArg;
        }
      } else if (expr.args.front().kind == Expr::Kind::Call) {
        const Expr &receiverExpr = expr.args.front();
        std::string builtinName;
        if (getBuiltinPointerName(receiverExpr, builtinName) &&
            builtinName == "location" && receiverExpr.args.size() == 1) {
          const Expr &target = receiverExpr.args.front();
          if (target.kind == Expr::Kind::Name) {
            if (const BindingInfo *binding = findParamBinding(params, target.name)) {
              targetTypeText = binding->typeTemplateArg.empty()
                                   ? binding->typeName
                                   : binding->typeName + "<" +
                                         binding->typeTemplateArg + ">";
            }
            auto localIt = locals.find(target.name);
            if (targetTypeText.empty() && localIt != locals.end()) {
              targetTypeText = localIt->second.typeTemplateArg.empty()
                                   ? localIt->second.typeName
                                   : localIt->second.typeName + "<" +
                                         localIt->second.typeTemplateArg + ">";
            }
          }
          if (targetTypeText.empty()) {
            const ReturnKind targetKind =
                inferExprReturnKind(target, params, locals);
            targetTypeText = typeNameForReturnKind(targetKind);
          }
        } else {
          const std::string receiverPath = resolveCalleePath(receiverExpr);
          auto defIt = defMap_.find(receiverPath);
          if (defIt != defMap_.end() && defIt->second != nullptr) {
            for (const auto &transform : defIt->second->transforms) {
              if (transform.name != "return" ||
                  transform.templateArgs.size() != 1) {
                continue;
              }
              std::string base;
              std::string arg;
              if (!splitTemplateTypeName(
                      transform.templateArgs.front(), base, arg)) {
                continue;
              }
              base = normalizeBindingTypeName(base);
              if ((base == "Pointer" || base == "Reference") && !arg.empty()) {
                targetTypeText = arg;
                break;
              }
            }
          }
        }
      }
      if (!targetTypeText.empty()) {
        resolved = "/" + normalizeBindingTypeName(targetTypeText) + "/" +
                   namespacedHelper;
        isBuiltinMethod = false;
      }
    } else {
      const std::string pointerLikeType =
          inferPointerLikeCallReturnType(expr.args.front(), params, locals);
      if (!pointerLikeType.empty()) {
        resolved = "/" + pointerLikeType + "/" +
                   normalizeCollectionMethodName(expr.name);
        isBuiltinMethod = false;
      }
    }
  }
  if (!isBuiltinMethod && isVectorCompatibilityMethod && !resolved.empty() &&
      shouldRewriteExperimentalVectorCompatibilityMethodTargetToCanonical(resolved)) {
    resolved = canonicalVectorCompatibilityHelperPathOrFallback(expr.name);
  }
  if (isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity")) {
    isBuiltinMethod = true;
  } else if (isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") &&
             !expr.args.empty()) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType)) {
      isBuiltinMethod = true;
    }
  }
  bool keepBuiltinIndexedArgsPackKeyValueMethod = false;
  keepBuiltinIndexedArgsPackKeyValueMethod = resolveMapTarget(expr.args.front());
  if (expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr.args.front(), accessName) && expr.args.front().args.size() == 2) {
      if (const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(expr.args.front())) {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        keepBuiltinIndexedArgsPackKeyValueMethod =
            keepBuiltinIndexedArgsPackKeyValueMethod ||
            (resolveArgsPackElementTypeForExpr(*accessReceiver, params, locals, elemType) &&
             extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
      }
    }
  }
  auto hasVisibleStdlibMapMethodDefinition = [&](const std::string &path) {
    return hasImportedDefinitionPath(path) || hasDeclaredDefinitionPath(path);
  };
  auto isMissingStdlibMapMethodDefinition = [&](const std::string &path) {
    std::string helperName;
    return resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
               path, helperName) &&
           !hasVisibleStdlibMapMethodDefinition(path);
  };
  if (isMissingStdlibMapMethodDefinition(resolved) &&
      !keepBuiltinIndexedArgsPackKeyValueMethod) {
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
      isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") &&
      expr.args.size() != 1) {
    if (hasNamedArguments(expr.argNames)) {
      return failMethodResolutionDiagnostic(
          "named arguments not supported for builtin calls");
    }
    return failMethodResolutionDiagnostic(
        "argument count mismatch for builtin count");
  }
  if (!isBuiltinMethod &&
      isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity") &&
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
    std::string canonicalSoaMutatorPath = resolved;
    if (const size_t specializationSuffix = canonicalSoaMutatorPath.find("__");
        specializationSuffix != std::string::npos) {
      canonicalSoaMutatorPath.erase(specializationSuffix);
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaMutatorPath, "push") ||
        isLegacyOrCanonicalSoaHelperPath(canonicalSoaMutatorPath, "reserve")) {
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

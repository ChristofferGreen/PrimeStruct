#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <functional>
#include <optional>

namespace primec::semantics {

ReturnKind SemanticsValidator::inferPreDispatchCallReturnKind(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    InferPreDispatchCallContext &context,
    bool &handled) {
  handled = false;
  if (context.dispatchResolvers == nullptr ||
      context.dispatchResolverAdapters == nullptr) {
    return ReturnKind::Unknown;
  }

  const auto &builtinCollectionDispatchResolvers = *context.dispatchResolvers;
  const auto &builtinCollectionDispatchResolverAdapters =
      *context.dispatchResolverAdapters;
  [[maybe_unused]] const auto &resolveIndexedArgsPackElementType =
      builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType;
  [[maybe_unused]] const auto &resolveDereferencedIndexedArgsPackElementType =
      builtinCollectionDispatchResolvers
          .resolveDereferencedIndexedArgsPackElementType;
  [[maybe_unused]] const auto &resolveWrappedIndexedArgsPackElementType =
      builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType;
  [[maybe_unused]] const auto &resolveArgsPackAccessTarget =
      builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
  const auto &resolveArrayTarget =
      builtinCollectionDispatchResolvers.resolveArrayTarget;
  const auto &resolveVectorTarget =
      builtinCollectionDispatchResolvers.resolveVectorTarget;
  [[maybe_unused]] const auto &resolveSoaVectorTarget =
      builtinCollectionDispatchResolvers.resolveSoaVectorTarget;
  [[maybe_unused]] const auto &resolveBufferTarget =
      builtinCollectionDispatchResolvers.resolveBufferTarget;
  const auto &resolveStringTarget =
      builtinCollectionDispatchResolvers.resolveStringTarget;
  const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;

  auto finish = [&](ReturnKind kind) -> ReturnKind {
    handled = true;
    return kind;
  };
  auto failInferPreDispatchDiagnostic = [&](std::string message) -> ReturnKind {
    (void)failExprDiagnostic(expr, std::move(message));
    return finish(ReturnKind::Unknown);
  };
  Expr rewrittenCanonicalExperimentalVectorHelperCall;
  if (tryRewriteCanonicalExperimentalVectorHelperCall(
          expr,
          builtinCollectionDispatchResolvers,
          rewrittenCanonicalExperimentalVectorHelperCall)) {
    return finish(
        inferExprReturnKind(rewrittenCanonicalExperimentalVectorHelperCall,
                            params,
                            locals));
  }

  auto resolveArgsPackCountTarget =
      [&](const Expr &target, std::string &elemType) -> bool {
    return resolveInferArgsPackCountTarget(params, locals, target, elemType);
  };
  auto resolveMethodCallPath =
      [&](const std::string &methodName, std::string &resolvedOut) -> bool {
    return resolveInferMethodCallPath(expr, params, locals, methodName,
                                      resolvedOut);
  };
  auto inferResolvedPathReturnKind =
      [&](const std::string &resolvedPath, ReturnKind &kindOut) -> bool {
    auto methodIt = defMap_.find(resolvedPath);
    if (methodIt == defMap_.end()) {
      return false;
    }
    if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
      return false;
    }
    auto kindIt = returnKinds_.find(resolvedPath);
    if (kindIt == returnKinds_.end()) {
      return false;
    }
    kindOut = kindIt->second;
    return true;
  };

  std::function<ReturnKind(const Expr &)> pointerTargetKind =
      [&](const Expr &pointerExpr) -> ReturnKind {
    if (pointerExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding =
              findParamBinding(params, pointerExpr.name)) {
        if ((paramBinding->typeName == "Pointer" ||
             paramBinding->typeName == "Reference") &&
            !paramBinding->typeTemplateArg.empty()) {
          return inferReferenceTargetKind(paramBinding->typeTemplateArg,
                                          pointerExpr.namespacePrefix);
        }
        return ReturnKind::Unknown;
      }
      auto it = locals.find(pointerExpr.name);
      if (it != locals.end()) {
        if ((it->second.typeName == "Pointer" ||
             it->second.typeName == "Reference") &&
            !it->second.typeTemplateArg.empty()) {
          return inferReferenceTargetKind(it->second.typeTemplateArg,
                                          pointerExpr.namespacePrefix);
        }
      }
      return ReturnKind::Unknown;
    }
    if (pointerExpr.kind == Expr::Kind::Call) {
      std::string pointerName;
      if (getBuiltinPointerName(pointerExpr, pointerName) &&
          pointerName == "location" && pointerExpr.args.size() == 1) {
        const Expr &target = pointerExpr.args.front();
        if (target.kind != Expr::Kind::Name) {
          return ReturnKind::Unknown;
        }
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName == "Reference" &&
              !paramBinding->typeTemplateArg.empty()) {
            return inferReferenceTargetKind(paramBinding->typeTemplateArg,
                                            target.namespacePrefix);
          }
          return returnKindForTypeName(paramBinding->typeName);
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName == "Reference" &&
              !it->second.typeTemplateArg.empty()) {
            return inferReferenceTargetKind(it->second.typeTemplateArg,
                                            target.namespacePrefix);
          }
          return returnKindForTypeName(it->second.typeName);
        }
      }
      if (getBuiltinMemoryName(pointerExpr, pointerName)) {
        if (pointerName == "alloc" && pointerExpr.templateArgs.size() == 1 &&
            pointerExpr.args.size() == 1) {
          return inferReferenceTargetKind(pointerExpr.templateArgs.front(),
                                          pointerExpr.namespacePrefix);
        }
        if (pointerName == "realloc" && pointerExpr.args.size() == 2) {
          return pointerTargetKind(pointerExpr.args.front());
        }
        if (pointerName == "at" && pointerExpr.templateArgs.empty() &&
            pointerExpr.args.size() == 3) {
          return pointerTargetKind(pointerExpr.args.front());
        }
        if (pointerName == "at_unsafe" && pointerExpr.templateArgs.empty() &&
            pointerExpr.args.size() == 2) {
          return pointerTargetKind(pointerExpr.args.front());
        }
        if (pointerName == "reinterpret" && pointerExpr.templateArgs.size() == 1 &&
            pointerExpr.args.size() == 1) {
          return inferReferenceTargetKind(pointerExpr.templateArgs.front(),
                                          pointerExpr.namespacePrefix);
        }
      }
      std::string opName;
      if (getBuiltinOperatorName(pointerExpr, opName) &&
          (opName == "plus" || opName == "minus") &&
          pointerExpr.args.size() == 2) {
        ReturnKind leftKind = pointerTargetKind(pointerExpr.args[0]);
        if (leftKind != ReturnKind::Unknown) {
          return leftKind;
        }
        return pointerTargetKind(pointerExpr.args[1]);
      }
    }
    return ReturnKind::Unknown;
  };

  auto collectionHelperPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };
    auto isUnrootedKeyValueHelperSurfacePath =
        [](std::string_view candidatePath) {
      const StdlibSurfaceMetadata *metadata = mapHelperSurfaceMetadataLocal();
      if (metadata == nullptr) {
        return false;
      }
      std::string canonicalRoot(metadata->canonicalPath);
      if (!canonicalRoot.empty() && canonicalRoot.front() == '/') {
        canonicalRoot.erase(canonicalRoot.begin());
      }
      return candidatePath.size() > canonicalRoot.size() &&
             candidatePath.rfind(canonicalRoot, 0) == 0 &&
             candidatePath[canonicalRoot.size()] == '/';
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 ||
          isUnrootedCanonicalVectorCompatibilityPath(normalizedPath) ||
          isUnrootedKeyValueHelperSurfacePath(normalizedPath)) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix =
          normalizedPath.substr(std::string("/array/").size());
      if (suffix != "count" && suffix != "capacity" && suffix != "at" &&
          suffix != "at_unsafe" && suffix != "push" && suffix != "pop" &&
          suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique(canonicalVectorCompatibilityHelperPathOrFallback(suffix));
      }
    } else if (isCanonicalVectorCompatibilityPath(normalizedPath)) {
      // Keep explicit canonical vector helper calls isolated so canonical
      // paths no longer fall back through array aliases.
    }
    return candidates;
  };
  auto isTemplateCompatibleDefinition = [&](const Definition &def) {
    if (expr.templateArgs.empty()) {
      return true;
    }
    return !def.templateArgs.empty() &&
           def.templateArgs.size() == expr.templateArgs.size();
  };

  std::string earlyPointerBuiltin;
  if (getBuiltinPointerName(expr, earlyPointerBuiltin) && expr.args.size() == 1) {
    if (earlyPointerBuiltin == "dereference") {
      ReturnKind targetKind = pointerTargetKind(expr.args.front());
      if (targetKind != ReturnKind::Unknown) {
        return finish(targetKind);
      }
    }
    return finish(ReturnKind::Unknown);
  }
  if (expr.isMethodCall && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result") {
    if (expr.name == "error" && expr.args.size() == 2) {
      return finish(ReturnKind::Bool);
    }
    if (expr.name == "why" && expr.args.size() == 2) {
      return finish(ReturnKind::String);
    }
  }

  std::string resolvedCallee = resolveCalleePath(expr);
  std::string canonicalExperimentalKeyValueHelperResolved;
  if (!expr.isMethodCall &&
      (defMap_.count(resolvedCallee) == 0 ||
       shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath(
           resolvedCallee)) &&
      canonicalizeExperimentalKeyValueHelperResolvedPath(
          resolvedCallee, canonicalExperimentalKeyValueHelperResolved)) {
    resolvedCallee = canonicalExperimentalKeyValueHelperResolved;
  }
  Expr rewrittenCanonicalExperimentalKeyValueHelperCall;
  if (!expr.isMethodCall &&
      tryRewriteCanonicalExperimentalKeyValueHelperCall(
          expr,
          builtinCollectionDispatchResolvers,
          rewrittenCanonicalExperimentalKeyValueHelperCall)) {
    return finish(inferExprReturnKind(
        rewrittenCanonicalExperimentalKeyValueHelperCall, params, locals));
  }
  std::string borrowedExplicitCanonicalExperimentalKeyValueHelperPath;
  if (!expr.isMethodCall &&
      explicitCanonicalExperimentalKeyValueBorrowedHelperPath(
          expr,
          builtinCollectionDispatchResolvers,
          borrowedExplicitCanonicalExperimentalKeyValueHelperPath)) {
    return finish(ReturnKind::Unknown);
  }
  if (!expr.isMethodCall &&
      isArrayNamespacedVectorCountCompatibilityCall(
          expr, builtinCollectionDispatchResolvers)) {
    return finish(ReturnKind::Unknown);
  }
  const std::string directRemovedKeyValueCompatibilityPath =
      !expr.isMethodCall
          ? directMapHelperCompatibilityPath(expr,
                                             params,
                                             locals,
                                             builtinCollectionDispatchResolverAdapters)
          : std::string();
  auto isRemovedKeyValueAccessCompatibilityPath = [](std::string_view path) {
    const std::string helperName =
        metadataBackedMapHelperRootAliasMethodName(path);
    return helperName == "at" || helperName == "at_unsafe";
  };
  const bool isKeyValueNamespacedAccessCompatibilityCall =
      isRemovedKeyValueAccessCompatibilityPath(
          directRemovedKeyValueCompatibilityPath);
  if (!expr.isMethodCall && isKeyValueNamespacedAccessCompatibilityCall) {
    return finish(ReturnKind::Unknown);
  }
  const std::string removedCollectionMethodCompatibilityPath =
      expr.isMethodCall ? methodRemovedCollectionCompatibilityPath(
                              expr,
                              params,
                              locals,
                              builtinCollectionDispatchResolverAdapters)
                        : "";
  if (!removedCollectionMethodCompatibilityPath.empty()) {
    return finish(ReturnKind::Unknown);
  }

  const auto resolvedCandidates = collectionHelperPathCandidates(resolvedCallee);
  context.resolved = resolvedCandidates.empty()
                         ? preferVectorStdlibHelperPath(resolvedCallee)
                         : resolvedCandidates.front();
  bool hasResolvedPath = !expr.isMethodCall;
  if (!expr.isMethodCall && !resolvedCandidates.empty()) {
    std::string firstTemplateCompatibleCandidate;
    for (const auto &candidate : resolvedCandidates) {
      auto candidateIt = defMap_.find(candidate);
      if (candidateIt == defMap_.end() || !candidateIt->second) {
        continue;
      }
      if (!isTemplateCompatibleDefinition(*candidateIt->second)) {
        continue;
      }
      if (firstTemplateCompatibleCandidate.empty()) {
        firstTemplateCompatibleCandidate = candidate;
      }
      if (!ensureDefinitionReturnKindReady(*candidateIt->second)) {
        return finish(ReturnKind::Unknown);
      }
      auto candidateKindIt = returnKinds_.find(candidate);
      const bool candidateReturnsStruct =
          candidateKindIt != returnKinds_.end() &&
          candidateKindIt->second == ReturnKind::Array;
      if (candidateReturnsStruct &&
          returnStructs_.find(candidate) != returnStructs_.end()) {
        context.resolved = candidate;
        hasResolvedPath = true;
        break;
      }
    }
    if (!firstTemplateCompatibleCandidate.empty() &&
        !expr.templateArgs.empty() &&
        returnStructs_.find(context.resolved) == returnStructs_.end()) {
      context.resolved = firstTemplateCompatibleCandidate;
      hasResolvedPath = true;
    }
  }
  if (expr.isMethodCall && expr.name == "ok" && expr.args.size() >= 1) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
      return finish((expr.args.size() > 1) ? ReturnKind::Int64
                                           : ReturnKind::Int);
    }
  }
  if (expr.isMethodCall && expr.name == "why" && expr.args.size() == 2) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
      return finish(ReturnKind::String);
    }
  }
  if (expr.isMethodCall) {
    const std::string removedCollectionMethodPath =
        methodRemovedCollectionCompatibilityPath(
            expr, params, locals, builtinCollectionDispatchResolverAdapters);
    if (!removedCollectionMethodPath.empty()) {
      return failInferPreDispatchDiagnostic(
          "unknown method: " + removedCollectionMethodPath);
    }
  }
  if (expr.isMethodCall && isVectorBuiltinName(expr, "count") &&
      expr.args.size() == 1 &&
      !isArrayNamespacedVectorCountCompatibilityCall(
          expr, builtinCollectionDispatchResolvers)) {
    std::string elemType;
    if (resolveArgsPackCountTarget(expr.args.front(), elemType)) {
      const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
      if (!hasDefinitionPath(methodPath)) {
        return finish(ReturnKind::Int);
      }
      context.resolved = methodPath;
      hasResolvedPath = true;
    } else if (resolveVectorTarget(expr.args.front(), elemType)) {
      const std::string methodPath = preferredBareVectorHelperTarget("count");
      if (!hasDefinitionPath(methodPath)) {
        return finish(ReturnKind::Int);
      }
      context.resolved = methodPath;
      hasResolvedPath = true;
    } else if (resolveArrayTarget(expr.args.front(), elemType)) {
      const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
      if (!hasDefinitionPath(methodPath)) {
        return finish(ReturnKind::Int);
      }
      context.resolved = methodPath;
      hasResolvedPath = true;
    } else if (resolveStringTarget(expr.args.front())) {
      if (!hasDefinitionPath("/string/count")) {
        return finish(ReturnKind::Int);
      }
      context.resolved = "/string/count";
      hasResolvedPath = true;
    }
  }
  if (expr.isMethodCall && isVectorBuiltinName(expr, "capacity") &&
      expr.args.size() == 1) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType)) {
      const std::string methodPath = preferredBareVectorHelperTarget("capacity");
      if (!hasDefinitionPath(methodPath)) {
        return finish(ReturnKind::Int);
      }
      context.resolved = methodPath;
      hasResolvedPath = true;
    }
  }
  if (expr.isMethodCall) {
    std::string methodResolved;
    bool resolvedMethodTarget = false;
    bool isBuiltinMethod = false;
    if (!expr.args.empty() &&
        resolveMethodTarget(params,
                            locals,
                            expr.namespacePrefix,
                            expr.args.front(),
                            expr.name,
                            methodResolved,
                            isBuiltinMethod)) {
      resolvedMethodTarget = true;
    } else if (resolveMethodCallPath(expr.name, methodResolved)) {
      resolvedMethodTarget = true;
    }
    if (resolvedMethodTarget) {
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      std::string logicalMethodResolved = methodResolved;
      std::string canonicalMethodResolved;
      if (canonicalizeExperimentalKeyValueHelperResolvedPath(
              methodResolved, canonicalMethodResolved)) {
        logicalMethodResolved = canonicalMethodResolved;
      }
      auto hasVisibleStdlibMapMethodDefinition = [&](const std::string &path) {
        return hasImportedDefinitionPath(path) || hasDeclaredDefinitionPath(path);
      };
      auto resolveStdlibMapMethodHelperName = [&](const std::string &path,
                                                  std::string &helperNameOut) {
        return resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
            path, helperNameOut);
      };
      auto isMapMethodWithBuiltinReturn = [&](std::string_view helperName) {
        return helperName == "contains" || helperName == "contains_ref" ||
               helperName == "tryAt" || helperName == "tryAt_ref" ||
               helperName == "at" || helperName == "at_ref" ||
               helperName == "at_unsafe" || helperName == "at_unsafe_ref";
      };
      auto isMapMethodNeedingVisibleDefinition = [&](std::string_view helperName) {
        return isMapMethodWithBuiltinReturn(helperName) ||
               helperName == "insert" || helperName == "insert_ref";
      };
      auto isVisibleStdlibMapMethodWithBuiltinReturn = [&](const std::string &path) {
        std::string helperName;
        return resolveStdlibMapMethodHelperName(path, helperName) &&
               isMapMethodWithBuiltinReturn(helperName) &&
               hasVisibleStdlibMapMethodDefinition(path);
      };
      auto isMissingStdlibMapMethodDefinition = [&](const std::string &path) {
        std::string helperName;
        return resolveStdlibMapMethodHelperName(path, helperName) &&
               isMapMethodNeedingVisibleDefinition(helperName) &&
               !hasVisibleStdlibMapMethodDefinition(path);
      };
      if (logicalMethodResolved == "/file_error/why" ||
          logicalMethodResolved == "/FileError/why") {
        return finish(ReturnKind::String);
      }
      if (isVisibleStdlibMapMethodWithBuiltinReturn(logicalMethodResolved) &&
          !hasDefinitionPath(logicalMethodResolved)) {
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionMethodReturnKind(
                logicalMethodResolved,
                expr.args.front(),
                builtinCollectionDispatchResolvers,
                builtinMethodKind)) {
          return finish(builtinMethodKind);
        }
      }
      std::string builtinMapKeyType;
      std::string builtinMapValueType;
      if (isMissingStdlibMapMethodDefinition(logicalMethodResolved) &&
          !hasDefinitionPath(logicalMethodResolved) &&
          !resolveMapTarget(expr.args.front(), builtinMapKeyType, builtinMapValueType)) {
        return failInferPreDispatchDiagnostic(
            soaUnavailableMethodDiagnostic(methodResolved));
      }
      ReturnKind builtinMethodKind = ReturnKind::Unknown;
      if (!hasDefinitionPath(logicalMethodResolved) &&
          resolveBuiltinCollectionMethodReturnKind(
              logicalMethodResolved,
              expr.args.front(),
              builtinCollectionDispatchResolvers,
              builtinMethodKind)) {
        return finish(builtinMethodKind);
      }
      if (!hasDefinitionPath(methodResolved) &&
          !hasImportedDefinitionPath(methodResolved)) {
        return failInferPreDispatchDiagnostic(
            soaUnavailableMethodDiagnostic(methodResolved));
      }
      context.resolved = methodResolved;
      hasResolvedPath = true;
    }
  }
  if (!expr.isMethodCall) {
    std::string vectorHelper;
    if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {
      std::string namespacedCollection;
      std::string namespacedHelper;
      const bool isNamespacedCollectionHelperCall =
          getNamespacedCollectionHelperName(
              expr, namespacedCollection, namespacedHelper);
      const bool isNamespacedVectorHelperCall =
          isNamespacedCollectionHelperCall && namespacedCollection == "vector";
      const bool isStdNamespacedVectorCanonicalHelperCall =
          isCanonicalVectorCompatibilityPath(resolveCalleePath(expr)) &&
          (namespacedHelper == "push" || namespacedHelper == "pop" ||
           namespacedHelper == "reserve" || namespacedHelper == "clear" ||
           namespacedHelper == "remove_at" ||
           namespacedHelper == "remove_swap");
      if ((!hasDefinitionPath(context.resolved) || isNamespacedVectorHelperCall) &&
          !(isStdNamespacedVectorCanonicalHelperCall &&
            !hasDefinitionPath(context.resolved))) {
        auto isVectorHelperReceiverName = [&](const Expr &candidate) -> bool {
          if (candidate.kind != Expr::Kind::Name) {
            return false;
          }
          std::string typeName;
          if (const BindingInfo *paramBinding =
                  findParamBinding(params, candidate.name)) {
            typeName = normalizeBindingTypeName(paramBinding->typeName);
          } else {
            auto it = locals.find(candidate.name);
            if (it != locals.end()) {
              typeName = normalizeBindingTypeName(it->second.typeName);
            }
          }
          return typeName == "vector" || typeName == "soa" "_vector";
        };
        auto tryResolveReceiverIndex = [&](size_t index) -> std::optional<ReturnKind> {
          if (index >= expr.args.size()) {
            return std::nullopt;
          }
          const Expr &receiverCandidate = expr.args[index];
          std::string methodResolved;
          if (!resolveVectorHelperMethodTarget(params,
                                               locals,
                                               receiverCandidate,
                                               vectorHelper,
                                               methodResolved)) {
            return std::nullopt;
          }
          methodResolved = preferVectorStdlibHelperPath(methodResolved);
          ReturnKind helperReturnKind = ReturnKind::Unknown;
          if (inferResolvedPathReturnKind(methodResolved, helperReturnKind)) {
            return helperReturnKind;
          }
          if (hasDefinitionPath(methodResolved) ||
              hasImportedDefinitionPath(methodResolved)) {
            context.resolved = methodResolved;
            hasResolvedPath = true;
          }
          return std::nullopt;
        };
        const bool hasNamedArgs = hasNamedArguments(expr.argNames);
        if (hasNamedArgs) {
          bool hasValuesNamedReceiver = false;
          for (size_t i = 0; i < expr.args.size(); ++i) {
            if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
                *expr.argNames[i] == "values") {
              hasValuesNamedReceiver = true;
              if (const auto receiverKind = tryResolveReceiverIndex(i);
                  receiverKind.has_value()) {
                return finish(*receiverKind);
              }
            }
          }
          if (!hasValuesNamedReceiver) {
            if (const auto receiverKind = tryResolveReceiverIndex(0);
                receiverKind.has_value()) {
              return finish(*receiverKind);
            }
            for (size_t i = 1; i < expr.args.size(); ++i) {
              if (const auto receiverKind = tryResolveReceiverIndex(i);
                  receiverKind.has_value()) {
                return finish(*receiverKind);
              }
            }
          }
        } else {
          if (const auto receiverKind = tryResolveReceiverIndex(0);
              receiverKind.has_value()) {
            return finish(*receiverKind);
          }
        }
        const bool probePositionalReorderedReceiver =
            !hasNamedArgs && expr.args.size() > 1 &&
            (expr.args.front().kind == Expr::Kind::Literal ||
             expr.args.front().kind == Expr::Kind::BoolLiteral ||
             expr.args.front().kind == Expr::Kind::FloatLiteral ||
             expr.args.front().kind == Expr::Kind::StringLiteral ||
             (expr.args.front().kind == Expr::Kind::Name &&
              !isVectorHelperReceiverName(expr.args.front())));
        if (probePositionalReorderedReceiver) {
          for (size_t i = 1; i < expr.args.size(); ++i) {
            if (const auto receiverKind = tryResolveReceiverIndex(i);
                receiverKind.has_value()) {
              return finish(*receiverKind);
            }
          }
        }
      }
    }
  }

  (void)hasResolvedPath;
  return ReturnKind::Unknown;
}

} // namespace primec::semantics

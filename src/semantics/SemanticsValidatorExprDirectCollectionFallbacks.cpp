#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprDirectCollectionFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::optional<Expr> &rewrittenExprOut) {
  auto failDirectCollectionFallbackDiagnostic =
      [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  rewrittenExprOut.reset();

  const std::string removedRootedVectorDirectCallDiagnostic =
      getRemovedRootedVectorDirectCallDiagnostic(expr);
  if (!removedRootedVectorDirectCallDiagnostic.empty()) {
    return failDirectCollectionFallbackDiagnostic(
        removedRootedVectorDirectCallDiagnostic);
  }

  Expr rewrittenVectorHelperCall;
  auto directVectorMutatorHelperName = [&]() -> std::string {
    if (expr.isMethodCall || !expr.namespacePrefix.empty() ||
        expr.name.find('/') != std::string::npos ||
        !isPublishedVectorMutatorHelperName(expr.name)) {
      return {};
    }
    return expr.name;
  };
  if (const std::string vectorMutatorHelper = directVectorMutatorHelperName();
      !vectorMutatorHelper.empty() && !expr.args.empty()) {
    size_t receiverIndex = 0;
    if (hasNamedArguments(expr.argNames)) {
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          receiverIndex = i;
          break;
        }
      }
    }
    const Expr &receiverExpr = expr.args[receiverIndex];
    std::string elemType;
    const bool isVectorReceiver =
        dispatchResolvers.resolveVectorTarget != nullptr &&
        dispatchResolvers.resolveVectorTarget(receiverExpr, elemType);
    std::string keyType;
    std::string valueType;
    const bool isNonVectorCollectionReceiver =
        (dispatchResolvers.resolveArrayTarget != nullptr &&
         dispatchResolvers.resolveArrayTarget(receiverExpr, elemType)) ||
        (dispatchResolvers.resolveStringTarget != nullptr &&
         dispatchResolvers.resolveStringTarget(receiverExpr)) ||
        (dispatchResolvers.resolveMapTarget != nullptr &&
         dispatchResolvers.resolveMapTarget(receiverExpr, keyType, valueType));
    if (isNonVectorCollectionReceiver && !isVectorReceiver) {
      return failDirectCollectionFallbackDiagnostic(
          vectorMutatorHelper + " requires vector binding");
    }
    if (isVectorReceiver) {
      if (expr.sourceIsMethodCall) {
        const std::string canonicalPath =
            canonicalVectorCompatibilityHelperPathOrFallback(
                vectorMutatorHelper);
        if (!hasDeclaredDefinitionPath(canonicalPath) &&
            !hasImportedDefinitionPath(canonicalPath)) {
          return failDirectCollectionFallbackDiagnostic(
              "unknown method: " + canonicalPath);
        }
        if ((vectorMutatorHelper == "reserve" ||
             vectorMutatorHelper == "remove_at" ||
             vectorMutatorHelper == "remove_swap") &&
            expr.args.size() == 2) {
          const ReturnKind payloadKind =
              inferExprReturnKind(expr.args[1], params, locals);
          if (payloadKind != ReturnKind::Int &&
              payloadKind != ReturnKind::Int64 &&
              payloadKind != ReturnKind::UInt64) {
            const std::string payloadName =
                vectorMutatorHelper == "reserve" ? "capacity" : "index";
            return failDirectCollectionFallbackDiagnostic(
                vectorMutatorHelper + " requires integer " + payloadName);
          }
        }
      }
      Expr rewrittenBareVectorMutatorCall;
      if (this->tryRewriteBareVectorHelperCall(
              expr,
              vectorMutatorHelper,
              dispatchResolvers,
              rewrittenBareVectorMutatorCall)) {
        rewrittenExprOut = rewrittenBareVectorMutatorCall;
        return true;
      }
    }
  }
  auto methodVectorMutatorHelperName = [&]() -> std::string {
    if (!expr.isMethodCall || expr.name.empty() ||
        !isPublishedVectorMutatorHelperName(expr.name)) {
      return {};
    }
    return expr.name;
  };
  if (const std::string vectorMutatorHelper = methodVectorMutatorHelperName();
      !vectorMutatorHelper.empty() && !expr.args.empty()) {
    const Expr &receiverExpr = expr.args.front();
    std::string elemType;
    const bool isVectorReceiver =
        dispatchResolvers.resolveVectorTarget != nullptr &&
        dispatchResolvers.resolveVectorTarget(receiverExpr, elemType);
    std::string keyType;
    std::string valueType;
    const bool isNonVectorCollectionReceiver =
        (dispatchResolvers.resolveArrayTarget != nullptr &&
         dispatchResolvers.resolveArrayTarget(receiverExpr, elemType)) ||
        (dispatchResolvers.resolveStringTarget != nullptr &&
         dispatchResolvers.resolveStringTarget(receiverExpr)) ||
        (dispatchResolvers.resolveMapTarget != nullptr &&
         dispatchResolvers.resolveMapTarget(receiverExpr, keyType, valueType));
    if (isNonVectorCollectionReceiver && !isVectorReceiver) {
      return failDirectCollectionFallbackDiagnostic(
          vectorMutatorHelper + " requires vector binding");
    }
    if (isVectorReceiver) {
      const std::string canonicalPath =
          canonicalVectorCompatibilityHelperPathOrFallback(vectorMutatorHelper);
      std::string methodTarget;
      if (!resolveVectorHelperMethodTarget(
              params, locals, receiverExpr, vectorMutatorHelper, methodTarget) ||
          methodTarget.empty()) {
        methodTarget = canonicalPath;
      }
      if (!hasDeclaredDefinitionPath(methodTarget) &&
          !hasImportedDefinitionPath(methodTarget)) {
        return failDirectCollectionFallbackDiagnostic(
            "unknown method: " + canonicalPath);
      }
      if ((vectorMutatorHelper == "reserve" ||
           vectorMutatorHelper == "remove_at" ||
           vectorMutatorHelper == "remove_swap") &&
          expr.args.size() == 2) {
        const ReturnKind payloadKind =
            inferExprReturnKind(expr.args[1], params, locals);
        if (payloadKind != ReturnKind::Int &&
            payloadKind != ReturnKind::Int64 &&
            payloadKind != ReturnKind::UInt64) {
          const std::string payloadName =
              vectorMutatorHelper == "reserve" ? "capacity" : "index";
          return failDirectCollectionFallbackDiagnostic(
              vectorMutatorHelper + " requires integer " + payloadName);
        }
      }
      Expr rewrittenVectorMethodCall = expr;
      rewrittenVectorMethodCall.isMethodCall = false;
      rewrittenVectorMethodCall.namespacePrefix.clear();
      rewrittenVectorMethodCall.name = methodTarget;
      rewrittenExprOut = rewrittenVectorMethodCall;
      return true;
    }
  }

  const bool isBareVectorAccessHelperCall =
      expr.namespacePrefix.empty() &&
      (expr.name == "at" || expr.name == "at_unsafe");
  const bool isResolvedCanonicalVectorAccessHelper =
      isStdNamespacedVectorCompatibilityHelperPath(resolved, "at") ||
      isStdNamespacedVectorCompatibilityHelperPath(resolved, "at_unsafe");
  if (!expr.isMethodCall &&
      !hasNamedArguments(expr.argNames) &&
      expr.args.size() >= 1 &&
      (isBareVectorAccessHelperCall ||
       isResolvedCanonicalVectorAccessHelper)) {
    const bool isUnsafeHelper =
        expr.name == "at_unsafe" ||
        isStdNamespacedVectorCompatibilityHelperPath(resolved, "at_unsafe");
    const std::string helperName = isUnsafeHelper ? "at_unsafe" : "at";
    std::string experimentalElemType;
    std::string receiverTypeText;
    if (inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText)) {
      BindingInfo inferredBinding;
      const std::string normalizedType =
          normalizeBindingTypeName(receiverTypeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
        inferredBinding.typeTemplateArg.clear();
      }
      const std::string normalizedBase =
          normalizeBindingTypeName(inferredBinding.typeName);
      if (normalizedBase != "Reference" && normalizedBase != "Pointer" &&
          extractCollectionVectorElementType(inferredBinding, experimentalElemType)) {
        if (isBareVectorAccessHelperCall) {
          rewrittenExprOut = expr;
          rewrittenExprOut->isMethodCall = true;
          rewrittenExprOut->name = helperName;
          rewrittenExprOut->namespacePrefix.clear();
          return true;
        }
        return true;
      }
    }
    std::string builtinVectorElemType;
    const bool resolvesBuiltinVectorReceiver =
        dispatchResolvers.resolveVectorTarget != nullptr &&
        dispatchResolvers.resolveVectorTarget(expr.args.front(),
                                             builtinVectorElemType);
    bool isBuiltinMethod = false;
    std::string methodResolved;
    if (!resolvesBuiltinVectorReceiver &&
        resolveMethodTarget(params, locals, expr.namespacePrefix,
                            expr.args.front(), helperName, methodResolved,
                            isBuiltinMethod) &&
        !isBuiltinMethod &&
        (hasDeclaredDefinitionPath(methodResolved) ||
         hasImportedDefinitionPath(methodResolved))) {
      rewrittenExprOut = expr;
      rewrittenExprOut->isMethodCall = true;
      rewrittenExprOut->name = helperName;
      rewrittenExprOut->namespacePrefix.clear();
      return true;
    }
    Expr rewrittenBareVectorHelperCall;
    if (this->tryRewriteBareVectorHelperCall(
            expr, helperName, dispatchResolvers,
            rewrittenBareVectorHelperCall)) {
      rewrittenExprOut = rewrittenBareVectorHelperCall;
      return true;
    }
  } else if (this->tryRewriteBareVectorHelperCall(
                 expr, "at", dispatchResolvers, rewrittenVectorHelperCall) ||
             this->tryRewriteBareVectorHelperCall(
                 expr, "at_unsafe", dispatchResolvers,
                 rewrittenVectorHelperCall)) {
    rewrittenExprOut = rewrittenVectorHelperCall;
    return true;
  }

  return true;
}

} // namespace primec::semantics

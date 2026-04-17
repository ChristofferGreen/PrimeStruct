#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprDirectCollectionFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprDirectCollectionFallbackContext &context,
    std::optional<Expr> &rewrittenExprOut) {
  auto failDirectCollectionFallbackDiagnostic =
      [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  rewrittenExprOut.reset();
  if (context.dispatchResolvers == nullptr) {
    return true;
  }

  auto explicitDirectCallPath = [](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        candidate.name.empty()) {
      return {};
    }
    if (candidate.name.front() == '/') {
      return candidate.name;
    }
    std::string namespacePrefix = candidate.namespacePrefix;
    if (!namespacePrefix.empty() && namespacePrefix.front() != '/') {
      namespacePrefix.insert(namespacePrefix.begin(), '/');
    }
    if (namespacePrefix.empty()) {
      return "/" + candidate.name;
    }
    return namespacePrefix + "/" + candidate.name;
  };
  const std::string explicitPath = explicitDirectCallPath(expr);
  if (!expr.isMethodCall &&
      (explicitPath == "/vector/at" || explicitPath == "/vector/at_unsafe") &&
      !hasDeclaredDefinitionPath(explicitPath) &&
      !hasImportedDefinitionPath(explicitPath)) {
    return failDirectCollectionFallbackDiagnostic("unknown call target: " +
                                                  explicitPath);
  }

  const auto &dispatchResolvers = *context.dispatchResolvers;
  const auto &resolveStringTarget = dispatchResolvers.resolveStringTarget;
  const auto &resolveArrayTarget = dispatchResolvers.resolveArrayTarget;

  if (!expr.isMethodCall && context.isStdNamespacedVectorCountCall &&
      expr.args.size() == 1 &&
      !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
      !hasImportedDefinitionPath("/std/collections/vector/count")) {
    std::string elemType;
    if (resolveStringTarget(expr.args.front()) ||
        resolveArrayTarget(expr.args.front(), elemType) ||
        dispatchResolvers.resolveExperimentalVectorTarget(
            expr.args.front(), elemType)) {
      return failDirectCollectionFallbackDiagnostic(
          "unknown call target: /std/collections/vector/count");
    }
  }

  Expr rewrittenVectorHelperCall;
  const bool isBareVectorAccessHelperCall =
      expr.namespacePrefix.empty() &&
      (expr.name == "at" || expr.name == "at_unsafe");
  const bool isResolvedCanonicalVectorAccessHelper =
      resolved == "/std/collections/vector/at" ||
      resolved == "/std/collections/vector/at_unsafe";
  if (!expr.isMethodCall &&
      !hasNamedArguments(expr.argNames) &&
      expr.args.size() >= 1 &&
      (isBareVectorAccessHelperCall ||
       isResolvedCanonicalVectorAccessHelper)) {
    const bool isUnsafeHelper =
        expr.name == "at_unsafe" ||
        resolved == "/std/collections/vector/at_unsafe";
    const std::string helperName = isUnsafeHelper ? "at_unsafe" : "at";
    auto resolvesExperimentalVectorValueReceiverForBareAccess =
        [&](const Expr &receiverExpr, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      std::string receiverTypeText;
      if (!inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText)) {
        return false;
      }
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
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        return false;
      }
      return extractExperimentalVectorElementType(inferredBinding, elemTypeOut);
    };
    std::string experimentalElemType;
    if (resolvesExperimentalVectorValueReceiverForBareAccess(
            expr.args.front(), experimentalElemType)) {
      if (isBareVectorAccessHelperCall) {
        rewrittenExprOut = expr;
        rewrittenExprOut->isMethodCall = true;
        rewrittenExprOut->name = helperName;
        rewrittenExprOut->namespacePrefix.clear();
        return true;
      }
      return true;
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

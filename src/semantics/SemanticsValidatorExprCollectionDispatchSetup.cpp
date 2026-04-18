#include "SemanticsValidator.h"
#include "SemanticsVectorCompatibilityHelpers.h"

namespace primec::semantics {

namespace {

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

} // namespace

bool SemanticsValidator::prepareExprCollectionDispatchSetup(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    std::string &resolved,
    ExprCollectionDispatchSetup &setupOut) {
  setupOut = {};
  auto failCollectionDispatchDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  std::string accessHelperName;
  std::string namespacedCollection;
  const bool isNamespacedCollectionHelperCall =
      getNamespacedCollectionHelperName(expr, namespacedCollection, setupOut.namespacedHelper);
  setupOut.isNamespacedVectorHelperCall =
      isNamespacedCollectionHelperCall && namespacedCollection == "vector";
  setupOut.isStdNamespacedMapCountCall =
      !expr.isMethodCall &&
      (resolveCalleePath(expr) == "/std/collections/map/count" ||
       resolveCalleePath(expr) == "/std/collections/map/count_ref");
  setupOut.isNamespacedMapHelperCall =
      isNamespacedCollectionHelperCall && namespacedCollection == "map";
  const std::string directRemovedMapCompatibilityPath =
      !expr.isMethodCall
          ? this->directMapHelperCompatibilityPath(expr, params, locals, dispatchResolverAdapters)
          : std::string();
  const bool isMapNamespacedCountCompatibilityCall =
      directRemovedMapCompatibilityPath == "/map/count";
  setupOut.isNamespacedMapCountCall =
      !expr.isMethodCall && setupOut.isNamespacedMapHelperCall &&
      (setupOut.namespacedHelper == "count" ||
       setupOut.namespacedHelper == "count_ref") &&
      !setupOut.isStdNamespacedMapCountCall &&
      !isMapNamespacedCountCompatibilityCall &&
      !hasDefinitionPath(resolved);

  auto resolveMapTarget = [&](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
        dispatchResolvers.resolveExperimentalMapTarget(target, keyType, valueType)) {
      return true;
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(target));
    if ((defIt == defMap_.end() || defIt->second == nullptr) &&
        !target.name.empty() && target.name.find('/') == std::string::npos) {
      defIt = defMap_.find("/" + target.name);
    }
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferredTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
      if (returnsMapCollectionType(inferredTypeText)) {
        return true;
      }
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          returnsMapCollectionType(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
  };
  auto hasVisibleCanonicalVectorHelperPath = [&](const std::string &path) {
    const bool isStdlibVectorWrapperDefinition =
        currentValidationState_.context.definitionPath.rfind("/std/collections/", 0) == 0 ||
        currentValidationState_.context.definitionPath.rfind("/std/image/", 0) == 0;
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    auto paramsIt = paramsByDef_.find(path);
    if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
      std::string experimentalElemType;
      if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
        return isStdlibVectorWrapperDefinition;
      }
    }
    return true;
  };
  const bool callsStdNamespacedVectorCountHelper =
      isStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall, resolveCalleePath(expr), "count");
  const bool callsStdNamespacedVectorCapacityHelper =
      isStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall, resolveCalleePath(expr), "capacity");
  const bool callsInvisibleStdNamespacedVectorCapacityHelper =
      isInvisibleStdNamespacedVectorCompatibilityDirectCall(
          expr.isMethodCall,
          resolveCalleePath(expr),
          "capacity",
          hasVisibleCanonicalVectorHelperPath("/std/collections/vector/capacity"));

  setupOut.isNamespacedVectorCountCall =
      !callsStdNamespacedVectorCountHelper &&
      setupOut.isNamespacedVectorHelperCall && setupOut.namespacedHelper == "count" &&
      isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
      !hasDefinitionPath(resolved) &&
      !this->isArrayNamespacedVectorCountCompatibilityCall(expr, dispatchResolvers);
  setupOut.isUnnamespacedMapCountFallbackCall =
      !expr.isMethodCall &&
      this->isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, dispatchResolverAdapters);
  setupOut.isResolvedMapCountCall =
      !expr.isMethodCall && resolved == "/map/count" &&
      !isMapNamespacedCountCompatibilityCall &&
      !setupOut.isUnnamespacedMapCountFallbackCall;
  setupOut.isNamespacedVectorCapacityCall =
      !expr.isMethodCall && !callsStdNamespacedVectorCapacityHelper &&
      setupOut.isNamespacedVectorHelperCall && setupOut.namespacedHelper == "capacity" &&
      isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
      !hasDefinitionPath(resolved);
  const bool hasBuiltinAccessSpelling =
      !expr.isMethodCall && getBuiltinArrayAccessName(expr, accessHelperName);
  setupOut.isStdNamespacedVectorAccessCall =
      hasBuiltinAccessSpelling && !expr.isMethodCall &&
      resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
  setupOut.hasStdNamespacedVectorAccessDefinition =
      setupOut.isStdNamespacedVectorAccessCall &&
      hasImportedDefinitionPath(resolveCalleePath(expr));
  setupOut.isStdNamespacedMapAccessCall =
      !expr.isMethodCall &&
      isStdNamespacedCanonicalMapAccessPath(resolveCalleePath(expr));
  setupOut.hasStdNamespacedMapAccessDefinition =
      setupOut.isStdNamespacedMapAccessCall &&
      (hasImportedDefinitionPath(resolveCalleePath(expr)) ||
       hasDeclaredDefinitionPath(resolveCalleePath(expr)));
  setupOut.shouldAllowStdAccessCompatibilityFallback = false;
  if (setupOut.isStdNamespacedVectorAccessCall && expr.args.size() == 2) {
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
    if (receiverIndex < expr.args.size()) {
      const Expr &receiverExpr = expr.args[receiverIndex];
      std::string elemType;
      const bool isNonVectorCompatibilityReceiver =
          (dispatchResolvers.resolveArgsPackAccessTarget != nullptr &&
           dispatchResolvers.resolveArgsPackAccessTarget(receiverExpr, elemType)) ||
          (dispatchResolvers.resolveArrayTarget != nullptr &&
           dispatchResolvers.resolveArrayTarget(receiverExpr, elemType)) ||
          (dispatchResolvers.resolveStringTarget != nullptr &&
           dispatchResolvers.resolveStringTarget(receiverExpr));
      if (isNonVectorCompatibilityReceiver) {
        std::string ignoredElemType;
        const bool isDirectVectorReceiver =
            (dispatchResolvers.resolveVectorTarget != nullptr &&
             dispatchResolvers.resolveVectorTarget(receiverExpr, ignoredElemType)) ||
            (dispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
             dispatchResolvers.resolveExperimentalVectorValueTarget(receiverExpr, ignoredElemType));
        setupOut.shouldAllowStdAccessCompatibilityFallback =
            !isDirectVectorReceiver;
      }
    }
  }
  setupOut.isDirectStdNamespacedVectorCountWrapperMapTarget =
      callsStdNamespacedVectorCountHelper &&
      expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call &&
      resolveMapTarget(expr.args.front());

  const bool allowStdNamespacedVectorUserReceiverProbe =
      !expr.isMethodCall && hasNamedArguments(expr.argNames) && expr.args.size() == 1 &&
      setupOut.isNamespacedVectorHelperCall &&
      (setupOut.namespacedHelper == "count" || setupOut.namespacedHelper == "capacity");
  const std::string stdNamespacedVectorCountDiagnosticMessage =
      allowStdNamespacedVectorUserReceiverProbe
          ? ""
          : classifyStdNamespacedVectorCountDiagnosticMessage(
                isInvisibleStdNamespacedVectorCompatibilityDirectCall(
                    expr.isMethodCall,
                    resolveCalleePath(expr),
                    "count",
                    hasVisibleCanonicalVectorHelperPath("/std/collections/vector/count")),
                false,
                false,
                false,
                false);
  if (!stdNamespacedVectorCountDiagnosticMessage.empty()) {
    return failCollectionDispatchDiagnostic(
        std::move(stdNamespacedVectorCountDiagnosticMessage));
  }
  if (callsInvisibleStdNamespacedVectorCapacityHelper &&
      !allowStdNamespacedVectorUserReceiverProbe) {
    return failCollectionDispatchDiagnostic(
        vectorCompatibilityUnknownCallTargetDiagnostic("capacity"));
  }
  if (!expr.isMethodCall && expr.args.size() > 1 && !hasNamedArguments(expr.argNames) &&
      (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_ref") ||
       isSimpleCallName(expr, "at_unsafe") ||
       isSimpleCallName(expr, "at_unsafe_ref")) &&
      isCanonicalMapAccessHelperName(expr.name) &&
      defMap_.find("/" + expr.name) == defMap_.end()) {
    std::string shadowedReceiverPath;
    if (this->resolveDirectCallTemporaryAccessReceiverPath(expr.args.front(), expr.name, shadowedReceiverPath) ||
        this->resolveLeadingNonCollectionAccessReceiverPath(
            params,
            locals,
            expr.args.front(),
            expr.name,
            dispatchResolvers,
            shadowedReceiverPath)) {
      return failCollectionDispatchDiagnostic("unknown method: " + shadowedReceiverPath);
    }
  }

  return true;
}

} // namespace primec::semantics

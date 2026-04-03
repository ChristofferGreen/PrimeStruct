#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprTryBuiltin(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprTryBuiltinContext &context,
    bool &handledOut) {
  auto failTryDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  handledOut = false;

  if (expr.isMethodCall || !isSimpleCallName(expr, "try")) {
    return true;
  }

  handledOut = true;
  if (hasNamedArguments(expr.argNames)) {
    return failTryDiagnostic("named arguments not supported for builtin calls");
  }
  if (!expr.templateArgs.empty()) {
    return failTryDiagnostic("try does not accept template arguments");
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return failTryDiagnostic("try does not accept block arguments");
  }
  if (expr.args.size() != 1) {
    return failTryDiagnostic("try requires exactly one argument");
  }
  if (expr.args.front().kind == Expr::Kind::Call) {
    const Expr &tryTargetExpr = expr.args.front();
    const std::string removedMapCompatibilityPath =
        context.getDirectMapHelperCompatibilityPath != nullptr
            ? context.getDirectMapHelperCompatibilityPath(tryTargetExpr)
            : std::string{};
    if (!removedMapCompatibilityPath.empty()) {
      return failTryDiagnostic("unknown call target: " + removedMapCompatibilityPath);
    }
    const std::string tryTargetPath = resolveCalleePath(tryTargetExpr);
    bool isBareMapTryAtFallback = false;
    if (isSimpleCallName(tryTargetExpr, "tryAt") &&
        defMap_.find("/tryAt") == defMap_.end() &&
        tryTargetExpr.args.size() == 2) {
      for (const auto &tryArg : tryTargetExpr.args) {
        std::string inferredTypeText;
        if (inferQueryExprTypeText(tryArg, params, locals, inferredTypeText) &&
            returnsMapCollectionType(inferredTypeText)) {
          isBareMapTryAtFallback = true;
          break;
        }
      }
    }
    const bool allowCurrentMapWrapperTryAt =
        shouldBuiltinValidateCurrentMapWrapperHelper("tryAt") ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at") ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe");
    if ((tryTargetPath == "/std/collections/map/tryAt" || isBareMapTryAtFallback) &&
        !allowCurrentMapWrapperTryAt &&
        !hasImportedDefinitionPath("/std/collections/map/tryAt") &&
        !hasDeclaredDefinitionPath("/std/collections/map/tryAt") &&
        !hasDeclaredDefinitionPath("/tryAt") &&
        !(context.isIndexedArgsPackMapReceiverTarget != nullptr &&
          !tryTargetExpr.args.empty() &&
          context.isIndexedArgsPackMapReceiverTarget(tryTargetExpr.args.front()))) {
      return failTryDiagnostic("unknown call target: /std/collections/map/tryAt");
    }
  }

  ReturnKind enclosingReturnKind = ReturnKind::Unknown;
  if (!currentValidationContext_.definitionPath.empty()) {
    auto enclosingReturnIt = returnKinds_.find(currentValidationContext_.definitionPath);
    if (enclosingReturnIt != returnKinds_.end()) {
      enclosingReturnKind = enclosingReturnIt->second;
    }
  }

  const bool returnsResult =
      currentValidationContext_.resultType.has_value() &&
      currentValidationContext_.resultType->isResult;
  if (!currentValidationContext_.onError.has_value()) {
    return failTryDiagnostic("missing on_error for ? usage");
  }
  if (!returnsResult && enclosingReturnKind != ReturnKind::Int) {
    return failTryDiagnostic("try requires Result or int return type");
  }
  if (returnsResult &&
      !errorTypesMatch(currentValidationContext_.resultType->errorType,
                       currentValidationContext_.onError->errorType,
                       expr.namespacePrefix)) {
    return failTryDiagnostic("on_error error type mismatch");
  }
  if (!validateExpr(params, locals, expr.args.front())) {
    return false;
  }
  ResultTypeInfo argResult;
  if (!resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) ||
      !argResult.isResult) {
    return failTryDiagnostic("try requires Result argument");
  }
  if (currentValidationContext_.onError.has_value() &&
      !errorTypesMatch(argResult.errorType, currentValidationContext_.onError->errorType,
                       expr.namespacePrefix)) {
    return failTryDiagnostic("try error type mismatch");
  }

  return true;
}

} // namespace primec::semantics

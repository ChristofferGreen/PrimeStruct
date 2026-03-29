#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprTryBuiltin(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprTryBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;

  if (expr.isMethodCall || !isSimpleCallName(expr, "try")) {
    return true;
  }

  handledOut = true;
  if (hasNamedArguments(expr.argNames)) {
    error_ = "named arguments not supported for builtin calls";
    return false;
  }
  if (!expr.templateArgs.empty()) {
    error_ = "try does not accept template arguments";
    return false;
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    error_ = "try does not accept block arguments";
    return false;
  }
  if (expr.args.size() != 1) {
    error_ = "try requires exactly one argument";
    return false;
  }
  if (expr.args.front().kind == Expr::Kind::Call) {
    const Expr &tryTargetExpr = expr.args.front();
    const std::string removedMapCompatibilityPath =
        context.getDirectMapHelperCompatibilityPath != nullptr
            ? context.getDirectMapHelperCompatibilityPath(tryTargetExpr)
            : std::string{};
    if (!removedMapCompatibilityPath.empty()) {
      error_ = "unknown call target: " + removedMapCompatibilityPath;
      return false;
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
      error_ = "unknown call target: /std/collections/map/tryAt";
      return false;
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
    error_ = "missing on_error for ? usage";
    return false;
  }
  if (!returnsResult && enclosingReturnKind != ReturnKind::Int) {
    error_ = "try requires Result or int return type";
    return false;
  }
  if (returnsResult &&
      !errorTypesMatch(currentValidationContext_.resultType->errorType,
                       currentValidationContext_.onError->errorType,
                       expr.namespacePrefix)) {
    error_ = "on_error error type mismatch";
    return false;
  }
  if (!validateExpr(params, locals, expr.args.front())) {
    return false;
  }
  ResultTypeInfo argResult;
  if (!resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) ||
      !argResult.isResult) {
    error_ = "try requires Result argument";
    return false;
  }
  if (currentValidationContext_.onError.has_value() &&
      !errorTypesMatch(argResult.errorType, currentValidationContext_.onError->errorType,
                       expr.namespacePrefix)) {
    error_ = "try error type mismatch";
    return false;
  }

  return true;
}

} // namespace primec::semantics

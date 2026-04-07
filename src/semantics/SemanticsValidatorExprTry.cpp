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
        shouldBuiltinValidateCurrentMapWrapperHelper("at_ref") ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe") ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe_ref");
    if ((tryTargetPath == "/std/collections/map/tryAt" ||
         tryTargetPath == "/std/collections/map/tryAt_ref" ||
         isBareMapTryAtFallback) &&
        !allowCurrentMapWrapperTryAt &&
        !hasImportedDefinitionPath(tryTargetPath == "/std/collections/map/tryAt_ref"
                                       ? "/std/collections/map/tryAt_ref"
                                       : "/std/collections/map/tryAt") &&
        !hasDeclaredDefinitionPath(tryTargetPath == "/std/collections/map/tryAt_ref"
                                       ? "/std/collections/map/tryAt_ref"
                                       : "/std/collections/map/tryAt") &&
        !hasDeclaredDefinitionPath("/tryAt") &&
        !(context.isIndexedArgsPackMapReceiverTarget != nullptr &&
          !tryTargetExpr.args.empty() &&
          context.isIndexedArgsPackMapReceiverTarget(tryTargetExpr.args.front()))) {
      return failTryDiagnostic(
          "unknown call target: " +
          std::string(tryTargetPath == "/std/collections/map/tryAt_ref"
                          ? "/std/collections/map/tryAt_ref"
                          : "/std/collections/map/tryAt"));
    }
  }

  ReturnKind enclosingReturnKind = ReturnKind::Unknown;
  if (!currentValidationState_.context.definitionPath.empty()) {
    auto enclosingReturnIt = returnKinds_.find(currentValidationState_.context.definitionPath);
    if (enclosingReturnIt != returnKinds_.end()) {
      enclosingReturnKind = enclosingReturnIt->second;
    }
  }

  const bool returnsResult =
      currentValidationState_.context.resultType.has_value() &&
      currentValidationState_.context.resultType->isResult;
  if (!currentValidationState_.context.onError.has_value()) {
    return failTryDiagnostic("missing on_error for ? usage");
  }
  if (!returnsResult && enclosingReturnKind != ReturnKind::Int) {
    return failTryDiagnostic("try requires Result or int return type");
  }
  if (returnsResult &&
      !errorTypesMatch(currentValidationState_.context.resultType->errorType,
                       currentValidationState_.context.onError->errorType,
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
  if (currentValidationState_.context.onError.has_value() &&
      !errorTypesMatch(argResult.errorType, currentValidationState_.context.onError->errorType,
                       expr.namespacePrefix)) {
    return failTryDiagnostic("try error type mismatch");
  }

  return true;
}

} // namespace primec::semantics

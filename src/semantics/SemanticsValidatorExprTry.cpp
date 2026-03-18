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
    const std::string removedMapCompatibilityPath =
        context.getDirectMapHelperCompatibilityPath != nullptr
            ? context.getDirectMapHelperCompatibilityPath(expr.args.front())
            : std::string{};
    if (!removedMapCompatibilityPath.empty()) {
      error_ = "unknown call target: " + removedMapCompatibilityPath;
      return false;
    }
    const std::string tryTargetPath = resolveCalleePath(expr.args.front());
    if (tryTargetPath == "/std/collections/map/tryAt" &&
        defMap_.find(tryTargetPath) == defMap_.end() &&
        !(context.isIndexedArgsPackMapReceiverTarget != nullptr &&
          !expr.args.front().args.empty() &&
          context.isIndexedArgsPackMapReceiverTarget(expr.args.front().args.front()))) {
      error_ = "unknown call target: /std/collections/map/tryAt";
      return false;
    }
  }

  ReturnKind enclosingReturnKind = ReturnKind::Unknown;
  if (!currentDefinitionPath_.empty()) {
    auto enclosingReturnIt = returnKinds_.find(currentDefinitionPath_);
    if (enclosingReturnIt != returnKinds_.end()) {
      enclosingReturnKind = enclosingReturnIt->second;
    }
  }

  const bool returnsResult = currentResultType_.has_value() && currentResultType_->isResult;
  if (!currentOnError_.has_value()) {
    error_ = "missing on_error for ? usage";
    return false;
  }
  if (!returnsResult && enclosingReturnKind != ReturnKind::Int) {
    error_ = "try requires Result or int return type";
    return false;
  }
  if (returnsResult &&
      !errorTypesMatch(currentResultType_->errorType, currentOnError_->errorType,
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
  if (currentOnError_.has_value() &&
      !errorTypesMatch(argResult.errorType, currentOnError_->errorType,
                       expr.namespacePrefix)) {
    error_ = "try error type mismatch";
    return false;
  }

  return true;
}

} // namespace primec::semantics

#include "SemanticsValidator.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprResolvedStructConstructorCall(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprResolvedStructConstructorContext &context,
    bool &handledOut) {
  handledOut = false;
  if (!context.isResolvedStructConstructorCall ||
      context.resolvedDefinition == nullptr ||
      context.argumentValidationContext == nullptr ||
      context.diagnosticResolved == nullptr ||
      context.zeroArgDiagnostic == nullptr) {
    return true;
  }
  auto failStructConstructorDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  if (!context.zeroArgDiagnostic->empty()) {
    return failStructConstructorDiagnostic(*context.zeroArgDiagnostic);
  }

  std::vector<ParameterInfo> fieldParams;
  fieldParams.reserve(context.resolvedDefinition->statements.size());
  auto isStaticField = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };

  bool hasMissingDefaults = false;
  for (const auto &stmt : context.resolvedDefinition->statements) {
    if (!stmt.isBinding) {
      return failStructConstructorDiagnostic(
          "struct definitions may only contain field bindings: " + resolved);
    }
    if (isStaticField(stmt)) {
      continue;
    }
    ParameterInfo field;
    field.name = stmt.name;
    if (!SemanticsValidator::resolveStructFieldBinding(*context.resolvedDefinition,
                                                       stmt, field.binding)) {
      return false;
    }
    if (stmt.args.size() == 1) {
      field.defaultExpr = &stmt.args.front();
    } else {
      hasMissingDefaults = true;
    }
    fieldParams.push_back(field);
  }

  if (hasMissingDefaults && expr.args.empty() && !hasNamedArguments(expr.argNames)) {
    if (hasStructZeroArgConstructor(resolved)) {
      handledOut = true;
      return true;
    }
  }

  if (!validateNamedArgumentsAgainstParams(fieldParams, expr.argNames, error_)) {
    if (error_.find("argument count mismatch") != std::string::npos) {
      return failStructConstructorDiagnostic("argument count mismatch for " +
                                            *context.diagnosticResolved);
    }
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  }

  std::vector<const Expr *> orderedArgs;
  std::string orderError;
  if (!buildOrderedArguments(fieldParams, expr.args, expr.argNames, orderedArgs,
                             orderError)) {
    if (orderError.find("argument count mismatch") != std::string::npos) {
      return failStructConstructorDiagnostic("argument count mismatch for " +
                                            *context.diagnosticResolved);
    } else {
      return failStructConstructorDiagnostic(orderError);
    }
  }

  std::unordered_set<const Expr *> explicitArgs;
  explicitArgs.reserve(expr.args.size());
  for (const auto &arg : expr.args) {
    explicitArgs.insert(&arg);
  }

  for (const auto *arg : orderedArgs) {
    if (!arg || explicitArgs.count(arg) == 0) {
      continue;
    }
    if (!validateExpr(params, locals, *arg)) {
      return false;
    }
  }

  for (size_t argIndex = 0;
       argIndex < orderedArgs.size() && argIndex < fieldParams.size();
       ++argIndex) {
    const Expr *arg = orderedArgs[argIndex];
    if (arg == nullptr || explicitArgs.count(arg) == 0) {
      continue;
    }
    const ParameterInfo &fieldParam = fieldParams[argIndex];
    const std::string expectedTypeText =
        this->expectedBindingTypeText(fieldParam.binding);
    if (!this->validateArgumentTypeAgainstParam(
            *arg, fieldParam, fieldParam.binding.typeName, expectedTypeText,
            *context.argumentValidationContext)) {
      return false;
    }
  }

  handledOut = true;
  return true;
}

} // namespace primec::semantics

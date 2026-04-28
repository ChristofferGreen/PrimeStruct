#include "SemanticsValidator.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

bool isConstructorBlockArgument(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.name == "block" &&
         expr.hasBodyArguments && expr.args.empty() &&
         expr.templateArgs.empty() && !hasNamedArguments(expr.argNames);
}

bool normalizeConstructorBlockArgument(
    const Expr &expr,
    std::vector<Expr> &normalizedArgs,
    std::vector<std::optional<std::string>> &normalizedArgNames,
    const std::vector<Expr> *&constructorArgs,
    const std::vector<std::optional<std::string>> *&constructorArgNames) {
  if (expr.args.size() != 1 || expr.argNames.size() != 1 ||
      expr.argNames.front().has_value()) {
    return false;
  }
  const Expr &blockArg = expr.args.front();
  if (!isConstructorBlockArgument(blockArg)) {
    return false;
  }
  for (const Expr &bodyArg : blockArg.bodyArguments) {
    if (bodyArg.isBinding) {
      return false;
    }
  }
  normalizedArgs = blockArg.bodyArguments;
  normalizedArgNames.assign(normalizedArgs.size(), std::nullopt);
  constructorArgs = &normalizedArgs;
  constructorArgNames = &normalizedArgNames;
  return true;
}

} // namespace

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
  if (!expr.isBraceConstructor && hasNamedArguments(expr.argNames)) {
    return failStructConstructorDiagnostic(
        "struct construction requires braces: " + *context.diagnosticResolved);
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

  std::vector<Expr> normalizedBraceArgs;
  std::vector<std::optional<std::string>> normalizedBraceArgNames;
  const std::vector<Expr> *constructorArgs = &expr.args;
  const std::vector<std::optional<std::string>> *constructorArgNames =
      &expr.argNames;
  (void)normalizeConstructorBlockArgument(expr,
                                          normalizedBraceArgs,
                                          normalizedBraceArgNames,
                                          constructorArgs,
                                          constructorArgNames);

  if (hasMissingDefaults && constructorArgs->empty() &&
      !hasNamedArguments(*constructorArgNames)) {
    if (hasStructZeroArgConstructor(resolved)) {
      handledOut = true;
      return true;
    }
  }

  if (!validateNamedArgumentsAgainstParams(fieldParams, *constructorArgNames, error_)) {
    if (error_.find("argument count mismatch") != std::string::npos) {
      return failStructConstructorDiagnostic("argument count mismatch for " +
                                            *context.diagnosticResolved);
    }
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  }

  std::vector<const Expr *> orderedArgs;
  std::string orderError;
  if (!buildOrderedArguments(fieldParams, *constructorArgs, *constructorArgNames, orderedArgs,
                             orderError)) {
    if (orderError.find("argument count mismatch") != std::string::npos) {
      return failStructConstructorDiagnostic("argument count mismatch for " +
                                            *context.diagnosticResolved);
    } else {
      return failStructConstructorDiagnostic(orderError);
    }
  }

  std::unordered_set<const Expr *> explicitArgs;
  explicitArgs.reserve(constructorArgs->size());
  for (const auto &arg : *constructorArgs) {
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

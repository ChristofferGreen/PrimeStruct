#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprResolvedCallArguments(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprResolvedCallArgumentContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.calleeParams == nullptr ||
      context.argumentValidationContext == nullptr ||
      context.diagnosticResolved == nullptr) {
    return true;
  }

  Expr reorderedCallExpr;
  Expr trimmedTypeNamespaceCallExpr;
  const std::vector<Expr> *orderedCallArgs = &expr.args;
  const std::vector<std::optional<std::string>> *orderedCallArgNames =
      &expr.argNames;
  if (this->isTypeNamespaceMethodCall(params, locals, expr, resolved)) {
    trimmedTypeNamespaceCallExpr = expr;
    trimmedTypeNamespaceCallExpr.args.erase(
        trimmedTypeNamespaceCallExpr.args.begin());
    if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
      trimmedTypeNamespaceCallExpr.argNames.erase(
          trimmedTypeNamespaceCallExpr.argNames.begin());
    }
    orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
    orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
  } else if (context.hasMethodReceiverIndex &&
             context.methodReceiverIndex > 0 &&
             context.methodReceiverIndex < expr.args.size()) {
    reorderedCallExpr = expr;
    std::swap(reorderedCallExpr.args[0],
              reorderedCallExpr.args[context.methodReceiverIndex]);
    if (reorderedCallExpr.argNames.size() < reorderedCallExpr.args.size()) {
      reorderedCallExpr.argNames.resize(reorderedCallExpr.args.size());
    }
    std::swap(reorderedCallExpr.argNames[0],
              reorderedCallExpr.argNames[context.methodReceiverIndex]);
    orderedCallArgs = &reorderedCallExpr.args;
    orderedCallArgNames = &reorderedCallExpr.argNames;
  }

  const auto &calleeParams = *context.calleeParams;
  if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames,
                                           error_)) {
    if (error_.find("argument count mismatch") != std::string::npos) {
      error_ = "argument count mismatch for " + *context.diagnosticResolved;
    }
    return false;
  }

  std::vector<const Expr *> orderedArgs;
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = calleeParams.size();
  std::string orderError;
  if (!buildOrderedArguments(calleeParams, *orderedCallArgs, *orderedCallArgNames,
                             orderedArgs, packedArgs, packedParamIndex,
                             orderError)) {
    if (orderError.find("argument count mismatch") != std::string::npos) {
      error_ = "argument count mismatch for " + *context.diagnosticResolved;
    } else {
      error_ = orderError;
    }
    return false;
  }

  for (const auto *arg : orderedArgs) {
    if (!arg) {
      continue;
    }
    if (!validateExpr(params, locals, *arg)) {
      return false;
    }
  }
  for (const auto *arg : packedArgs) {
    if (!arg) {
      continue;
    }
    if (!validateExpr(params, locals, *arg)) {
      return false;
    }
  }

  for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
    const ParameterInfo &param = calleeParams[paramIndex];
    if (paramIndex == packedParamIndex) {
      std::string packElementTypeText;
      if (!getArgsPackElementType(param.binding, packElementTypeText)) {
        continue;
      }
      std::string packElementTypeName = packElementTypeText;
      std::string packBase;
      std::string packArgs;
      if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
        packElementTypeName = packBase;
      }
      if (!this->validateArgumentsForParameter(
              param, packElementTypeName, packElementTypeText, packedArgs,
              *context.argumentValidationContext)) {
        return false;
      }
      continue;
    }
    const Expr *arg =
        paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
    if (arg == nullptr) {
      continue;
    }
    if (arg == param.defaultExpr) {
      continue;
    }
    const std::string &expectedTypeName = param.binding.typeName;
    const std::string expectedTypeText =
        this->expectedBindingTypeText(param.binding);
    if (!this->validateArgumentTypeAgainstParam(
            *arg, param, expectedTypeName, expectedTypeText,
            *context.argumentValidationContext)) {
      return false;
    }
  }

  auto isReferenceTypeText = [](const std::string &typeName,
                                const std::string &typeText) {
    if (normalizeBindingTypeName(typeName) == "Reference") {
      return true;
    }
    std::string base;
    std::string argText;
    return splitTemplateTypeName(typeText, base, argText) &&
           normalizeBindingTypeName(base) == "Reference";
  };

  bool calleeIsUnsafe = false;
  if (context.resolvedDefinition != nullptr) {
    for (const auto &transform : context.resolvedDefinition->transforms) {
      if (transform.name == "unsafe") {
        calleeIsUnsafe = true;
        break;
      }
    }
  }

  if (currentValidationContext_.definitionIsUnsafe && !calleeIsUnsafe) {
    for (size_t i = 0; i < calleeParams.size(); ++i) {
      const ParameterInfo &param = calleeParams[i];
      if (i == packedParamIndex) {
        std::string packElementTypeText;
        if (!getArgsPackElementType(param.binding, packElementTypeText)) {
          continue;
        }
        std::string packElementTypeName = packElementTypeText;
        std::string packBase;
        std::string packArgs;
        if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
          packElementTypeName = packBase;
        }
        if (!isReferenceTypeText(packElementTypeName, packElementTypeText)) {
          continue;
        }
        for (const Expr *arg : packedArgs) {
          if (!arg || !isUnsafeReferenceExpr(params, locals, *arg)) {
            continue;
          }
          error_ = "unsafe reference escapes across safe boundary to " + resolved;
          return false;
        }
        continue;
      }

      const Expr *arg = i < orderedArgs.size() ? orderedArgs[i] : nullptr;
      if (arg == nullptr) {
        continue;
      }
      const std::string expectedTypeText =
          param.binding.typeTemplateArg.empty()
              ? param.binding.typeName
              : param.binding.typeName + "<" + param.binding.typeTemplateArg +
                    ">";
      if (!isReferenceTypeText(param.binding.typeName, expectedTypeText)) {
        continue;
      }
      if (!isUnsafeReferenceExpr(params, locals, *arg)) {
        continue;
      }
      error_ = "unsafe reference escapes across safe boundary to " + resolved;
      return false;
    }
  }

  handledOut = true;
  return true;
}

} // namespace primec::semantics

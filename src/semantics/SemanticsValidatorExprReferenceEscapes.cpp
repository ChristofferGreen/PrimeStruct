#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::isUnsafeReferenceExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr) {
  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      return paramBinding->typeName == "Reference" &&
             paramBinding->isUnsafeReference;
    }
    auto itLocal = locals.find(expr.name);
    return itLocal != locals.end() &&
           itLocal->second.typeName == "Reference" &&
           itLocal->second.isUnsafeReference;
  }
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return false;
  }

  auto hasUnsafeChildExpr = [&](const Expr &callExpr) {
    for (const auto &nestedArg : callExpr.args) {
      if (isUnsafeReferenceExpr(params, locals, nestedArg)) {
        return true;
      }
    }
    for (const auto &bodyExpr : callExpr.bodyArguments) {
      if (isUnsafeReferenceExpr(params, locals, bodyExpr)) {
        return true;
      }
    }
    return false;
  };
  if (isIfCall(expr) || isMatchCall(expr) || isPickCall(expr) || isBlockCall(expr) ||
      isReturnCall(expr) || isSimpleCallName(expr, "then") ||
      isSimpleCallName(expr, "else") || isSimpleCallName(expr, "case")) {
    return hasUnsafeChildExpr(expr);
  }

  const std::string nestedResolved = resolveCalleePath(expr);
  if (nestedResolved.empty()) {
    return false;
  }
  auto nestedIt = defMap_.find(nestedResolved);
  if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
    return false;
  }

  bool returnsReference = false;
  for (const auto &transform : nestedIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) &&
        base == "Reference") {
      returnsReference = true;
      break;
    }
  }
  if (!returnsReference) {
    return false;
  }

  const auto &nestedParams = paramsByDef_[nestedResolved];
  if (nestedParams.empty()) {
    return false;
  }
  std::string nestedArgError;
  if (!validateNamedArgumentsAgainstParams(nestedParams, expr.argNames,
                                           nestedArgError)) {
    return false;
  }
  std::vector<const Expr *> nestedOrderedArgs;
  if (!buildOrderedArguments(nestedParams, expr.args, expr.argNames,
                             nestedOrderedArgs, nestedArgError)) {
    return false;
  }
  for (size_t nestedIndex = 0;
       nestedIndex < nestedOrderedArgs.size() &&
       nestedIndex < nestedParams.size();
       ++nestedIndex) {
    const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
    if (nestedArg == nullptr ||
        nestedParams[nestedIndex].binding.typeName != "Reference") {
      continue;
    }
    if (isUnsafeReferenceExpr(params, locals, *nestedArg)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveEscapingReferenceRoot(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &rootOut) {
  rootOut.clear();
  if (expr.kind == Expr::Kind::Name) {
    if (findParamBinding(params, expr.name) != nullptr) {
      return false;
    }
    auto itLocal = locals.find(expr.name);
    if (itLocal == locals.end() || itLocal->second.typeName != "Reference") {
      return false;
    }
    std::string sourceRoot = itLocal->second.referenceRoot.empty()
                                 ? expr.name
                                 : itLocal->second.referenceRoot;
    if (const BindingInfo *rootParam = findParamBinding(params, sourceRoot)) {
      if (rootParam->typeName == "Reference") {
        return false;
      }
    }
    rootOut = sourceRoot;
    return true;
  }
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return false;
  }

  auto resolveChildRoot = [&](const Expr &callExpr) {
    for (const auto &nestedArg : callExpr.args) {
      if (resolveEscapingReferenceRoot(params, locals, nestedArg, rootOut)) {
        return true;
      }
    }
    for (const auto &bodyExpr : callExpr.bodyArguments) {
      if (resolveEscapingReferenceRoot(params, locals, bodyExpr, rootOut)) {
        return true;
      }
    }
    return false;
  };
  if (isIfCall(expr) || isMatchCall(expr) || isPickCall(expr) || isBlockCall(expr) ||
      isReturnCall(expr) || isSimpleCallName(expr, "then") ||
      isSimpleCallName(expr, "else") || isSimpleCallName(expr, "case")) {
    return resolveChildRoot(expr);
  }

  const std::string nestedResolved = resolveCalleePath(expr);
  if (nestedResolved.empty()) {
    return false;
  }
  auto nestedIt = defMap_.find(nestedResolved);
  if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
    return false;
  }

  bool returnsReference = false;
  for (const auto &transform : nestedIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) &&
        base == "Reference") {
      returnsReference = true;
      break;
    }
  }
  if (!returnsReference) {
    return false;
  }

  const auto &nestedParams = paramsByDef_[nestedResolved];
  if (nestedParams.empty()) {
    return false;
  }
  std::string nestedArgError;
  if (!validateNamedArgumentsAgainstParams(nestedParams, expr.argNames,
                                           nestedArgError)) {
    return false;
  }
  std::vector<const Expr *> nestedOrderedArgs;
  if (!buildOrderedArguments(nestedParams, expr.args, expr.argNames,
                             nestedOrderedArgs, nestedArgError)) {
    return false;
  }
  for (size_t nestedIndex = 0;
       nestedIndex < nestedOrderedArgs.size() &&
       nestedIndex < nestedParams.size();
       ++nestedIndex) {
    const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
    if (nestedArg == nullptr ||
        nestedParams[nestedIndex].binding.typeName != "Reference") {
      continue;
    }
    if (resolveEscapingReferenceRoot(params, locals, *nestedArg, rootOut)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::reportReferenceAssignmentEscape(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &sinkName,
    const Expr &rhsExpr) {
  auto failReferenceEscapeDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(rhsExpr, std::move(message));
  };
  std::string sourceRoot;
  if (!resolveEscapingReferenceRoot(params, locals, rhsExpr, sourceRoot)) {
    return false;
  }
  if (sourceRoot.empty()) {
    sourceRoot = "<unknown>";
  }
  const std::string sink = sinkName.empty() ? "<unknown>" : sinkName;
  if (currentValidationState_.context.definitionIsUnsafe &&
      isUnsafeReferenceExpr(params, locals, rhsExpr)) {
    failReferenceEscapeDiagnostic("unsafe reference escapes via assignment to " + sink +
                                  " (root: " + sourceRoot + ", sink: " + sink + ")");
    return true;
  }
  failReferenceEscapeDiagnostic("reference escapes via assignment to " + sink +
                                " (root: " + sourceRoot + ", sink: " + sink + ")");
  return true;
}

bool SemanticsValidator::resolveReferenceEscapeSink(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &targetName,
    std::string &sinkOut) {
  sinkOut.clear();
  if (const BindingInfo *targetParam = findParamBinding(params, targetName)) {
    if (targetParam->typeName == "Reference") {
      sinkOut = targetName;
      return true;
    }
    return false;
  }
  auto targetIt = locals.find(targetName);
  if (targetIt == locals.end() || targetIt->second.typeName != "Reference" ||
      targetIt->second.referenceRoot.empty()) {
    return false;
  }
  if (const BindingInfo *rootParam =
          findParamBinding(params, targetIt->second.referenceRoot)) {
    if (rootParam->typeName == "Reference") {
      sinkOut = targetIt->second.referenceRoot;
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics

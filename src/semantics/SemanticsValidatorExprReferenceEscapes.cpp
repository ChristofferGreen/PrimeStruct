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

bool SemanticsValidator::resolveEscapingLocalPointerRoot(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &rootOut) {
  rootOut.clear();
  auto acceptLocalRoot = [&](const std::string &root) -> bool {
    const std::string base = root.substr(0, root.find('.'));
    if (base.empty() || findParamBinding(params, base) != nullptr) {
      return false;
    }
    auto localIt = locals.find(base);
    // A Pointer/Reference local holds an address; what it points at is checked through its
    // own root.
    if (localIt == locals.end() || localIt->second.typeName == "Pointer" ||
        localIt->second.typeName == "Reference") {
      return false;
    }
    rootOut = base;
    return true;
  };
  if (expr.kind == Expr::Kind::Name) {
    if (findParamBinding(params, expr.name) != nullptr) {
      return false;
    }
    auto localIt = locals.find(expr.name);
    if (localIt == locals.end() || localIt->second.typeName != "Pointer" ||
        localIt->second.referenceRoot.empty()) {
      return false;
    }
    return acceptLocalRoot(localIt->second.referenceRoot);
  }
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" &&
      expr.args.size() == 1) {
    const Expr *target = &expr.args.front();
    while (target->kind == Expr::Kind::Call && target->isFieldAccess && target->args.size() == 1) {
      target = &target->args.front();
    }
    return target->kind == Expr::Kind::Name && acceptLocalRoot(target->name);
  }
  std::string operatorName;
  if (getBuiltinOperatorName(expr, operatorName) &&
      (operatorName == "plus" || operatorName == "minus") && expr.args.size() == 2) {
    return resolveEscapingLocalPointerRoot(params, locals, expr.args.front(), rootOut);
  }
  return false;
}

bool SemanticsValidator::resolveParameterRootedAssignmentSink(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &target,
    std::string &sinkOut) {
  sinkOut.clear();
  const Expr *base = &target;
  while (base->kind == Expr::Kind::Call && base->isFieldAccess && base->args.size() == 1) {
    base = &base->args.front();
  }
  bool throughPointer = false;
  std::string builtinName;
  if (base->kind == Expr::Kind::Call && getBuiltinPointerName(*base, builtinName) &&
      builtinName == "dereference" && base->args.size() == 1) {
    base = &base->args.front();
    throughPointer = true;
  }
  if (base->kind != Expr::Kind::Name) {
    return false;
  }
  if (const BindingInfo *param = findParamBinding(params, base->name)) {
    // Writes reach the caller through a pointer, a Reference, or a `mut` borrow; an owned
    // (`copy`/`move`) parameter is the callee's own value.
    const bool reachesCaller = throughPointer || param->typeName == "Reference" ||
                               (param->isMutable && !param->isCopy && !param->isMove);
    if (!reachesCaller) {
      return false;
    }
    sinkOut = base->name;
    return true;
  }
  if (!throughPointer) {
    return false;
  }
  auto localIt = locals.find(base->name);
  if (localIt == locals.end() || localIt->second.referenceRoot.empty()) {
    return false;
  }
  const std::string rootBase =
      localIt->second.referenceRoot.substr(0, localIt->second.referenceRoot.find('.'));
  if (findParamBinding(params, rootBase) == nullptr) {
    return false;
  }
  sinkOut = rootBase;
  return true;
}

bool SemanticsValidator::reportAssignmentValueEscape(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &assignExpr) {
  const Expr &value = assignExpr.args[1];
  if (!currentValidationState_.context.definitionIsUnsafe) {
    std::string pointerSink;
    std::string localPointerRoot;
    if (resolveParameterRootedAssignmentSink(
            params, locals, assignExpr.args.front(), pointerSink) &&
        resolveEscapingLocalPointerRoot(params, locals, value, localPointerRoot)) {
      failExprDiagnostic(assignExpr,
                         "pointer escapes via assignment to " + pointerSink +
                             " (root: " + localPointerRoot + ")");
      return true;
    }
  }
  if (isOwningBorrowedParameter(params, value, assignExpr.namespacePrefix)) {
    failExprDiagnostic(assignExpr, "borrowed parameter escapes via assignment: " + value.name);
    return true;
  }
  return false;
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

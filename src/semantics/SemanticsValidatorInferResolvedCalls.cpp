#include "SemanticsValidator.h"

namespace primec::semantics {

ReturnKind SemanticsValidator::inferResolvedCallReturnKind(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const InferResolvedCallContext &context,
    bool &handled) {
  handled = false;
  if (context.collectionDispatchSetup == nullptr ||
      context.dispatchResolvers == nullptr || context.resolved.empty()) {
    return ReturnKind::Unknown;
  }

  const auto &inferCollectionDispatchSetup = *context.collectionDispatchSetup;
  const auto &builtinCollectionDispatchResolvers = *context.dispatchResolvers;
  const auto defIt = defMap_.find(context.resolved);
  if (defIt == defMap_.end() ||
      inferCollectionDispatchSetup
          .shouldDeferResolvedNamespacedCollectionHelperReturn) {
    return ReturnKind::Unknown;
  }

  handled = true;
  if (inferCollectionDispatchSetup.hasPreferredBuiltinAccessKind &&
      expr.isMethodCall) {
    return inferCollectionDispatchSetup.preferredBuiltinAccessKind;
  }

  auto isTypeNamespaceMethodCall = [&](const Expr &callExpr,
                                       const std::string &resolvedPath) -> bool {
    if (!callExpr.isMethodCall || callExpr.args.empty() || resolvedPath.empty()) {
      return false;
    }
    const Expr &receiver = callExpr.args.front();
    if (receiver.kind != Expr::Kind::Name) {
      return false;
    }
    if (findParamBinding(params, receiver.name) != nullptr ||
        locals.find(receiver.name) != locals.end()) {
      return false;
    }
    const size_t methodSlash = resolvedPath.find_last_of('/');
    if (methodSlash == std::string::npos || methodSlash == 0) {
      return false;
    }
    const std::string receiverPath = resolvedPath.substr(0, methodSlash);
    const size_t receiverSlash = receiverPath.find_last_of('/');
    const std::string receiverTypeName =
        receiverSlash == std::string::npos
            ? receiverPath
            : receiverPath.substr(receiverSlash + 1);
    return receiverTypeName == receiver.name;
  };

  if (expr.isMethodCall) {
    const auto &calleeParams = paramsByDef_[context.resolved];
    Expr trimmedTypeNamespaceCallExpr;
    const std::vector<Expr> *orderedCallArgs = &expr.args;
    const std::vector<std::optional<std::string>> *orderedCallArgNames =
        &expr.argNames;
    if (isTypeNamespaceMethodCall(expr, context.resolved)) {
      trimmedTypeNamespaceCallExpr = expr;
      trimmedTypeNamespaceCallExpr.args.erase(
          trimmedTypeNamespaceCallExpr.args.begin());
      if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
        trimmedTypeNamespaceCallExpr.argNames.erase(
            trimmedTypeNamespaceCallExpr.argNames.begin());
      }
      orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
      orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
    }

    std::string orderedArgError;
    if (!validateNamedArgumentsAgainstParams(
            calleeParams, *orderedCallArgNames, orderedArgError)) {
      error_ = orderedArgError.find("argument count mismatch") !=
                       std::string::npos
                   ? "argument count mismatch for " + context.resolved
                   : orderedArgError;
      return ReturnKind::Unknown;
    }

    std::vector<const Expr *> orderedArgs;
    if (!buildOrderedArguments(calleeParams,
                               *orderedCallArgs,
                               *orderedCallArgNames,
                               orderedArgs,
                               orderedArgError)) {
      error_ = orderedArgError.find("argument count mismatch") !=
                       std::string::npos
                   ? "argument count mismatch for " + context.resolved
                   : orderedArgError;
      return ReturnKind::Unknown;
    }

    for (size_t paramIndex = 0;
         paramIndex < orderedArgs.size() && paramIndex < calleeParams.size();
         ++paramIndex) {
      const Expr *arg = orderedArgs[paramIndex];
      if (arg == nullptr) {
        continue;
      }
      const ParameterInfo &param = calleeParams[paramIndex];
      if (param.binding.typeName.empty() || param.binding.typeName == "auto") {
        continue;
      }
      std::string expectedBase;
      std::string expectedArgText;
      if (splitTemplateTypeName(
              param.binding.typeName, expectedBase, expectedArgText) ||
          splitTemplateTypeName(
              param.binding.typeTemplateArg, expectedBase, expectedArgText)) {
        continue;
      }
      const ReturnKind expectedKind =
          returnKindForTypeName(normalizeBindingTypeName(param.binding.typeName));
      if (expectedKind == ReturnKind::Unknown) {
        continue;
      }
      const ReturnKind actualKind = inferExprReturnKind(*arg, params, locals);
      if (actualKind == ReturnKind::Unknown) {
        continue;
      }
      if (actualKind != expectedKind) {
        error_ = "argument type mismatch for " + context.resolved +
                 " parameter " + param.name;
        return ReturnKind::Unknown;
      }
    }
  }

  if (!ensureDefinitionReturnKindReady(*defIt->second)) {
    ReturnKind builtinAccessKind = ReturnKind::Unknown;
    if (resolveBuiltinCollectionAccessCallReturnKind(
            expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
      return builtinAccessKind;
    }
    ReturnKind builtinCollectionKind = ReturnKind::Unknown;
    if (resolveBuiltinCollectionCountCapacityReturnKind(
            expr,
            inferCollectionDispatchSetup
                .builtinCollectionCountCapacityDispatchContext,
            builtinCollectionKind)) {
      return builtinCollectionKind;
    }
    return ReturnKind::Unknown;
  }

  auto kindIt = returnKinds_.find(context.resolved);
  if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
    return kindIt->second;
  }

  ReturnKind builtinAccessKind = ReturnKind::Unknown;
  if (resolveBuiltinCollectionAccessCallReturnKind(
          expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
    return builtinAccessKind;
  }

  ReturnKind builtinCollectionKind = ReturnKind::Unknown;
  if (resolveBuiltinCollectionCountCapacityReturnKind(
          expr,
          inferCollectionDispatchSetup
              .builtinCollectionCountCapacityDispatchContext,
          builtinCollectionKind)) {
    return builtinCollectionKind;
  }

  return ReturnKind::Unknown;
}

} // namespace primec::semantics

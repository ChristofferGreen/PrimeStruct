#include "SemanticsValidator.h"

#include <functional>

namespace primec::semantics {

bool SemanticsValidator::inferQuerySnapshotData(const std::vector<ParameterInfo> &defParams,
                                                const std::unordered_map<std::string, BindingInfo> &activeLocals,
                                                const Expr &expr,
                                                QuerySnapshotData &out) {
  out = {};

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  CallSnapshotData callData;
  if (inferCallSnapshotData(defParams, activeLocals, expr, callData)) {
    out.resolvedPath = std::move(callData.resolvedPath);
    out.binding = std::move(callData.binding);
  }
  withPreservedError([&]() {
    return inferQueryExprTypeText(expr, defParams, activeLocals, out.typeText);
  });
  if (out.typeText.empty() && !out.binding.typeName.empty()) {
    out.typeText = out.binding.typeTemplateArg.empty()
                       ? out.binding.typeName
                       : out.binding.typeName + "<" + out.binding.typeTemplateArg + ">";
  }
  if (!(withPreservedError([&]() {
          return resolveResultTypeForExpr(expr, defParams, activeLocals, out.resultInfo);
        }) &&
        out.resultInfo.isResult)) {
    out.resultInfo = {};
  }

  const bool receiverQueryCandidate =
      expr.kind == Expr::Kind::Call &&
      !expr.args.empty() &&
      !out.resolvedPath.empty() &&
      (expr.isMethodCall ||
       out.resolvedPath.rfind("/std/collections/", 0) == 0 ||
       out.resolvedPath.rfind("/array/", 0) == 0 ||
       out.resolvedPath.rfind("/vector/", 0) == 0 ||
       out.resolvedPath.rfind("/map/", 0) == 0);
  if (receiverQueryCandidate &&
      !(withPreservedError([&]() {
          return inferBindingTypeFromInitializer(expr.args.front(), defParams, activeLocals, out.receiverBinding);
        }) &&
        !out.receiverBinding.typeName.empty())) {
    out.receiverBinding = {};
  }

  return !out.resolvedPath.empty() ||
         !out.typeText.empty() ||
         !out.binding.typeName.empty() ||
         out.resultInfo.isResult ||
         !out.receiverBinding.typeName.empty();
}

bool SemanticsValidator::inferCallSnapshotData(const std::vector<ParameterInfo> &defParams,
                                               const std::unordered_map<std::string, BindingInfo> &activeLocals,
                                               const Expr &expr,
                                               CallSnapshotData &out) {
  out = {};

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  out.resolvedPath = resolveCalleePath(expr);
  if (!out.resolvedPath.empty()) {
    BindingInfo resolvedBinding;
    if (inferResolvedDirectCallBindingType(out.resolvedPath, resolvedBinding) &&
        !resolvedBinding.typeName.empty()) {
      out.binding = std::move(resolvedBinding);
    }
  }
  if (out.binding.typeName.empty()) {
    withPreservedError([&]() {
      return inferBindingTypeFromInitializer(expr, defParams, activeLocals, out.binding);
    });
  }

  return !out.resolvedPath.empty() || !out.binding.typeName.empty();
}

bool SemanticsValidator::inferTrySnapshotData(const Definition &def,
                                              const std::vector<ParameterInfo> &defParams,
                                              const std::unordered_map<std::string, BindingInfo> &activeLocals,
                                              const Expr &expr,
                                              LocalAutoTrySnapshotData &out) {
  out = {};

  if (expr.isMethodCall || !isSimpleCallName(expr, "try") || expr.args.size() != 1 ||
      !expr.templateArgs.empty() || expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return false;
  }

  QuerySnapshotData operandQueryData;
  if (!inferQuerySnapshotData(defParams, activeLocals, expr.args.front(), operandQueryData) ||
      !operandQueryData.resultInfo.isResult ||
      !operandQueryData.resultInfo.hasValue ||
      operandQueryData.resultInfo.valueType.empty()) {
    return false;
  }

  out.operandResolvedPath = std::move(operandQueryData.resolvedPath);
  out.operandBinding = std::move(operandQueryData.binding);
  out.operandReceiverBinding = std::move(operandQueryData.receiverBinding);
  out.operandQueryTypeText = std::move(operandQueryData.typeText);
  out.valueType = std::move(operandQueryData.resultInfo.valueType);
  out.errorType = std::move(operandQueryData.resultInfo.errorType);
  if (const auto returnKindIt = returnKinds_.find(def.fullPath); returnKindIt != returnKinds_.end()) {
    out.contextReturnKind = returnKindIt->second;
  }
  const auto state = buildDefinitionValidationState(def);
  if (state.context.onError.has_value()) {
    out.onErrorHandlerPath = state.context.onError->handlerPath;
    out.onErrorErrorType = state.context.onError->errorType;
    out.onErrorBoundArgCount = state.context.onError->boundArgs.size();
  }
  return true;
}

void SemanticsValidator::forEachLocalAwareSnapshotCall(
    const std::function<void(const Definition &,
                             const std::vector<ParameterInfo> &,
                             const Expr &,
                             const std::unordered_map<std::string, BindingInfo> &)> &visitor) {
  using ActiveLocalBindings = std::unordered_map<std::string, BindingInfo>;

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  auto inferBindingForLocals = [&](const Definition &def,
                                   const std::vector<ParameterInfo> &defParams,
                                   const ActiveLocalBindings &activeLocals,
                                   const Expr &bindingExpr,
                                   BindingInfo &bindingOut) {
    const std::string namespacePrefix =
        bindingExpr.namespacePrefix.empty() ? def.namespacePrefix : bindingExpr.namespacePrefix;
    std::optional<std::string> restrictType;
    if (!withPreservedError([&]() {
          return parseBindingInfo(
              bindingExpr, namespacePrefix, structNames_, importAliases_, bindingOut, restrictType, error_);
        })) {
      return false;
    }
    const bool hasExplicitType = hasExplicitBindingTypeTransform(bindingExpr);
    const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(bindingOut.typeName) == "auto";
    if (bindingExpr.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
      BindingInfo inferred = bindingOut;
      if (withPreservedError([&]() {
            return inferBindingTypeFromInitializer(
                bindingExpr.args.front(), defParams, activeLocals, inferred, &bindingExpr);
          })) {
        bindingOut = std::move(inferred);
      }
    }
    if (!restrictType.has_value()) {
      return true;
    }
    const bool hasTemplate = !bindingOut.typeTemplateArg.empty();
    return restrictMatchesBinding(
        *restrictType, bindingOut.typeName, bindingOut.typeTemplateArg, hasTemplate, namespacePrefix);
  };

  std::function<void(const Definition &, const std::vector<ParameterInfo> &, const Expr &, ActiveLocalBindings &)>
      visitExpr;
  std::function<void(const Definition &,
                     const std::vector<ParameterInfo> &,
                     const std::vector<Expr> &,
                     ActiveLocalBindings &)>
      visitExprSequence;

  visitExprSequence = [&](const Definition &def,
                          const std::vector<ParameterInfo> &defParams,
                          const std::vector<Expr> &exprs,
                          ActiveLocalBindings &activeLocals) {
    for (const auto &expr : exprs) {
      visitExpr(def, defParams, expr, activeLocals);
    }
  };

  visitExpr = [&](const Definition &def,
                  const std::vector<ParameterInfo> &defParams,
                  const Expr &expr,
                  ActiveLocalBindings &activeLocals) {
    if (expr.isBinding) {
      for (const auto &arg : expr.args) {
        ActiveLocalBindings argLocals = activeLocals;
        visitExpr(def, defParams, arg, argLocals);
      }
      if (!expr.bodyArguments.empty()) {
        ActiveLocalBindings bodyLocals = activeLocals;
        visitExprSequence(def, defParams, expr.bodyArguments, bodyLocals);
      }

      BindingInfo binding;
      if (inferBindingForLocals(def, defParams, activeLocals, expr, binding)) {
        activeLocals.emplace(expr.name, std::move(binding));
      }
      return;
    }

    if (expr.kind == Expr::Kind::Call) {
      visitor(def, defParams, expr, activeLocals);
    }

    for (const auto &arg : expr.args) {
      ActiveLocalBindings argLocals = activeLocals;
      visitExpr(def, defParams, arg, argLocals);
    }
    if (!expr.bodyArguments.empty()) {
      ActiveLocalBindings bodyLocals = activeLocals;
      visitExprSequence(def, defParams, expr.bodyArguments, bodyLocals);
    }
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      continue;
    }
    const auto &defParams = paramsIt->second;

    for (const auto &param : def.parameters) {
      for (const auto &arg : param.args) {
        ActiveLocalBindings paramLocals;
        visitExpr(def, defParams, arg, paramLocals);
      }
      if (!param.bodyArguments.empty()) {
        ActiveLocalBindings paramLocals;
        visitExprSequence(def, defParams, param.bodyArguments, paramLocals);
      }
    }

    ActiveLocalBindings definitionLocals;
    visitExprSequence(def, defParams, def.statements, definitionLocals);
    if (def.returnExpr.has_value()) {
      ActiveLocalBindings returnLocals = definitionLocals;
      visitExpr(def, defParams, *def.returnExpr, returnLocals);
    }
  }
}

void SemanticsValidator::forEachInferredQuerySnapshot(
    const std::function<void(const Definition &, const Expr &, QuerySnapshotData &&)> &visitor) {
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    QuerySnapshotData queryData;
    if (!inferQuerySnapshotData(defParams, activeLocals, expr, queryData)) {
      return;
    }
    visitor(def, expr, std::move(queryData));
  });
}

SemanticsValidator::GraphLocalAutoKey SemanticsValidator::graphLocalAutoBindingKey(const std::string &scopePath,
                                                                                    int sourceLine,
                                                                                    int sourceColumn) {
  return GraphLocalAutoKey{scopePath, sourceLine, sourceColumn};
}

std::pair<int, int> SemanticsValidator::graphLocalAutoSourceLocation(const Expr &expr) {
  if (expr.sourceLine > 0 && expr.sourceColumn > 0) {
    return {expr.sourceLine, expr.sourceColumn};
  }
  if (!expr.args.empty() && expr.args.front().sourceLine > 0 && expr.args.front().sourceColumn > 0) {
    return {expr.args.front().sourceLine, expr.args.front().sourceColumn};
  }
  return {expr.sourceLine, expr.sourceColumn};
}

} // namespace primec::semantics

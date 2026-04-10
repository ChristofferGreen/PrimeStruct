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

void SemanticsValidator::ensureQuerySnapshotFactCaches() {
  if (querySnapshotFactCacheValid_) {
    return;
  }

  queryFactSnapshotCache_.clear();
  queryReceiverBindingSnapshotCache_.clear();
  queryCallTypeSnapshotCache_.clear();
  queryBindingSnapshotCache_.clear();
  queryResultTypeSnapshotCache_.clear();
  forEachInferredQuerySnapshot([&](const Definition &def, const Expr &expr, QuerySnapshotData &&queryData) {
    QueryFactSnapshotEntry factEntry{
        def.fullPath,
        expr.name,
        queryData.resolvedPath,
        expr.sourceLine,
        expr.sourceColumn,
        queryData.typeText,
        queryData.binding,
        queryData.receiverBinding,
        queryData.resultInfo.isResult,
        queryData.resultInfo.isResult && queryData.resultInfo.hasValue,
        queryData.resultInfo.valueType,
        queryData.resultInfo.errorType,
        expr.semanticNodeId,
    };
    queryFactSnapshotCache_.push_back(std::move(factEntry));

    if (!queryData.typeText.empty()) {
      queryCallTypeSnapshotCache_.push_back(QueryCallTypeSnapshotEntry{
          def.fullPath,
          expr.name,
          queryData.resolvedPath,
          expr.sourceLine,
          expr.sourceColumn,
          queryData.typeText,
      });
    }

    if (!queryData.binding.typeName.empty()) {
      queryBindingSnapshotCache_.push_back(QueryBindingSnapshotEntry{
          def.fullPath,
          expr.name,
          queryData.resolvedPath,
          expr.sourceLine,
          expr.sourceColumn,
          queryData.binding,
      });
    }

    if (queryData.resultInfo.isResult) {
      queryResultTypeSnapshotCache_.push_back(QueryResultTypeSnapshotEntry{
          def.fullPath,
          expr.name,
          queryData.resolvedPath,
          expr.sourceLine,
          expr.sourceColumn,
          queryData.resultInfo.hasValue,
          queryData.resultInfo.valueType,
          queryData.resultInfo.errorType,
      });
    }

    if (!queryData.receiverBinding.typeName.empty()) {
      queryReceiverBindingSnapshotCache_.push_back(QueryReceiverBindingSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(queryData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          std::move(queryData.receiverBinding),
      });
    }
  });

  std::stable_sort(queryFactSnapshotCache_.begin(),
                   queryFactSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     if (left.callName != right.callName) {
                       return left.callName < right.callName;
                     }
                     return left.resolvedPath < right.resolvedPath;
                   });
  std::stable_sort(queryReceiverBindingSnapshotCache_.begin(),
                   queryReceiverBindingSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     return left.callName < right.callName;
                   });
  std::stable_sort(queryCallTypeSnapshotCache_.begin(),
                   queryCallTypeSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     return left.callName < right.callName;
                   });
  std::stable_sort(queryBindingSnapshotCache_.begin(),
                   queryBindingSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     return left.callName < right.callName;
                   });
  std::stable_sort(queryResultTypeSnapshotCache_.begin(),
                   queryResultTypeSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     return left.callName < right.callName;
                   });
  querySnapshotFactCacheValid_ = true;
}

void SemanticsValidator::ensureCallAndTrySnapshotFactCaches() {
  if (callAndTrySnapshotFactCacheValid_) {
    return;
  }

  callBindingSnapshotCache_.clear();
  tryValueSnapshotCache_.clear();
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    CallSnapshotData callData;
    if (inferCallSnapshotData(defParams, activeLocals, expr, callData) &&
        !callData.resolvedPath.empty() &&
        !callData.binding.typeName.empty()) {
      callBindingSnapshotCache_.push_back(CallBindingSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(callData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          std::move(callData.binding),
      });
    }

    LocalAutoTrySnapshotData tryData;
    if (!inferTrySnapshotData(def, defParams, activeLocals, expr, tryData)) {
      return;
    }
    tryValueSnapshotCache_.push_back(TryValueSnapshotEntry{
        def.fullPath,
        std::move(tryData.operandResolvedPath),
        expr.sourceLine,
        expr.sourceColumn,
        std::move(tryData.operandBinding),
        std::move(tryData.operandReceiverBinding),
        std::move(tryData.operandQueryTypeText),
        std::move(tryData.valueType),
        std::move(tryData.errorType),
        tryData.contextReturnKind,
        std::move(tryData.onErrorHandlerPath),
        std::move(tryData.onErrorErrorType),
        tryData.onErrorBoundArgCount,
        expr.semanticNodeId,
    });
  });

  std::stable_sort(callBindingSnapshotCache_.begin(),
                   callBindingSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     return left.callName < right.callName;
                   });
  std::stable_sort(tryValueSnapshotCache_.begin(),
                   tryValueSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.scopePath != right.scopePath) {
                       return left.scopePath < right.scopePath;
                     }
                     if (left.sourceLine != right.sourceLine) {
                       return left.sourceLine < right.sourceLine;
                     }
                     if (left.sourceColumn != right.sourceColumn) {
                       return left.sourceColumn < right.sourceColumn;
                     }
                     return left.operandResolvedPath < right.operandResolvedPath;
                   });
  callAndTrySnapshotFactCacheValid_ = true;
}

SemanticsValidator::GraphLocalAutoKey SemanticsValidator::graphLocalAutoBindingKey(std::string_view scopePath,
                                                                                    int sourceLine,
                                                                                    int sourceColumn) const {
  if (benchmarkGraphLocalAutoLegacyKeyShadowEnabled_) {
    std::string legacyKey;
    legacyKey.reserve(scopePath.size() + 32);
    legacyKey.append(scopePath);
    legacyKey.push_back('#');
    legacyKey.append(std::to_string(sourceLine));
    legacyKey.push_back(':');
    legacyKey.append(std::to_string(sourceColumn));
    graphLocalAutoLegacyKeyShadow_.insert(std::move(legacyKey));
  }
  const SymbolId scopePathId = graphLocalAutoScopePathInterner_.intern(scopePath);
  return GraphLocalAutoKey{scopePathId, sourceLine, sourceColumn};
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

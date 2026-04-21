#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>

namespace primec::semantics {

bool SemanticsValidator::inferQuerySnapshotData(const std::vector<ParameterInfo> &defParams,
                                                const std::unordered_map<std::string, BindingInfo> &activeLocals,
                                                const Expr &expr,
                                                QuerySnapshotData &out) {
  out = {};

  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeName.empty()) {
      return std::string{};
    }
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };

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

  out.typeText = bindingTypeText(out.binding);
  if (out.typeText.empty()) {
    withPreservedError([&]() {
      return inferQueryExprTypeText(expr, defParams, activeLocals, out.typeText);
    });
  }

  if (!out.binding.typeName.empty()) {
    if (!resolveResultTypeFromTypeName(out.typeText, out.resultInfo) ||
        !out.resultInfo.isResult) {
      out.resultInfo = {};
    }
  } else if (!(withPreservedError([&]() {
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
       out.resolvedPath.rfind("/map/", 0) == 0);
  if (receiverQueryCandidate) {
    const Expr &receiverExpr = expr.args.front();
    if (receiverExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *boundReceiver = findBinding(defParams, activeLocals, receiverExpr.name);
          boundReceiver != nullptr) {
        out.receiverBinding = *boundReceiver;
      }
    }
    if (out.receiverBinding.typeName.empty() &&
        expr.isMethodCall &&
        !(withPreservedError([&]() {
            return inferBindingTypeFromInitializer(receiverExpr, defParams, activeLocals, out.receiverBinding);
          }) &&
          !out.receiverBinding.typeName.empty())) {
      out.receiverBinding = {};
    }
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

  out.resolvedPath = preferredCollectionHelperResolvedPath(expr);
  if (out.resolvedPath.empty()) {
    out.resolvedPath = resolveCalleePath(expr);
  }
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty()) {
    std::string resolvedMethodPath;
    bool isBuiltinMethod = false;
    if (withPreservedError([&]() {
          return resolveMethodTarget(defParams,
                                     activeLocals,
                                     expr.namespacePrefix,
                                     expr.args.front(),
                                     expr.name,
                                     resolvedMethodPath,
                                     isBuiltinMethod);
        }) &&
        !resolvedMethodPath.empty()) {
      out.resolvedPath = std::move(resolvedMethodPath);
    }
  }
  if (!out.resolvedPath.empty()) {
    const std::string concreteResolvedPath =
        resolveExprConcreteCallPath(defParams, activeLocals, expr, out.resolvedPath);
    if (!concreteResolvedPath.empty()) {
      out.resolvedPath = concreteResolvedPath;
    }
  }
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

  std::function<void(const Definition &,
                     const std::vector<ParameterInfo> &,
                     const Expr &,
                     ActiveLocalBindings &,
                     LocalBindingScope &)>
      visitExpr;
  std::function<void(const Definition &,
                     const std::vector<ParameterInfo> &,
                     const std::vector<Expr> &,
                     ActiveLocalBindings &,
                     LocalBindingScope &)>
      visitExprSequence;

  visitExprSequence = [&](const Definition &def,
                          const std::vector<ParameterInfo> &defParams,
                          const std::vector<Expr> &exprs,
                          ActiveLocalBindings &activeLocals,
                          LocalBindingScope &scope) {
    for (const auto &expr : exprs) {
      visitExpr(def, defParams, expr, activeLocals, scope);
    }
  };

  visitExpr = [&](const Definition &def,
                  const std::vector<ParameterInfo> &defParams,
                  const Expr &expr,
                  ActiveLocalBindings &activeLocals,
                  LocalBindingScope &scope) {
    (void)scope;
    if (expr.isBinding) {
      for (const auto &arg : expr.args) {
        LocalBindingScope argScope(*this, activeLocals);
        visitExpr(def, defParams, arg, activeLocals, argScope);
      }
      if (!expr.bodyArguments.empty()) {
        LocalBindingScope bodyScope(*this, activeLocals);
        visitExprSequence(def, defParams, expr.bodyArguments, activeLocals, bodyScope);
      }

      BindingInfo binding;
      if (inferBindingForLocals(def, defParams, activeLocals, expr, binding)) {
        insertLocalBinding(activeLocals, expr.name, std::move(binding));
      }
      return;
    }

    if (expr.kind == Expr::Kind::Call) {
      visitor(def, defParams, expr, activeLocals);
    }

    for (const auto &arg : expr.args) {
      LocalBindingScope argScope(*this, activeLocals);
      visitExpr(def, defParams, arg, activeLocals, argScope);
    }
    if (!expr.bodyArguments.empty()) {
      LocalBindingScope bodyScope(*this, activeLocals);
      visitExprSequence(def, defParams, expr.bodyArguments, activeLocals, bodyScope);
    }
  };

  auto resetSnapshotInferenceCaches = [&]() {
    callTargetResolutionScratch_.resetArena();
    inferExprReturnKindMemo_.clear();
    inferExprReturnKindMemo_.rehash(0);
    inferStructReturnMemo_.clear();
    inferStructReturnMemo_.rehash(0);
    structFieldReturnKindMemo_.clear();
    structFieldReturnKindMemo_.rehash(0);
    localBindingMemoRevisionByIdentity_.clear();
    localBindingMemoRevisionByIdentity_.rehash(0);
    queryTypeInferenceDefinitionStack_.clear();
    queryTypeInferenceDefinitionStack_.rehash(0);
    queryTypeInferenceExprStack_.clear();
    queryTypeInferenceExprStack_.rehash(0);
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      resetSnapshotInferenceCaches();
      continue;
    }
    const auto &defParams = paramsIt->second;

    for (const auto &param : def.parameters) {
      for (const auto &arg : param.args) {
        ActiveLocalBindings paramLocals;
        LocalBindingScope paramScope(*this, paramLocals);
        visitExpr(def, defParams, arg, paramLocals, paramScope);
      }
      if (!param.bodyArguments.empty()) {
        ActiveLocalBindings paramLocals;
        LocalBindingScope paramScope(*this, paramLocals);
        visitExprSequence(def, defParams, param.bodyArguments, paramLocals, paramScope);
      }
    }

    ActiveLocalBindings definitionLocals;
    LocalBindingScope definitionScopeLocals(*this, definitionLocals);
    visitExprSequence(def, defParams, def.statements, definitionLocals, definitionScopeLocals);
    if (def.returnExpr.has_value()) {
      LocalBindingScope returnScope(*this, definitionLocals);
      visitExpr(def, defParams, *def.returnExpr, definitionLocals, returnScope);
    }
    resetSnapshotInferenceCaches();
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
  if (queryFactSnapshotCacheValid_) {
    return;
  }

  queryFactSnapshotCache_.clear();

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
  queryFactSnapshotCacheValid_ = true;
}

void SemanticsValidator::ensureCallAndTrySnapshotFactCaches(bool includeTryValues,
                                                            bool includeCallBindings) {
  const bool buildTryValues = includeTryValues && !tryValueSnapshotCacheValid_;
  const bool buildCallBindings = includeCallBindings && !callBindingSnapshotCacheValid_;
  if (!buildTryValues && !buildCallBindings) {
    return;
  }

  if (buildCallBindings) {
    callBindingSnapshotCache_.clear();
  }
  if (buildTryValues) {
    tryValueSnapshotCache_.clear();
  }

  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    if (buildCallBindings) {
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
    }

    if (buildTryValues) {
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
    }
  });

  if (buildCallBindings) {
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
    callBindingSnapshotCacheValid_ = true;
  }

  if (buildTryValues) {
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
    tryValueSnapshotCacheValid_ = true;
  }
}

SemanticsValidator::GraphLocalAutoKey SemanticsValidator::graphLocalAutoBindingKey(std::string_view scopePath,
                                                                                    int sourceLine,
                                                                                    int sourceColumn) const {
  if (benchmarkGraphLocalAutoLegacyKeyShadowEnabled_ &&
      benchmarkGraphLocalAutoLegacyShadowState_ != nullptr) {
    std::string legacyKey;
    legacyKey.reserve(scopePath.size() + 32);
    legacyKey.append(scopePath);
    legacyKey.push_back('#');
    legacyKey.append(std::to_string(sourceLine));
    legacyKey.push_back(':');
    legacyKey.append(std::to_string(sourceColumn));
    benchmarkGraphLocalAutoLegacyShadowState_->keyShadow.insert(std::move(legacyKey));
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

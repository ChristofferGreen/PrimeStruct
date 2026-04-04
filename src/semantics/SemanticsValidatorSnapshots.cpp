#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>

namespace primec::semantics {

std::vector<SemanticsValidator::ReturnResolutionSnapshotEntry>
SemanticsValidator::returnResolutionSnapshotForTesting() const {
  std::vector<ReturnResolutionSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());
  for (const auto &definition : program_.definitions) {
    const auto kindIt = returnKinds_.find(definition.fullPath);
    if (kindIt == returnKinds_.end()) {
      continue;
    }
    ReturnResolutionSnapshotEntry entry;
    entry.definitionPath = definition.fullPath;
    entry.kind = kindIt->second;
    if (const auto structIt = returnStructs_.find(definition.fullPath);
        structIt != returnStructs_.end()) {
      entry.structPath = structIt->second;
    }
    if (const auto bindingIt = returnBindings_.find(definition.fullPath);
        bindingIt != returnBindings_.end()) {
      entry.binding = bindingIt->second;
    }
    entries.push_back(std::move(entry));
  }
  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.definitionPath < right.definitionPath;
  });
  return entries;
}

std::vector<SemanticsValidator::LocalAutoBindingSnapshotEntry>
SemanticsValidator::localAutoBindingSnapshotForTesting() const {
  auto explicitBindingTypeName = [](const Expr &expr) -> std::optional<std::string> {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isBindingAuxTransformName(transform.name) || !transform.arguments.empty()) {
        continue;
      }
      return transform.name;
    }
    return std::nullopt;
  };

  auto isLocalAutoCandidate = [&](const Expr &expr) {
    const std::optional<std::string> typeName = explicitBindingTypeName(expr);
    return !typeName.has_value() || normalizeBindingTypeName(*typeName) == "auto";
  };

  std::vector<LocalAutoBindingSnapshotEntry> entries;
  std::function<void(const std::string &, const Expr &)> visitExpr;
  visitExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding && isLocalAutoCandidate(expr)) {
      const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(expr);
      const auto bindingIt = graphLocalAutoBindings_.find(
          graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
      if (bindingIt != graphLocalAutoBindings_.end()) {
        std::string initializerResolvedPath;
        if (const auto pathIt = graphLocalAutoResolvedPaths_.find(
                graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
            pathIt != graphLocalAutoResolvedPaths_.end()) {
          initializerResolvedPath = pathIt->second;
        }
        BindingInfo initializerBinding;
        if (const auto bindingInfoIt = graphLocalAutoInitializerBindings_.find(
                graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
            bindingInfoIt != graphLocalAutoInitializerBindings_.end()) {
          initializerBinding = bindingInfoIt->second;
        }
        BindingInfo initializerReceiverBinding;
        if (const auto receiverIt = graphLocalAutoReceiverBindings_.find(
                graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
            receiverIt != graphLocalAutoReceiverBindings_.end()) {
          initializerReceiverBinding = receiverIt->second;
        }
        std::string initializerQueryTypeText;
        if (const auto queryIt = graphLocalAutoQueryTypeTexts_.find(
                graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
            queryIt != graphLocalAutoQueryTypeTexts_.end()) {
          initializerQueryTypeText = queryIt->second;
        }
        bool initializerResultHasValue = false;
        std::string initializerResultValueType;
        std::string initializerResultErrorType;
        if (const auto resultIt = graphLocalAutoResultTypes_.find(
                graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
            resultIt != graphLocalAutoResultTypes_.end()) {
          initializerResultHasValue = resultIt->second.hasValue;
          initializerResultValueType = resultIt->second.valueType;
          initializerResultErrorType = resultIt->second.errorType;
        }
        bool initializerHasTry = false;
        std::string initializerTryOperandResolvedPath;
        BindingInfo initializerTryOperandBinding;
        BindingInfo initializerTryOperandReceiverBinding;
        std::string initializerTryOperandQueryTypeText;
        std::string initializerTryValueType;
        std::string initializerTryErrorType;
        ReturnKind initializerTryContextReturnKind = ReturnKind::Unknown;
        std::string initializerTryOnErrorHandlerPath;
        std::string initializerTryOnErrorErrorType;
        size_t initializerTryOnErrorBoundArgCount = 0;
        if (const auto tryIt = graphLocalAutoTryValues_.find(
                graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn));
            tryIt != graphLocalAutoTryValues_.end()) {
          initializerHasTry = true;
          initializerTryOperandResolvedPath = tryIt->second.operandResolvedPath;
          initializerTryOperandBinding = tryIt->second.operandBinding;
          initializerTryOperandReceiverBinding = tryIt->second.operandReceiverBinding;
          initializerTryOperandQueryTypeText = tryIt->second.operandQueryTypeText;
          initializerTryValueType = tryIt->second.valueType;
          initializerTryErrorType = tryIt->second.errorType;
          initializerTryContextReturnKind = tryIt->second.contextReturnKind;
          initializerTryOnErrorHandlerPath = tryIt->second.onErrorHandlerPath;
          initializerTryOnErrorErrorType = tryIt->second.onErrorErrorType;
          initializerTryOnErrorBoundArgCount = tryIt->second.onErrorBoundArgCount;
        }
        entries.push_back(LocalAutoBindingSnapshotEntry{
            scopePath,
            expr.name,
            sourceLine,
            sourceColumn,
            bindingIt->second,
            std::move(initializerResolvedPath),
            std::move(initializerBinding),
            std::move(initializerReceiverBinding),
            std::move(initializerQueryTypeText),
            initializerResultHasValue,
            std::move(initializerResultValueType),
            std::move(initializerResultErrorType),
            initializerHasTry,
            std::move(initializerTryOperandResolvedPath),
            std::move(initializerTryOperandBinding),
            std::move(initializerTryOperandReceiverBinding),
            std::move(initializerTryOperandQueryTypeText),
            std::move(initializerTryValueType),
            std::move(initializerTryErrorType),
            initializerTryContextReturnKind,
            std::move(initializerTryOnErrorHandlerPath),
            std::move(initializerTryOnErrorErrorType),
            initializerTryOnErrorBoundArgCount,
        });
      }
    }
    for (const auto &arg : expr.args) {
      visitExpr(scopePath, arg);
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      visitExpr(scopePath, bodyExpr);
    }
  };

  for (const auto &definition : program_.definitions) {
    for (const auto &parameter : definition.parameters) {
      for (const auto &arg : parameter.args) {
        visitExpr(definition.fullPath, arg);
      }
      for (const auto &bodyExpr : parameter.bodyArguments) {
        visitExpr(definition.fullPath, bodyExpr);
      }
    }
    for (const auto &statement : definition.statements) {
      visitExpr(definition.fullPath, statement);
    }
    if (definition.returnExpr.has_value()) {
      visitExpr(definition.fullPath, *definition.returnExpr);
    }
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.scopePath != right.scopePath) {
      return left.scopePath < right.scopePath;
    }
    if (left.sourceLine != right.sourceLine) {
      return left.sourceLine < right.sourceLine;
    }
    if (left.sourceColumn != right.sourceColumn) {
      return left.sourceColumn < right.sourceColumn;
    }
    return left.bindingName < right.bindingName;
  });
  return entries;
}

std::vector<SemanticsValidator::QueryCallTypeSnapshotEntry>
SemanticsValidator::queryCallTypeSnapshotForTesting() {
  std::vector<QueryCallTypeSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    QuerySnapshotData queryData;
    if (inferQuerySnapshotData(defParams, activeLocals, expr, queryData) &&
        !queryData.typeText.empty()) {
      entries.push_back(QueryCallTypeSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(queryData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          std::move(queryData.typeText),
      });
    }
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::QueryBindingSnapshotEntry>
SemanticsValidator::queryBindingSnapshotForTesting() {
  std::vector<QueryBindingSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    QuerySnapshotData queryData;
    if (inferQuerySnapshotData(defParams, activeLocals, expr, queryData) &&
        !queryData.binding.typeName.empty()) {
      entries.push_back(QueryBindingSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(queryData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          std::move(queryData.binding),
      });
    }
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::QueryResultTypeSnapshotEntry>
SemanticsValidator::queryResultTypeSnapshotForTesting() {
  std::vector<QueryResultTypeSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    QuerySnapshotData queryData;
    if (inferQuerySnapshotData(defParams, activeLocals, expr, queryData) &&
        queryData.resultInfo.isResult) {
      entries.push_back(QueryResultTypeSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(queryData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          queryData.resultInfo.hasValue,
          std::move(queryData.resultInfo.valueType),
          std::move(queryData.resultInfo.errorType),
      });
    }
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::TryValueSnapshotEntry>
SemanticsValidator::tryValueSnapshotForTesting() {
  std::vector<TryValueSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    LocalAutoTrySnapshotData tryData;
    if (!inferTrySnapshotData(def, defParams, activeLocals, expr, tryData)) {
      return;
    }

    entries.push_back(TryValueSnapshotEntry{
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
    });
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::CallBindingSnapshotEntry>
SemanticsValidator::callBindingSnapshotForTesting() {
  std::vector<CallBindingSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    CallSnapshotData callData;
    if (inferCallSnapshotData(defParams, activeLocals, expr, callData) &&
        !callData.resolvedPath.empty() &&
        !callData.binding.typeName.empty()) {
      entries.push_back(CallBindingSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(callData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          std::move(callData.binding),
      });
    }
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::DirectCallTargetSnapshotEntry>
SemanticsValidator::directCallTargetSnapshotForSemanticProduct() const {
  std::vector<DirectCallTargetSnapshotEntry> entries;
  std::function<void(const std::string &, const Expr &)> visitExpr;
  auto visitExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      visitExpr(scopePath, expr);
    }
  };

  visitExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      const std::string resolvedPath = resolveCalleePath(expr);
      if (!resolvedPath.empty()) {
        entries.push_back(DirectCallTargetSnapshotEntry{
            scopePath,
            expr.name,
            resolvedPath,
            expr.sourceLine,
            expr.sourceColumn,
        });
      }
    }
    visitExprs(scopePath, expr.args);
    visitExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program_.definitions) {
    visitExprs(def.fullPath, def.parameters);
    visitExprs(def.fullPath, def.statements);
    if (def.returnExpr.has_value()) {
      visitExpr(def.fullPath, *def.returnExpr);
    }
  }

  for (const auto &exec : program_.executions) {
    visitExprs(exec.fullPath, exec.arguments);
    visitExprs(exec.fullPath, exec.bodyArguments);
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::MethodCallTargetSnapshotEntry>
SemanticsValidator::methodCallTargetSnapshotForSemanticProduct() {
  std::vector<MethodCallTargetSnapshotEntry> entries;

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty()) {
      return;
    }

    std::string resolvedPath;
    bool builtin = false;
    if (!resolveMethodTarget(defParams,
                             activeLocals,
                             expr.namespacePrefix,
                             expr.args.front(),
                             expr.name,
                             resolvedPath,
                             builtin) ||
        resolvedPath.empty()) {
      return;
    }

    BindingInfo receiverBinding;
    if (!(withPreservedError([&]() {
            return inferBindingTypeFromInitializer(
                expr.args.front(), defParams, activeLocals, receiverBinding);
          }) &&
          !receiverBinding.typeName.empty())) {
      receiverBinding = {};
    }

    entries.push_back(MethodCallTargetSnapshotEntry{
        def.fullPath,
        expr.name,
        std::move(resolvedPath),
        expr.sourceLine,
        expr.sourceColumn,
        std::move(receiverBinding),
    });
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.scopePath != right.scopePath) {
      return left.scopePath < right.scopePath;
    }
    if (left.sourceLine != right.sourceLine) {
      return left.sourceLine < right.sourceLine;
    }
    if (left.sourceColumn != right.sourceColumn) {
      return left.sourceColumn < right.sourceColumn;
    }
    if (left.methodName != right.methodName) {
      return left.methodName < right.methodName;
    }
    return left.resolvedPath < right.resolvedPath;
  });
  return entries;
}

std::vector<SemanticsValidator::QueryReceiverBindingSnapshotEntry>
SemanticsValidator::queryReceiverBindingSnapshotForTesting() {
  std::vector<QueryReceiverBindingSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    QuerySnapshotData queryData;
    if (inferQuerySnapshotData(defParams, activeLocals, expr, queryData) &&
        !queryData.receiverBinding.typeName.empty()) {
      entries.push_back(QueryReceiverBindingSnapshotEntry{
          def.fullPath,
          expr.name,
          std::move(queryData.resolvedPath),
          expr.sourceLine,
          expr.sourceColumn,
          std::move(queryData.receiverBinding),
      });
    }
  });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
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
  return entries;
}

std::vector<SemanticsValidator::OnErrorSnapshotEntry>
SemanticsValidator::onErrorSnapshotForTesting() {
  std::vector<OnErrorSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());
  for (const auto &def : program_.definitions) {
    const auto state = buildDefinitionValidationState(def);
    const auto &context = state.context;
    if (!context.onError.has_value()) {
      continue;
    }
    ReturnKind returnKind = ReturnKind::Unknown;
    if (const auto returnKindIt = returnKinds_.find(def.fullPath); returnKindIt != returnKinds_.end()) {
      returnKind = returnKindIt->second;
    }

    entries.push_back(OnErrorSnapshotEntry{
        def.fullPath,
        returnKind,
        context.onError->handlerPath,
        context.onError->errorType,
        context.onError->boundArgs.size(),
        context.resultType.has_value() && context.resultType->isResult && context.resultType->hasValue,
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->valueType : std::string{},
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->errorType : std::string{},
    });
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.definitionPath < right.definitionPath;
  });
  return entries;
}

std::vector<SemanticsValidator::ValidationContextSnapshotEntry>
SemanticsValidator::validationContextSnapshotForTesting() const {
  std::vector<ValidationContextSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());
  for (const auto &def : program_.definitions) {
    const auto state = buildDefinitionValidationState(def);
    const auto &context = state.context;
    ReturnKind returnKind = ReturnKind::Unknown;
    if (const auto returnKindIt = returnKinds_.find(def.fullPath); returnKindIt != returnKinds_.end()) {
      returnKind = returnKindIt->second;
    }
    std::vector<std::string> activeEffects(context.activeEffects.begin(), context.activeEffects.end());
    std::sort(activeEffects.begin(), activeEffects.end());
    activeEffects.erase(std::unique(activeEffects.begin(), activeEffects.end()), activeEffects.end());

    entries.push_back(ValidationContextSnapshotEntry{
        def.fullPath,
        returnKind,
        context.definitionIsCompute,
        context.definitionIsUnsafe,
        std::move(activeEffects),
        context.resultType.has_value() && context.resultType->isResult,
        context.resultType.has_value() && context.resultType->isResult && context.resultType->hasValue,
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->valueType : std::string{},
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->errorType : std::string{},
        context.onError.has_value(),
        context.onError.has_value() ? context.onError->handlerPath : std::string{},
        context.onError.has_value() ? context.onError->errorType : std::string{},
        context.onError.has_value() ? context.onError->boundArgs.size() : 0,
    });
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.definitionPath < right.definitionPath;
  });
  return entries;
}

} // namespace primec::semantics

#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace primec::semantics {

namespace {

bool isSlashlessMapHelperName(std::string_view name) {
  if (!name.empty() && name.front() == '/') {
    name.remove_prefix(1);
  }
  return name.rfind("map/", 0) == 0 || name.rfind("std/collections/map/", 0) == 0;
}

} // namespace

SemanticsValidator::SemanticsValidator(const Program &program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       const std::vector<std::string> &defaultEffects,
                                       const std::vector<std::string> &entryDefaultEffects,
                                       SemanticDiagnosticInfo *diagnosticInfo,
                                       bool collectDiagnostics)
    : program_(program),
      entryPath_(entryPath),
      error_(error),
      defaultEffects_(defaultEffects),
      entryDefaultEffects_(entryDefaultEffects),
      diagnosticInfo_(diagnosticInfo),
      diagnosticSink_(diagnosticInfo),
      collectDiagnostics_(collectDiagnostics) {
  diagnosticSink_.reset();
  auto registerMathImport = [&](const std::string &importPath) {
    if (importPath == "/std/math/*") {
      mathImportAll_ = true;
      return;
    }
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
      std::string name = importPath.substr(10);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
  };
  for (const auto &importPath : program_.sourceImports) {
    registerMathImport(importPath);
  }
  for (const auto &importPath : program_.imports) {
    registerMathImport(importPath);
  }
}

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
  const auto &context = buildDefinitionValidationContext(def);
  if (context.onError.has_value()) {
    out.onErrorHandlerPath = context.onError->handlerPath;
    out.onErrorErrorType = context.onError->errorType;
    out.onErrorBoundArgCount = context.onError->boundArgs.size();
  }
  return true;
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
    const auto &context = buildDefinitionValidationContext(def);
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
    const auto &context = buildDefinitionValidationContext(def);
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
    if (!hasExplicitBindingTypeTransform(bindingExpr) && bindingExpr.args.size() == 1) {
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
  std::function<void(const Definition &, const std::vector<ParameterInfo> &, const std::vector<Expr> &, ActiveLocalBindings &)>
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
    ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));
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

std::string SemanticsValidator::graphLocalAutoBindingKey(const std::string &scopePath,
                                                         int sourceLine,
                                                         int sourceColumn) {
  return scopePath + "@" + std::to_string(sourceLine) + ":" + std::to_string(sourceColumn);
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

std::string SemanticsValidator::formatUnknownCallTarget(const Expr &expr) const {
  if (!isSlashlessMapHelperName(expr.name)) {
    return expr.name;
  }
  const std::string resolved = resolveCalleePath(expr);
  return resolved.empty() ? expr.name : resolved;
}

std::string SemanticsValidator::diagnosticCallTargetPath(const std::string &path) const {
  if (path.empty()) {
    return path;
  }
  if (path.rfind("/std/collections/map/count__t", 0) == 0) {
    return "/std/collections/map/count";
  }
  if (path.rfind("/map/count__t", 0) == 0) {
    return "/map/count";
  }
  const size_t lastSlash = path.find_last_of('/');
  const size_t nameStart = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  const size_t suffix = path.find("__t", nameStart);
  if (suffix == std::string::npos) {
    return path;
  }
  const std::string basePath = path.substr(0, suffix);
  auto it = defMap_.find(basePath);
  if (it == defMap_.end() || it->second == nullptr || it->second->templateArgs.empty()) {
    return path;
  }
  return basePath;
}

bool SemanticsValidator::makeDefinitionValidationContext(const Definition &def, ValidationContext &out) {
  out = {};
  out.definitionPath = def.fullPath;
  for (const auto &transform : def.transforms) {
    if (transform.name == "compute") {
      out.definitionIsCompute = true;
    } else if (transform.name == "unsafe") {
      out.definitionIsUnsafe = true;
    } else if (transform.name == "return" && transform.templateArgs.size() == 1) {
      ResultTypeInfo resultInfo;
      if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultInfo)) {
        out.resultType = std::move(resultInfo);
      }
    }
  }
  out.activeEffects = resolveEffects(def.transforms, def.fullPath == entryPath_);
  std::optional<OnErrorHandler> onErrorHandler;
  if (!parseOnErrorTransform(def.transforms, def.namespacePrefix, def.fullPath, onErrorHandler)) {
    return false;
  }
  out.onError = std::move(onErrorHandler);
  return true;
}

SemanticsValidator::ValidationContext
SemanticsValidator::makeExecutionValidationContext(const Execution &exec) const {
  ValidationContext context;
  context.definitionPath.clear();
  context.definitionIsCompute = false;
  context.definitionIsUnsafe = false;
  context.activeEffects = resolveEffects(exec.transforms, false);
  return context;
}

const SemanticsValidator::ValidationContext &
SemanticsValidator::buildDefinitionValidationContext(const Definition &def) const {
  auto it = definitionValidationContexts_.find(def.fullPath);
  if (it != definitionValidationContexts_.end()) {
    return it->second;
  }
  static const ValidationContext EmptyContext;
  return EmptyContext;
}

const SemanticsValidator::ValidationContext &
SemanticsValidator::buildExecutionValidationContext(const Execution &exec) const {
  auto it = executionValidationContexts_.find(exec.fullPath);
  if (it != executionValidationContexts_.end()) {
    return it->second;
  }
  static const ValidationContext EmptyContext;
  return EmptyContext;
}

void SemanticsValidator::capturePrimarySpanIfUnset(int line, int column) {
  diagnosticSink_.capturePrimarySpanIfUnset(line, column);
}

void SemanticsValidator::captureRelatedSpan(int line, int column, const std::string &label) {
  diagnosticSink_.addRelatedSpan(line, column, label);
}

void SemanticsValidator::captureExprContext(const Expr &expr) {
  capturePrimarySpanIfUnset(expr.sourceLine, expr.sourceColumn);
  if (currentDefinitionContext_ != nullptr) {
    captureRelatedSpan(currentDefinitionContext_->sourceLine,
                       currentDefinitionContext_->sourceColumn,
                       "definition: " + currentDefinitionContext_->fullPath);
  }
  if (currentExecutionContext_ != nullptr) {
    captureRelatedSpan(currentExecutionContext_->sourceLine,
                       currentExecutionContext_->sourceColumn,
                       "execution: " + currentExecutionContext_->fullPath);
  }
}

void SemanticsValidator::captureDefinitionContext(const Definition &def) {
  capturePrimarySpanIfUnset(def.sourceLine, def.sourceColumn);
  captureRelatedSpan(def.sourceLine, def.sourceColumn, "definition: " + def.fullPath);
}

void SemanticsValidator::captureExecutionContext(const Execution &exec) {
  capturePrimarySpanIfUnset(exec.sourceLine, exec.sourceColumn);
  captureRelatedSpan(exec.sourceLine, exec.sourceColumn, "execution: " + exec.fullPath);
}

bool SemanticsValidator::collectDuplicateDefinitionDiagnostics() {
  std::unordered_map<std::string, std::vector<const Definition *>> definitionsByPath;
  definitionsByPath.reserve(program_.definitions.size());
  for (const auto &def : program_.definitions) {
    definitionsByPath[def.fullPath].push_back(&def);
  }

  struct DuplicateGroup {
    std::string path;
    std::vector<const Definition *> definitions;
  };
  std::vector<DuplicateGroup> duplicateGroups;
  duplicateGroups.reserve(definitionsByPath.size());
  for (auto &entry : definitionsByPath) {
    if (entry.second.size() <= 1) {
      continue;
    }
    auto &defs = entry.second;
    std::stable_sort(defs.begin(), defs.end(), [](const Definition *left, const Definition *right) {
      const int leftLine = left->sourceLine > 0 ? left->sourceLine : std::numeric_limits<int>::max();
      const int rightLine = right->sourceLine > 0 ? right->sourceLine : std::numeric_limits<int>::max();
      if (leftLine != rightLine) {
        return leftLine < rightLine;
      }
      const int leftColumn = left->sourceColumn > 0 ? left->sourceColumn : std::numeric_limits<int>::max();
      const int rightColumn = right->sourceColumn > 0 ? right->sourceColumn : std::numeric_limits<int>::max();
      if (leftColumn != rightColumn) {
        return leftColumn < rightColumn;
      }
      return left->fullPath < right->fullPath;
    });
    duplicateGroups.push_back(DuplicateGroup{entry.first, defs});
  }
  if (duplicateGroups.empty()) {
    return true;
  }

  std::stable_sort(duplicateGroups.begin(), duplicateGroups.end(), [](const DuplicateGroup &left, const DuplicateGroup &right) {
    const Definition *leftDef = left.definitions.front();
    const Definition *rightDef = right.definitions.front();
    const int leftLine = leftDef->sourceLine > 0 ? leftDef->sourceLine : std::numeric_limits<int>::max();
    const int rightLine = rightDef->sourceLine > 0 ? rightDef->sourceLine : std::numeric_limits<int>::max();
    if (leftLine != rightLine) {
      return leftLine < rightLine;
    }
    const int leftColumn = leftDef->sourceColumn > 0 ? leftDef->sourceColumn : std::numeric_limits<int>::max();
    const int rightColumn = rightDef->sourceColumn > 0 ? rightDef->sourceColumn : std::numeric_limits<int>::max();
    if (leftColumn != rightColumn) {
      return leftColumn < rightColumn;
    }
    return left.path < right.path;
  });

  if (diagnosticInfo_ != nullptr) {
    std::vector<DiagnosticSinkRecord> records;
    records.reserve(duplicateGroups.size());
    for (const auto &group : duplicateGroups) {
      DiagnosticSinkRecord record;
      record.message = "duplicate definition: " + group.path;
      const Definition *primary = group.definitions.front();
      if (primary->sourceLine > 0 && primary->sourceColumn > 0) {
        record.primarySpan.line = primary->sourceLine;
        record.primarySpan.column = primary->sourceColumn;
        record.primarySpan.endLine = primary->sourceLine;
        record.primarySpan.endColumn = primary->sourceColumn;
        record.hasPrimarySpan = true;
      }
      for (const Definition *def : group.definitions) {
        if (def->sourceLine <= 0 || def->sourceColumn <= 0) {
          continue;
        }
        const std::string label = "definition: " + def->fullPath;
        bool duplicateSpan = false;
        for (const auto &existing : record.relatedSpans) {
          if (existing.span.line == def->sourceLine && existing.span.column == def->sourceColumn &&
              existing.label == label) {
            duplicateSpan = true;
            break;
          }
        }
        if (duplicateSpan) {
          continue;
        }
        DiagnosticRelatedSpan span;
        span.span.line = def->sourceLine;
        span.span.column = def->sourceColumn;
        span.span.endLine = def->sourceLine;
        span.span.endColumn = def->sourceColumn;
        span.label = label;
        record.relatedSpans.push_back(std::move(span));
      }
      records.push_back(std::move(record));
    }
    diagnosticSink_.setRecords(std::move(records));
  }

  error_ = "duplicate definition: " + duplicateGroups.front().path;
  return false;
}

bool SemanticsValidator::run() {
  if (collectDiagnostics_ && !collectDuplicateDefinitionDiagnostics()) {
    return false;
  }
  if (!buildDefinitionMaps()) {
    return false;
  }
  if (!inferUnknownReturnKinds()) {
    return false;
  }
  if (!validateTraitConstraints()) {
    return false;
  }
  if (!validateStructLayouts()) {
    return false;
  }
  if (!validateDefinitions()) {
    return false;
  }
  if (!validateExecutions()) {
    return false;
  }
  return validateEntry();
}

bool SemanticsValidator::allowMathBareName(const std::string &name) const {
  (void)name;
  if (name.empty() || name.find('/') != std::string::npos) {
    return false;
  }
  if (!currentValidationContext_.definitionPath.empty()) {
    if (currentValidationContext_.definitionPath == "/std/math" ||
        currentValidationContext_.definitionPath.rfind("/std/math/", 0) == 0) {
      return true;
    }
    if (currentValidationContext_.definitionPath.rfind("/std/", 0) == 0) {
      for (const auto &importPath : program_.imports) {
        if (importPath == "/std/math/*") {
          return true;
        }
        if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
          const std::string importedName = importPath.substr(10);
          if (!importedName.empty() && importedName.find('/') == std::string::npos) {
            return true;
          }
        }
      }
    }
  }
  return hasAnyMathImport();
}

bool SemanticsValidator::hasAnyMathImport() const {
  return mathImportAll_ || !mathImports_.empty();
}

bool SemanticsValidator::isEntryArgsName(const std::string &name) const {
  if (currentValidationContext_.definitionPath != entryPath_) {
    return false;
  }
  if (entryArgsName_.empty()) {
    return false;
  }
  return name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgsAccess(const Expr &expr) const {
  if (currentValidationContext_.definitionPath != entryPath_ || entryArgsName_.empty()) {
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string accessName;
  if (!getBuiltinArrayAccessName(expr, accessName) || expr.args.size() != 2) {
    return false;
  }
  if (expr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  return expr.args.front().name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgStringBinding(const std::unordered_map<std::string, BindingInfo> &locals,
                                                 const Expr &expr) const {
  if (currentValidationContext_.definitionPath != entryPath_) {
    return false;
  }
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = locals.find(expr.name);
  return it != locals.end() && it->second.isEntryArgString;
}

bool SemanticsValidator::parseTransformArgumentExpr(const std::string &text,
                                                    const std::string &namespacePrefix,
                                                    Expr &out) {
  Lexer lexer(text);
  Parser parser(lexer.tokenize());
  std::string parseError;
  if (!parser.parseExpression(out, namespacePrefix, parseError)) {
    error_ = parseError.empty() ? "invalid transform argument expression" : parseError;
    return false;
  }
  return true;
}

namespace {
std::string trimText(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}
} // namespace

bool SemanticsValidator::resolveResultTypeFromTemplateArg(const std::string &templateArg, ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  std::vector<std::string> parts;
  if (!splitTopLevelTemplateArgs(templateArg, parts)) {
    return false;
  }
  if (parts.size() == 1) {
    out.isResult = true;
    out.hasValue = false;
    out.errorType = trimText(parts.front());
    return true;
  }
  if (parts.size() == 2) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = trimText(parts[0]);
    out.errorType = trimText(parts[1]);
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveResultTypeFromTypeName(const std::string &typeName, ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg) || base != "Result") {
    return false;
  }
  return resolveResultTypeFromTemplateArg(arg, out);
}

bool SemanticsValidator::resolveResultTypeForExpr(const Expr &expr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                                  ResultTypeInfo &out) {
  out = ResultTypeInfo{};
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto resolveDirectResultOkType = [&](const Expr &candidate,
                                       const std::vector<ParameterInfo> &currentParams,
                                       const std::unordered_map<std::string, BindingInfo> &currentLocals,
                                       const std::string &preferredErrorType,
                                       ResultTypeInfo &resultOut) -> bool {
    resultOut = ResultTypeInfo{};
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
        !candidate.templateArgs.empty() || candidate.hasBodyArguments || !candidate.bodyArguments.empty() ||
        candidate.args.empty()) {
      return false;
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    std::string errorType = preferredErrorType;
    if (errorType.empty() && currentValidationContext_.resultType.has_value() &&
        currentValidationContext_.resultType->isResult &&
        !currentValidationContext_.resultType->errorType.empty()) {
      errorType = currentValidationContext_.resultType->errorType;
    }
    if (errorType.empty() && currentValidationContext_.onError.has_value() &&
        !currentValidationContext_.onError->errorType.empty()) {
      errorType = currentValidationContext_.onError->errorType;
    }
    if (errorType.empty()) {
      errorType = "_";
    }
    resultOut.isResult = true;
    resultOut.errorType = std::move(errorType);
    if (candidate.args.size() == 1) {
      resultOut.hasValue = false;
      return true;
    }
    if (candidate.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(candidate.args.back(), currentParams, currentLocals, payloadBinding)) {
      return false;
    }
    resultOut.hasValue = true;
    resultOut.valueType = bindingTypeText(payloadBinding);
    return !resultOut.valueType.empty();
  };
  auto resolveBindingTypeText = [&](const std::string &name, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    auto describeBindingType = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      typeTextOut = describeBindingType(*paramBinding);
      return true;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      typeTextOut = describeBindingType(it->second);
      return true;
    }
    return false;
  };
  auto resolveBuiltinMapResultType = [&](const std::string &typeText) -> bool {
    const std::string normalizedTypeText = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedTypeText, base, argText) &&
        normalizeBindingTypeName(base) == "map") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
        return false;
      }
      out.isResult = true;
      out.hasValue = true;
      out.valueType = args[1];
      out.errorType = "ContainerError";
      return true;
    }
    std::string resolvedPath = normalizedTypeText;
    if (!resolvedPath.empty() && resolvedPath.front() != '/') {
      resolvedPath.insert(resolvedPath.begin(), '/');
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
      return false;
    }
    auto defIt = defMap_.find(resolvedPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    std::string valueType;
    for (const auto &fieldExpr : defIt->second->statements) {
      if (!fieldExpr.isBinding || fieldExpr.name != "payloads") {
        continue;
      }
      BindingInfo fieldBinding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (!parseBindingInfo(fieldExpr,
                            defIt->second->namespacePrefix,
                            structNames_,
                            importAliases_,
                            fieldBinding,
                            restrictType,
                            parseError)) {
        continue;
      }
      if (normalizeBindingTypeName(fieldBinding.typeName) != "vector" ||
          fieldBinding.typeTemplateArg.empty()) {
        continue;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      valueType = args.front();
      break;
    }
    if (valueType.empty()) {
      return false;
    }
    out.isResult = true;
    out.hasValue = true;
    out.valueType = valueType;
    out.errorType = "ContainerError";
    return true;
  };
  std::function<bool(const Expr &, std::string &)> resolveMapReceiverTypeText;
  resolveMapReceiverTypeText = [&](const Expr &receiverExpr, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    std::string accessName;
    if (receiverExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverExpr, accessName) &&
        receiverExpr.args.size() == 2 && !receiverExpr.args.empty()) {
      const Expr &accessReceiver = receiverExpr.args.front();
      if (accessReceiver.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = findParamBinding(params, accessReceiver.name);
      if (!binding) {
        auto it = locals.find(accessReceiver.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      if (binding == nullptr) {
        return false;
      }
      if (getArgsPackElementType(*binding, typeTextOut)) {
        return true;
      }
    }
    return inferQueryExprTypeText(receiverExpr, params, locals, typeTextOut);
  };
  auto seedLambdaLocals = [&](const Expr &lambdaExpr,
                              std::unordered_map<std::string, BindingInfo> &lambdaLocals) -> bool {
    lambdaLocals.clear();
    bool captureAll = false;
    std::vector<std::string> captureNames;
    captureNames.reserve(lambdaExpr.lambdaCaptures.size());
    for (const auto &capture : lambdaExpr.lambdaCaptures) {
      const std::vector<std::string> tokens = runSemanticsValidatorExprCaptureSplitStep(capture);
      if (tokens.empty()) {
        return false;
      }
      if (tokens.size() == 1) {
        if (tokens[0] == "=" || tokens[0] == "&") {
          captureAll = true;
          continue;
        }
        captureNames.push_back(tokens[0]);
        continue;
      }
      captureNames.push_back(tokens.back());
    }
    auto addCapturedBinding = [&](const std::string &name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        lambdaLocals.emplace(name, *paramBinding);
        return true;
      }
      auto localIt = locals.find(name);
      if (localIt != locals.end()) {
        lambdaLocals.emplace(name, localIt->second);
        return true;
      }
      return false;
    };
    if (captureAll) {
      for (const auto &param : params) {
        lambdaLocals.emplace(param.name, param.binding);
      }
      for (const auto &entry : locals) {
        lambdaLocals.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &name : captureNames) {
        (void)addCapturedBinding(name);
      }
    }
    for (const auto &param : lambdaExpr.args) {
      if (!param.isBinding) {
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(
              param, lambdaExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      lambdaLocals[param.name] = std::move(binding);
    }
    return true;
  };
  auto inferLambdaBodyTypeText = [&](const Expr &lambdaExpr, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!seedLambdaLocals(lambdaExpr, lambdaLocals)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : lambdaExpr.bodyArguments) {
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(
                bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
          return false;
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), {}, lambdaLocals, binding, &bodyExpr);
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !binding.typeTemplateArg.empty();
          if (!restrictMatchesBinding(
                  *restrictType, binding.typeName, binding.typeTemplateArg, hasTemplate, bodyExpr.namespacePrefix)) {
            return false;
          }
        }
        lambdaLocals[bodyExpr.name] = std::move(binding);
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        break;
      }
      valueExpr = &bodyExpr;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    BindingInfo inferredBinding;
    if (inferBindingTypeFromInitializer(*valueExpr, {}, lambdaLocals, inferredBinding)) {
      typeTextOut = bindingTypeText(inferredBinding);
      if (!typeTextOut.empty()) {
        return true;
      }
    }
    return inferQueryExprTypeText(*valueExpr, {}, lambdaLocals, typeTextOut) && !typeTextOut.empty();
  };
  auto inferLambdaBodyResultType = [&](const Expr &lambdaExpr,
                                       const std::string &preferredErrorType,
                                       ResultTypeInfo &resultOut) -> bool {
    resultOut = ResultTypeInfo{};
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!seedLambdaLocals(lambdaExpr, lambdaLocals)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : lambdaExpr.bodyArguments) {
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(
                bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
          return false;
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), {}, lambdaLocals, binding, &bodyExpr);
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !binding.typeTemplateArg.empty();
          if (!restrictMatchesBinding(
                  *restrictType, binding.typeName, binding.typeTemplateArg, hasTemplate, bodyExpr.namespacePrefix)) {
            return false;
          }
        }
        lambdaLocals[bodyExpr.name] = std::move(binding);
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        break;
      }
      valueExpr = &bodyExpr;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (resolveDirectResultOkType(*valueExpr, {}, lambdaLocals, preferredErrorType, resultOut)) {
      return true;
    }
    if (!resolveResultTypeForExpr(*valueExpr, {}, lambdaLocals, resultOut) || !resultOut.isResult) {
      return false;
    }
    if (!preferredErrorType.empty() && resultOut.errorType == "_") {
      resultOut.errorType = preferredErrorType;
    }
    return true;
  };
  auto resolveMethodResultPath = [&]() -> std::string {
    if (!expr.isMethodCall || expr.args.empty()) {
      return "";
    }
    const BuiltinCollectionDispatchResolvers dispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals);
    std::string resolvedPath;
    if (resolveLeadingNonCollectionAccessReceiverPath(
            params, locals, expr.args.front(), expr.name, dispatchResolvers, resolvedPath)) {
      return resolvedPath;
    }
    const Expr &receiver = expr.args.front();
    const std::string receiverTypeName = [&]() -> std::string {
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          return paramBinding->typeName;
        }
        auto localIt = locals.find(receiver.name);
        if (localIt != locals.end()) {
          return localIt->second.typeName;
        }
        if (isPrimitiveBindingTypeName(receiver.name)) {
          return receiver.name;
        }
        const std::string rootReceiverPath = "/" + receiver.name;
        if (defMap_.find(rootReceiverPath) != defMap_.end() || structNames_.count(rootReceiverPath) > 0) {
          return rootReceiverPath;
        }
        auto importIt = importAliases_.find(receiver.name);
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix, structNames_);
        if (!resolvedType.empty()) {
          return resolvedType;
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall) {
        const std::string resolvedReceiverPath = resolveCalleePath(receiver);
        if (!resolvedReceiverPath.empty()) {
          return resolvedReceiverPath;
        }
      }
      return std::string();
    }();
    if (receiverTypeName.empty() || receiverTypeName == "File") {
      return "";
    }
    if (isPrimitiveBindingTypeName(receiverTypeName)) {
      return "/" + normalizeBindingTypeName(receiverTypeName) + "/" + expr.name;
    }
    std::string resolvedType;
    if (!receiverTypeName.empty() && receiverTypeName.front() == '/') {
      resolvedType = receiverTypeName;
    } else {
      resolvedType = resolveStructTypePath(receiverTypeName, receiver.namespacePrefix, structNames_);
      if (resolvedType.empty()) {
        auto importIt = importAliases_.find(receiverTypeName);
        if (importIt != importAliases_.end()) {
          resolvedType = importIt->second;
        }
      }
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(receiverTypeName, receiver.namespacePrefix);
      }
    }
    if (resolvedType.empty()) {
      return "";
    }
    return resolvedType + "/" + expr.name;
    return "";
  };
  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Result") {
        return resolveResultTypeFromTemplateArg(paramBinding->typeTemplateArg, out);
      }
    }
    auto it = locals.find(expr.name);
    if (it != locals.end() && it->second.typeName == "Result") {
      return resolveResultTypeFromTemplateArg(it->second.typeTemplateArg, out);
    }
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (resolveDirectResultOkType(expr, params, locals, "", out)) {
    return true;
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "File") && expr.templateArgs.size() == 1) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = "File<" + expr.templateArgs.front() + ">";
    out.errorType = "FileError";
    return true;
  }
  if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
    std::string pointeeType;
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      if (resolveBindingTypeText(target.name, pointeeType)) {
        return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(pointeeType), out);
      }
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 && !target.args.empty()) {
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name && resolveBindingTypeText(receiver.name, pointeeType)) {
        std::string elemType;
        if (resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(pointeeType), out)) {
          return true;
        }
        if (const BindingInfo *binding = findParamBinding(params, receiver.name)) {
          if (getArgsPackElementType(*binding, elemType)) {
            return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
          }
        } else if (auto it = locals.find(receiver.name); it != locals.end()) {
          if (getArgsPackElementType(it->second, elemType)) {
            return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
          }
        }
      }
    }
  }
  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 && !expr.args.empty()) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *binding = findParamBinding(params, receiver.name);
      if (!binding) {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      std::string elemType;
      if (binding != nullptr && getArgsPackElementType(*binding, elemType)) {
        return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
      }
    }
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Name &&
        normalizeBindingTypeName(expr.args.front().name) == "Result") {
      if ((expr.name == "map" || expr.name == "and_then") && expr.args.size() == 3) {
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult ||
            !argResult.hasValue) {
          return false;
        }
        if (expr.name == "map") {
          std::string mappedType;
          if (!inferLambdaBodyTypeText(expr.args[2], mappedType) || mappedType.empty()) {
            return false;
          }
          out.isResult = true;
          out.hasValue = true;
          out.valueType = std::move(mappedType);
          out.errorType = argResult.errorType;
          return true;
        }
        ResultTypeInfo chainedResult;
        if (!inferLambdaBodyResultType(expr.args[2], argResult.errorType, chainedResult) || !chainedResult.isResult) {
          return false;
        }
        if (!chainedResult.errorType.empty() && !argResult.errorType.empty() &&
            !errorTypesMatch(chainedResult.errorType, argResult.errorType, expr.namespacePrefix)) {
          return false;
        }
        out = std::move(chainedResult);
        return true;
      }
      if (expr.name == "map2" && expr.args.size() == 4) {
        ResultTypeInfo leftResult;
        ResultTypeInfo rightResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, leftResult) || !leftResult.isResult ||
            !leftResult.hasValue ||
            !resolveResultTypeForExpr(expr.args[2], params, locals, rightResult) || !rightResult.isResult ||
            !rightResult.hasValue) {
          return false;
        }
        if (!errorTypesMatch(leftResult.errorType, rightResult.errorType, expr.namespacePrefix)) {
          return false;
        }
        std::string mappedType;
        if (!inferLambdaBodyTypeText(expr.args[3], mappedType) || mappedType.empty()) {
          return false;
        }
        out.isResult = true;
        out.hasValue = true;
        out.valueType = std::move(mappedType);
        out.errorType = leftResult.errorType;
        return true;
      }
    }
    if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
        expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = "FileError";
      return true;
    }
    const std::string resolvedMethodPath = resolveMethodResultPath();
    if (!resolvedMethodPath.empty()) {
      auto it = defMap_.find(resolvedMethodPath);
      if (it != defMap_.end()) {
        for (const auto &transform : it->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          return resolveResultTypeFromTypeName(transform.templateArgs.front(), out);
        }
      }
    }
    if (expr.name == "tryAt" && !expr.args.empty()) {
      std::string receiverTypeText;
      if (resolveMapReceiverTypeText(expr.args.front(), receiverTypeText) &&
          resolveBuiltinMapResultType(receiverTypeText)) {
        return true;
      }
    }
  }
  if (!expr.isMethodCall) {
    const std::string resolved = resolveCalleePath(expr);
    auto it = defMap_.find(resolved);
    if (it != defMap_.end()) {
      for (const auto &transform : it->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return resolveResultTypeFromTypeName(transform.templateArgs.front(), out);
      }
    }
    if ((resolved == "/std/collections/map/tryAt" || resolved == "/map/tryAt" || isSimpleCallName(expr, "tryAt")) &&
        !expr.args.empty()) {
      const Expr *receiverExpr = &expr.args.front();
      if (hasNamedArguments(expr.argNames)) {
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
              *expr.argNames[i] == "values") {
            receiverExpr = &expr.args[i];
            break;
          }
        }
      }
      std::string receiverTypeText;
      if (resolveMapReceiverTypeText(*receiverExpr, receiverTypeText) &&
          resolveBuiltinMapResultType(receiverTypeText)) {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::errorTypesMatch(const std::string &left,
                                         const std::string &right,
                                         const std::string &namespacePrefix) const {
  const auto stripInnerWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        out.push_back(static_cast<char>(ch));
      }
    }
    return out;
  };
  const auto normalizeType = [&](const std::string &name) -> std::string {
    const std::string trimmed = trimText(name);
    const std::string normalized = normalizeBindingTypeName(trimmed);
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(normalized, base, arg) &&
        (isBuiltinTemplateTypeName(base) || base == "array" || base == "vector" || base == "soa_vector" ||
         base == "map" || base == "Result" || base == "File")) {
      return stripInnerWhitespace(normalized);
    }
    if (normalized != trimmed || isPrimitiveBindingTypeName(trimmed)) {
      return stripInnerWhitespace(normalized);
    }
    if (!trimmed.empty() && trimmed[0] == '/') {
      return stripInnerWhitespace(trimmed);
    }
    if (auto aliasIt = importAliases_.find(trimmed); aliasIt != importAliases_.end()) {
      return stripInnerWhitespace(aliasIt->second);
    }
    return stripInnerWhitespace(resolveTypePath(trimmed, namespacePrefix));
  };
  return normalizeType(left) == normalizeType(right);
}

bool SemanticsValidator::parseOnErrorTransform(const std::vector<Transform> &transforms,
                                               const std::string &namespacePrefix,
                                               const std::string &context,
                                               std::optional<OnErrorHandler> &out) {
  out.reset();
  for (const auto &transform : transforms) {
    if (transform.name != "on_error") {
      continue;
    }
    if (out.has_value()) {
      error_ = "duplicate on_error transform on " + context;
      return false;
    }
    if (transform.templateArgs.size() != 2) {
      error_ = "on_error requires exactly two template arguments on " + context;
      return false;
    }
    OnErrorHandler handler;
    handler.errorType = transform.templateArgs.front();
    Expr handlerExpr;
    handlerExpr.kind = Expr::Kind::Name;
    handlerExpr.name = transform.templateArgs[1];
    handlerExpr.namespacePrefix = namespacePrefix;
    handler.handlerPath = resolveCalleePath(handlerExpr);
    if (defMap_.count(handler.handlerPath) == 0) {
      error_ = "unknown on_error handler: " + handler.handlerPath;
      return false;
    }
    auto paramIt = paramsByDef_.find(handler.handlerPath);
    if (paramIt == paramsByDef_.end() || paramIt->second.empty()) {
      error_ = "on_error handler requires an error parameter: " + handler.handlerPath;
      return false;
    }
    const BindingInfo &paramBinding = paramIt->second.front().binding;
    if (!errorTypesMatch(handler.errorType, paramBinding.typeName, namespacePrefix)) {
      error_ = "on_error handler error type mismatch: " + handler.handlerPath;
      return false;
    }
    handler.boundArgs.reserve(transform.arguments.size());
    for (const auto &argText : transform.arguments) {
      Expr argExpr;
      if (!parseTransformArgumentExpr(argText, namespacePrefix, argExpr)) {
        if (error_.empty()) {
          error_ = "invalid on_error argument on " + context;
        }
        return false;
      }
      handler.boundArgs.push_back(std::move(argExpr));
    }
    out = std::move(handler);
  }
  return true;
}

bool SemanticsValidator::exprUsesName(const Expr &expr, const std::string &name) const {
  if (name.empty()) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    return true;
  }
  if (expr.isLambda) {
    for (const auto &capture : expr.lambdaCaptures) {
      std::string token;
      std::vector<std::string> tokens;
      for (char c : capture) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
          if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
          }
          continue;
        }
        token.push_back(c);
      }
      if (!token.empty()) {
        tokens.push_back(token);
      }
      if (tokens.size() == 1 && tokens.front() == name) {
        return true;
      }
      if (tokens.size() == 2 && tokens.back() == name) {
        return true;
      }
    }
    for (const auto &param : expr.args) {
      for (const auto &defaultArg : param.args) {
        if (exprUsesName(defaultArg, name)) {
          return true;
        }
      }
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (exprUsesName(bodyExpr, name)) {
        return true;
      }
    }
    return false;
  }
  for (const auto &arg : expr.args) {
    if (exprUsesName(arg, name)) {
      return true;
    }
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    if (exprUsesName(bodyExpr, name)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::statementsUseNameFrom(const std::vector<Expr> &statements,
                                               size_t startIndex,
                                               const std::string &name) const {
  for (size_t index = startIndex; index < statements.size(); ++index) {
    if (exprUsesName(statements[index], name)) {
      return true;
    }
  }
  return false;
}

void SemanticsValidator::expireReferenceBorrowsForRemainder(const std::vector<ParameterInfo> &params,
                                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                                            const std::vector<Expr> &statements,
                                                            size_t nextIndex) {
  std::function<bool(const Expr &, const std::string &, bool)> exprUsesNameOutsideWriteTarget;
  exprUsesNameOutsideWriteTarget = [&](const Expr &expr, const std::string &name, bool inWriteTarget) -> bool {
    if (name.empty()) {
      return false;
    }
    if (expr.kind == Expr::Kind::Name && expr.name == name) {
      return !inWriteTarget;
    }
    if (expr.isLambda) {
      if (!inWriteTarget) {
        for (const auto &capture : expr.lambdaCaptures) {
          std::string token;
          std::vector<std::string> tokens;
          for (char c : capture) {
            if (std::isspace(static_cast<unsigned char>(c)) != 0) {
              if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
              }
              continue;
            }
            token.push_back(c);
          }
          if (!token.empty()) {
            tokens.push_back(token);
          }
          if ((tokens.size() == 1 && tokens.front() == name) || (tokens.size() == 2 && tokens.back() == name)) {
            return true;
          }
        }
      }
      for (const auto &param : expr.args) {
        for (const auto &defaultArg : param.args) {
          if (exprUsesNameOutsideWriteTarget(defaultArg, name, false)) {
            return true;
          }
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    if (isAssignCall(expr) && !expr.args.empty()) {
      if (exprUsesNameOutsideWriteTarget(expr.args.front(), name, true)) {
        return true;
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (exprUsesNameOutsideWriteTarget(expr.args[i], name, false)) {
          return true;
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && !expr.args.empty()) {
      if (exprUsesNameOutsideWriteTarget(expr.args.front(), name, true)) {
        return true;
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (exprUsesNameOutsideWriteTarget(expr.args[i], name, false)) {
          return true;
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    for (const auto &arg : expr.args) {
      if (exprUsesNameOutsideWriteTarget(arg, name, inWriteTarget)) {
        return true;
      }
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (exprUsesNameOutsideWriteTarget(bodyExpr, name, inWriteTarget)) {
        return true;
      }
    }
    return false;
  };
  auto statementsUseNameOutsideWriteTargetsFrom =
      [&](const std::vector<Expr> &candidates, size_t startIndex, const std::string &name) -> bool {
    for (size_t index = startIndex; index < candidates.size(); ++index) {
      if (exprUsesNameOutsideWriteTarget(candidates[index], name, false)) {
        return true;
      }
    }
    return false;
  };
  auto referenceRootForBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
    if (binding.typeName != "Reference") {
      return "";
    }
    if (!binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return bindingName;
  };
  auto pointerAliasUsesReferenceRoot = [&](const std::string &referenceRoot) -> bool {
    if (referenceRoot.empty()) {
      return false;
    }
    auto pointerUsesRoot = [&](const std::string &bindingName, const BindingInfo &binding) -> bool {
      if (binding.typeName != "Pointer" || binding.referenceRoot != referenceRoot) {
        return false;
      }
      return statementsUseNameOutsideWriteTargetsFrom(statements, nextIndex, bindingName);
    };
    for (const auto &param : params) {
      if (pointerUsesRoot(param.name, param.binding)) {
        return true;
      }
    }
    for (const auto &entry : locals) {
      if (pointerUsesRoot(entry.first, entry.second)) {
        return true;
      }
    }
    return false;
  };
  auto updateName = [&](const std::string &bindingName, const BindingInfo &binding) {
    if (binding.typeName != "Reference") {
      return;
    }
    const std::string referenceRoot = referenceRootForBinding(bindingName, binding);
    if (statementsUseNameFrom(statements, nextIndex, bindingName) ||
        pointerAliasUsesReferenceRoot(referenceRoot)) {
      currentValidationContext_.endedReferenceBorrows.erase(bindingName);
      return;
    }
    currentValidationContext_.endedReferenceBorrows.insert(bindingName);
  };
  for (const auto &param : params) {
    updateName(param.name, param.binding);
  }
  for (const auto &entry : locals) {
    updateName(entry.first, entry.second);
  }
}

} // namespace primec::semantics

#include "SemanticsValidator.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <optional>

namespace primec::semantics {
namespace {

bool isBridgeHelperName(std::string_view collectionFamily, std::string_view helperName) {
  if (collectionFamily == "vector") {
    return helperName == "count" || helperName == "capacity" || helperName == "at" ||
           helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
           helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
           helperName == "remove_swap";
  }
  if (collectionFamily == "map") {
    return helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
           helperName == "at" || helperName == "at_unsafe" || helperName == "insert" ||
           helperName == "mapInsertRef";
  }
  if (collectionFamily == "soa_vector") {
    return helperName == "count" || helperName == "get" || helperName == "ref" ||
           helperName == "to_aos" || helperName == "push" || helperName == "reserve";
  }
  return false;
}

std::optional<std::pair<std::string, std::string>>
collectionBridgeChoiceFromResolvedPath(const std::string &resolvedPath) {
  auto parsePrefixedHelper = [&](std::string_view prefix,
                                std::string_view collectionFamily)
      -> std::optional<std::pair<std::string, std::string>> {
    if (resolvedPath.rfind(prefix, 0) != 0) {
      return std::nullopt;
    }
    std::string helperName = resolvedPath.substr(prefix.size());
    const size_t specializationSuffix = helperName.find("__t");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
    if (!isBridgeHelperName(collectionFamily, helperName)) {
      return std::nullopt;
    }
    return std::pair<std::string, std::string>(std::string(collectionFamily), std::move(helperName));
  };

  if (auto parsed = parsePrefixedHelper("/vector/", "vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/vector/", "vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/experimental_vector/", "vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/map/", "map")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/map/", "map")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/experimental_map/", "map")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/soa_vector/", "soa_vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/soa_vector/", "soa_vector")) {
    return parsed;
  }
  return std::nullopt;
}

std::vector<std::string> snapshotCapabilities(const std::vector<Transform> &transforms) {
  std::vector<std::string> capabilities;
  for (const auto &transform : transforms) {
    if (transform.name != "capabilities") {
      continue;
    }
    capabilities = transform.arguments;
    break;
  }
  std::sort(capabilities.begin(), capabilities.end());
  capabilities.erase(std::unique(capabilities.begin(), capabilities.end()), capabilities.end());
  return capabilities;
}

bool isStaticFieldStatement(const Expr &stmt) {
  for (const auto &transform : stmt.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

bool parsePositiveIntSnapshotArg(const std::string &text, uint32_t &valueOut) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  uint64_t value = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    value = value * 10u + static_cast<uint64_t>(c - '0');
    if (value > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
  }
  valueOut = static_cast<uint32_t>(value);
  return valueOut > 0;
}

bool explicitAlignmentBytesForSnapshot(const std::vector<Transform> &transforms, uint32_t &alignmentOut) {
  alignmentOut = 0;
  for (const auto &transform : transforms) {
    if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
      continue;
    }
    if (transform.arguments.empty()) {
      return false;
    }
    uint32_t value = 0;
    if (!parsePositiveIntSnapshotArg(transform.arguments.front(), value)) {
      return false;
    }
    if (transform.name == "align_kbytes") {
      if (value > std::numeric_limits<uint32_t>::max() / 1024u) {
        return false;
      }
      value *= 1024u;
    }
    alignmentOut = value;
    return true;
  }
  return false;
}

bool isImplicitStructDefinitionSnapshot(const Definition &def) {
  bool hasStructTransform = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isStructTransformName(transform.name)) {
      hasStructTransform = true;
    }
  }
  if (hasStructTransform) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

std::string typeMetadataCategoryForSnapshot(const Definition &def) {
  bool sawStructLikeTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "enum") {
      return "enum";
    }
    if (transform.name == "gpu_lane") {
      return "gpu_lane";
    }
    if (transform.name == "handle") {
      return "handle";
    }
    if (transform.name == "pod") {
      return "pod";
    }
    if (transform.name == "struct") {
      return "struct";
    }
    if (isStructTransformName(transform.name)) {
      sawStructLikeTransform = true;
    }
  }
  if (sawStructLikeTransform || isImplicitStructDefinitionSnapshot(def)) {
    return "struct";
  }
  return {};
}

} // namespace

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
            expr.semanticNodeId,
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
        expr.semanticNodeId,
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
            expr.semanticNodeId,
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
      resolvedPath = resolveCalleePath(expr);
      if (resolvedPath.empty()) {
        return;
      }
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
        expr.semanticNodeId,
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

std::vector<SemanticsValidator::BridgePathChoiceSnapshotEntry>
SemanticsValidator::bridgePathChoiceSnapshotForSemanticProduct() const {
  std::vector<BridgePathChoiceSnapshotEntry> entries;
  std::function<void(const std::string &, const Expr &)> visitExpr;
  auto visitExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      visitExpr(scopePath, expr);
    }
  };

  visitExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      const std::string resolvedPath = resolveCalleePath(expr);
      if (const auto bridgeChoice = collectionBridgeChoiceFromResolvedPath(resolvedPath);
          bridgeChoice.has_value()) {
        entries.push_back(BridgePathChoiceSnapshotEntry{
            scopePath,
            bridgeChoice->first,
            bridgeChoice->second,
            resolvedPath,
            expr.sourceLine,
            expr.sourceColumn,
            expr.semanticNodeId,
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
    if (left.collectionFamily != right.collectionFamily) {
      return left.collectionFamily < right.collectionFamily;
    }
    if (left.helperName != right.helperName) {
      return left.helperName < right.helperName;
    }
    return left.chosenPath < right.chosenPath;
  });
  return entries;
}

std::vector<SemanticsValidator::CallableSummarySnapshotEntry>
SemanticsValidator::callableSummarySnapshotForSemanticProduct() const {
  std::vector<CallableSummarySnapshotEntry> entries;
  entries.reserve(program_.definitions.size() + program_.executions.size());

  for (const auto &def : program_.definitions) {
    const auto state = buildDefinitionValidationState(def);
    const auto &context = state.context;
    ReturnKind returnKind = ReturnKind::Unknown;
    if (const auto returnKindIt = returnKinds_.find(def.fullPath); returnKindIt != returnKinds_.end()) {
      returnKind = returnKindIt->second;
    }
    entries.push_back(CallableSummarySnapshotEntry{
        def.fullPath,
        false,
        returnKind,
        context.definitionIsCompute,
        context.definitionIsUnsafe,
        std::vector<std::string>(context.activeEffects.begin(), context.activeEffects.end()),
        snapshotCapabilities(def.transforms),
        context.resultType.has_value() && context.resultType->isResult,
        context.resultType.has_value() && context.resultType->isResult && context.resultType->hasValue,
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->valueType : std::string{},
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->errorType : std::string{},
        context.onError.has_value(),
        context.onError.has_value() ? context.onError->handlerPath : std::string{},
        context.onError.has_value() ? context.onError->errorType : std::string{},
        context.onError.has_value() ? context.onError->boundArgs.size() : 0,
        def.semanticNodeId,
    });
  }

  for (const auto &exec : program_.executions) {
    const auto state = buildExecutionValidationState(exec);
    const auto &context = state.context;
    entries.push_back(CallableSummarySnapshotEntry{
        exec.fullPath,
        true,
        ReturnKind::Unknown,
        context.definitionIsCompute,
        context.definitionIsUnsafe,
        std::vector<std::string>(context.activeEffects.begin(), context.activeEffects.end()),
        snapshotCapabilities(exec.transforms),
        false,
        false,
        {},
        {},
        false,
        {},
        {},
        0,
        exec.semanticNodeId,
    });
  }

  for (auto &entry : entries) {
    std::sort(entry.activeEffects.begin(), entry.activeEffects.end());
    entry.activeEffects.erase(std::unique(entry.activeEffects.begin(), entry.activeEffects.end()),
                              entry.activeEffects.end());
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.fullPath != right.fullPath) {
      return left.fullPath < right.fullPath;
    }
    return left.isExecution < right.isExecution;
  });
  return entries;
}

std::vector<SemanticsValidator::TypeMetadataSnapshotEntry>
SemanticsValidator::typeMetadataSnapshotForSemanticProduct() const {
  std::vector<TypeMetadataSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());

  for (const auto &def : program_.definitions) {
    const std::string category = typeMetadataCategoryForSnapshot(def);
    if (category.empty()) {
      continue;
    }

    bool hasNoPadding = false;
    bool hasPlatformIndependentPadding = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "no_padding") {
        hasNoPadding = true;
      } else if (transform.name == "platform_independent_padding") {
        hasPlatformIndependentPadding = true;
      }
    }

    uint32_t explicitAlignmentBytes = 0;
    const bool hasExplicitAlignment = explicitAlignmentBytesForSnapshot(def.transforms, explicitAlignmentBytes);

    size_t fieldCount = 0;
    size_t enumValueCount = 0;
    if (category == "enum") {
      enumValueCount = def.statements.size();
    } else {
      for (const auto &stmt : def.statements) {
        if (stmt.isBinding && !isStaticFieldStatement(stmt)) {
          ++fieldCount;
        }
      }
    }

    entries.push_back(TypeMetadataSnapshotEntry{
        def.fullPath,
        category,
        publicDefinitions_.count(def.fullPath) > 0,
        hasNoPadding,
        hasPlatformIndependentPadding,
        hasExplicitAlignment,
        explicitAlignmentBytes,
        fieldCount,
        enumValueCount,
        def.sourceLine,
        def.sourceColumn,
        def.semanticNodeId,
    });
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.fullPath != right.fullPath) {
      return left.fullPath < right.fullPath;
    }
    return left.category < right.category;
  });
  return entries;
}

std::vector<SemanticsValidator::StructFieldMetadataSnapshotEntry>
SemanticsValidator::structFieldMetadataSnapshotForSemanticProduct() {
  std::vector<StructFieldMetadataSnapshotEntry> entries;

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  for (const auto &def : program_.definitions) {
    const std::string category = typeMetadataCategoryForSnapshot(def);
    if (category.empty() || category == "enum") {
      continue;
    }

    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticFieldStatement(stmt)) {
        continue;
      }

      BindingInfo binding;
      if (!withPreservedError([&]() { return resolveStructFieldBinding(def, stmt, binding); })) {
        continue;
      }

      entries.push_back(StructFieldMetadataSnapshotEntry{
          def.fullPath,
          stmt.name,
          fieldIndex,
          stmt.sourceLine,
          stmt.sourceColumn,
          std::move(binding),
          stmt.semanticNodeId,
      });
      ++fieldIndex;
    }
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.structPath != right.structPath) {
      return left.structPath < right.structPath;
    }
    if (left.fieldIndex != right.fieldIndex) {
      return left.fieldIndex < right.fieldIndex;
    }
    return left.fieldName < right.fieldName;
  });
  return entries;
}

std::vector<SemanticsValidator::BindingFactSnapshotEntry>
SemanticsValidator::bindingFactSnapshotForSemanticProduct() {
  using ActiveLocalBindings = std::unordered_map<std::string, BindingInfo>;

  std::vector<BindingFactSnapshotEntry> entries;

  for (const auto &def : program_.definitions) {
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      continue;
    }
    const auto &defParams = paramsIt->second;
    const size_t syntheticLeadingParamCount =
        (defParams.size() > def.parameters.size() && !defParams.empty() && defParams.front().name == "this")
            ? (defParams.size() - def.parameters.size())
            : 0;
    const size_t paramCount =
        std::min(def.parameters.size(), defParams.size() - std::min(defParams.size(), syntheticLeadingParamCount));
    for (size_t i = 0; i < paramCount; ++i) {
      entries.push_back(BindingFactSnapshotEntry{
          def.fullPath,
          "parameter",
          defParams[syntheticLeadingParamCount + i].name,
          {},
          def.parameters[i].sourceLine,
          def.parameters[i].sourceColumn,
          defParams[syntheticLeadingParamCount + i].binding,
          def.parameters[i].semanticNodeId,
      });
    }
  }

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
        entries.push_back(BindingFactSnapshotEntry{
            def.fullPath,
            "local",
            expr.name,
            {},
            expr.sourceLine,
            expr.sourceColumn,
            binding,
            expr.semanticNodeId,
        });
        activeLocals.emplace(expr.name, std::move(binding));
      }
      return;
    }

    if (expr.kind == Expr::Kind::Call) {
      CallSnapshotData callData;
      if (inferCallSnapshotData(defParams, activeLocals, expr, callData) &&
          !callData.binding.typeName.empty()) {
        entries.push_back(BindingFactSnapshotEntry{
            def.fullPath,
            "temporary",
            expr.name,
            std::move(callData.resolvedPath),
            expr.sourceLine,
            expr.sourceColumn,
            std::move(callData.binding),
            expr.semanticNodeId,
        });
      }
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
    ValidationStateScope validationStateScope(*this, buildDefinitionValidationState(def));
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      continue;
    }
    const auto &defParams = paramsIt->second;

    ActiveLocalBindings definitionLocals;
    visitExprSequence(def, defParams, def.statements, definitionLocals);
    if (def.returnExpr.has_value()) {
      ActiveLocalBindings returnLocals = definitionLocals;
      visitExpr(def, defParams, *def.returnExpr, returnLocals);
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
    if (left.siteKind != right.siteKind) {
      return left.siteKind < right.siteKind;
    }
    if (left.name != right.name) {
      return left.name < right.name;
    }
    return left.resolvedPath < right.resolvedPath;
  });
  return entries;
}

std::vector<SemanticsValidator::ReturnFactSnapshotEntry>
SemanticsValidator::returnFactSnapshotForSemanticProduct() {
  std::vector<ReturnFactSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  for (const auto &definition : program_.definitions) {
    const auto kindIt = returnKinds_.find(definition.fullPath);
    if (kindIt == returnKinds_.end()) {
      continue;
    }
    ReturnFactSnapshotEntry entry;
    entry.definitionPath = definition.fullPath;
    entry.kind = kindIt->second;
    entry.sourceLine = definition.returnExpr.has_value() ? definition.returnExpr->sourceLine : definition.sourceLine;
    entry.sourceColumn =
        definition.returnExpr.has_value() ? definition.returnExpr->sourceColumn : definition.sourceColumn;
    entry.semanticNodeId = definition.semanticNodeId;
    if (const auto structIt = returnStructs_.find(definition.fullPath);
        structIt != returnStructs_.end()) {
      entry.structPath = structIt->second;
    }
    if (const auto bindingIt = returnBindings_.find(definition.fullPath);
        bindingIt != returnBindings_.end()) {
      entry.binding = bindingIt->second;
    }
    if (entry.binding.typeName.empty()) {
      BindingInfo inferredBinding;
      if (withPreservedError([&]() {
            return inferDefinitionReturnBinding(definition, inferredBinding);
          })) {
        entry.binding = std::move(inferredBinding);
      }
    }
    if (entry.binding.typeName.empty()) {
      if (entry.kind == ReturnKind::Array && !entry.structPath.empty()) {
        entry.binding.typeName = entry.structPath;
      } else if (entry.kind == ReturnKind::Void) {
        entry.binding.typeName = "void";
      } else if (entry.kind != ReturnKind::Unknown) {
        entry.binding.typeName = typeNameForReturnKind(entry.kind);
      }
    }
    entries.push_back(std::move(entry));
  }
  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.definitionPath < right.definitionPath;
  });
  return entries;
}

std::vector<SemanticsValidator::LocalAutoBindingSnapshotEntry>
SemanticsValidator::localAutoFactSnapshotForSemanticProduct() const {
  return localAutoBindingSnapshotForTesting();
}

std::vector<SemanticsValidator::QueryFactSnapshotEntry>
SemanticsValidator::queryFactSnapshotForSemanticProduct() {
  std::vector<QueryFactSnapshotEntry> entries;
  forEachLocalAwareSnapshotCall([&](const Definition &def,
                                    const std::vector<ParameterInfo> &defParams,
                                    const Expr &expr,
                                    const std::unordered_map<std::string, BindingInfo> &activeLocals) {
    QuerySnapshotData queryData;
    if (!inferQuerySnapshotData(defParams, activeLocals, expr, queryData)) {
      return;
    }
    entries.push_back(QueryFactSnapshotEntry{
        def.fullPath,
        expr.name,
        std::move(queryData.resolvedPath),
        expr.sourceLine,
        expr.sourceColumn,
        std::move(queryData.typeText),
        std::move(queryData.binding),
        std::move(queryData.receiverBinding),
        queryData.resultInfo.isResult,
        queryData.resultInfo.isResult && queryData.resultInfo.hasValue,
        std::move(queryData.resultInfo.valueType),
        std::move(queryData.resultInfo.errorType),
        expr.semanticNodeId,
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
    if (left.callName != right.callName) {
      return left.callName < right.callName;
    }
    return left.resolvedPath < right.resolvedPath;
  });
  return entries;
}

std::vector<SemanticsValidator::TryValueSnapshotEntry>
SemanticsValidator::tryFactSnapshotForSemanticProduct() {
  return tryValueSnapshotForTesting();
}

std::vector<SemanticsValidator::OnErrorSnapshotEntry>
SemanticsValidator::onErrorFactSnapshotForSemanticProduct() {
  return onErrorSnapshotForTesting();
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

    std::vector<std::string> boundArgTexts;
    boundArgTexts.reserve(context.onError->boundArgs.size());
    for (const auto &transform : def.transforms) {
      if (transform.name != "on_error") {
        continue;
      }
      boundArgTexts = transform.arguments;
      break;
    }

    entries.push_back(OnErrorSnapshotEntry{
        def.fullPath,
        returnKind,
        context.onError->handlerPath,
        context.onError->errorType,
        context.onError->boundArgs.size(),
        std::move(boundArgTexts),
        context.resultType.has_value() && context.resultType->isResult && context.resultType->hasValue,
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->valueType : std::string{},
        context.resultType.has_value() && context.resultType->isResult ? context.resultType->errorType : std::string{},
        def.semanticNodeId,
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

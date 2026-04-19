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
    return helperName == "count" || helperName == "count_ref" ||
           helperName == "contains" || helperName == "contains_ref" ||
           helperName == "tryAt" || helperName == "tryAt_ref" ||
           helperName == "at" || helperName == "at_ref" ||
           helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
           helperName == "insert" || helperName == "insert_ref" || helperName == "mapInsert" ||
           helperName == "mapCountRef" || helperName == "mapContainsRef" ||
           helperName == "mapTryAtRef" || helperName == "mapAtRef" ||
           helperName == "mapAtUnsafeRef" || helperName == "mapInsertRef";
  }
  if (collectionFamily == "soa_vector") {
    return helperName == "count" || helperName == "count_ref" ||
           helperName == "get" || helperName == "get_ref" ||
           helperName == "ref" || helperName == "ref_ref" ||
           helperName == "to_aos" || helperName == "to_aos_ref" ||
           helperName == "push" || helperName == "reserve";
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

template <typename ResolveFn, typename VisitorFn>
void forEachResolvedNonMethodCallSnapshot(const Program &program,
                                          ResolveFn &&resolvePath,
                                          VisitorFn &&visitor) {
  std::function<void(const std::string &, const Expr &)> visitExpr;
  auto visitExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      visitExpr(scopePath, expr);
    }
  };

  visitExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      std::string resolvedPath = resolvePath(expr);
      if (!resolvedPath.empty()) {
        visitor(scopePath, expr, std::move(resolvedPath));
      }
    }
    visitExprs(scopePath, expr.args);
    visitExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    visitExprs(def.fullPath, def.parameters);
    visitExprs(def.fullPath, def.statements);
    if (def.returnExpr.has_value()) {
      visitExpr(def.fullPath, *def.returnExpr);
    }
  }

  for (const auto &exec : program.executions) {
    visitExprs(exec.fullPath, exec.arguments);
    visitExprs(exec.fullPath, exec.bodyArguments);
  }
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
  std::vector<LocalAutoBindingSnapshotEntry> entries;
  std::function<void(const std::string &, const Expr &)> visitExpr;
  visitExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding) {
      const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(expr);
      const GraphLocalAutoKey bindingKey =
          graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn);
      const auto factIt = graphLocalAutoFacts_.find(bindingKey);
      if (factIt != graphLocalAutoFacts_.end() && factIt->second.hasBinding) {
          const GraphLocalAutoFacts &fact = factIt->second;
          std::string initializerResolvedPath;
          if (!fact.initializerResolvedPath.empty()) {
            initializerResolvedPath = fact.initializerResolvedPath;
          }
          std::string initializerDirectCallResolvedPath;
          if (!fact.directCallResolvedPath.empty()) {
            initializerDirectCallResolvedPath = fact.directCallResolvedPath;
          }
          ReturnKind initializerDirectCallReturnKind = ReturnKind::Unknown;
          if (fact.hasDirectCallReturnKind) {
            initializerDirectCallReturnKind = fact.directCallReturnKind;
          }
          std::string initializerMethodCallResolvedPath;
          if (!fact.methodCallResolvedPath.empty()) {
            initializerMethodCallResolvedPath = fact.methodCallResolvedPath;
          }
          ReturnKind initializerMethodCallReturnKind = ReturnKind::Unknown;
          if (fact.hasMethodCallReturnKind) {
            initializerMethodCallReturnKind = fact.methodCallReturnKind;
          }
          BindingInfo initializerBinding;
          if (fact.hasInitializerBinding) {
            initializerBinding = fact.initializerBinding;
          }
          BindingInfo initializerReceiverBinding;
          std::string initializerQueryTypeText;
          bool initializerResultHasValue = false;
          std::string initializerResultValueType;
          std::string initializerResultErrorType;
          if (fact.hasQuerySnapshot) {
            const QuerySnapshotData &querySnapshot = fact.querySnapshot;
            initializerReceiverBinding = querySnapshot.receiverBinding;
            initializerQueryTypeText = querySnapshot.typeText;
            if (querySnapshot.resultInfo.isResult) {
              initializerResultHasValue = querySnapshot.resultInfo.hasValue;
              initializerResultValueType = querySnapshot.resultInfo.valueType;
              initializerResultErrorType = querySnapshot.resultInfo.errorType;
            }
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
          if (fact.hasTryValue) {
            const LocalAutoTrySnapshotData &tryValue = fact.tryValue;
            initializerHasTry = true;
            initializerTryOperandResolvedPath = tryValue.operandResolvedPath;
            initializerTryOperandBinding = tryValue.operandBinding;
            initializerTryOperandReceiverBinding = tryValue.operandReceiverBinding;
            initializerTryOperandQueryTypeText = tryValue.operandQueryTypeText;
            initializerTryValueType = tryValue.valueType;
            initializerTryErrorType = tryValue.errorType;
            initializerTryContextReturnKind = tryValue.contextReturnKind;
            initializerTryOnErrorHandlerPath = tryValue.onErrorHandlerPath;
            initializerTryOnErrorErrorType = tryValue.onErrorErrorType;
            initializerTryOnErrorBoundArgCount = tryValue.onErrorBoundArgCount;
          }
          entries.push_back(LocalAutoBindingSnapshotEntry{
              scopePath,
              expr.name,
              sourceLine,
              sourceColumn,
              fact.binding,
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
              std::move(initializerDirectCallResolvedPath),
              initializerDirectCallReturnKind,
              std::move(initializerMethodCallResolvedPath),
              initializerMethodCallReturnKind,
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

std::vector<SemanticsValidator::CallBindingSnapshotEntry>
SemanticsValidator::callBindingSnapshotForTesting() {
  ensureCallAndTrySnapshotFactCaches(
      false, true);
  return callBindingSnapshotCache_;
}

void SemanticsValidator::collectPilotRoutingSemanticProductFacts() {
  if (pilotRoutingSemanticCollectorsValid_) {
    return;
  }

  collectedDirectCallTargets_.clear();
  collectedMethodCallTargets_.clear();
  collectedBridgePathChoices_.clear();
  collectedCallableSummaries_.clear();
  collectedDirectCallTargets_.reserve(program_.definitions.size());
  collectedMethodCallTargets_.reserve(program_.definitions.size());
  collectedBridgePathChoices_.reserve(program_.definitions.size());
  collectedCallableSummaries_.reserve(
      program_.definitions.size() + program_.executions.size());

  std::function<void(const std::string &, const Expr &)> collectDirectCallExpr;
  auto collectDirectCallExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      collectDirectCallExpr(scopePath, expr);
    }
  };

  collectDirectCallExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      std::string resolvedPath = resolveCalleePath(expr);
      if (!resolvedPath.empty()) {
        if (const auto bridgeChoice = collectionBridgeChoiceFromResolvedPath(resolvedPath);
            bridgeChoice.has_value()) {
          collectedBridgePathChoices_.push_back(CollectedBridgePathChoiceEntry{
              scopePath,
              bridgeChoice->first,
              bridgeChoice->second,
              resolvedPath,
              expr.sourceLine,
              expr.sourceColumn,
              expr.semanticNodeId,
          });
        }
        collectedDirectCallTargets_.push_back(CollectedDirectCallTargetEntry{
            scopePath,
            expr.name,
            std::move(resolvedPath),
            expr.sourceLine,
            expr.sourceColumn,
            expr.semanticNodeId,
        });
      }
    }
    collectDirectCallExprs(scopePath, expr.args);
    collectDirectCallExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    ValidationStateScope validationStateScope(*this, buildDefinitionValidationState(def));
    collectDirectCallExprs(def.fullPath, def.parameters);
    collectDirectCallExprs(def.fullPath, def.statements);
    if (def.returnExpr.has_value()) {
      collectDirectCallExpr(def.fullPath, *def.returnExpr);
    }
  }

  for (const auto &exec : program_.executions) {
    ExecutionContextScope executionScope(*this, exec);
    collectDirectCallExprs(exec.fullPath, exec.arguments);
    collectDirectCallExprs(exec.fullPath, exec.bodyArguments);
  }

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

    collectedMethodCallTargets_.push_back(CollectedMethodCallTargetEntry{
        def.fullPath,
        expr.name,
        std::move(resolvedPath),
        expr.sourceLine,
        expr.sourceColumn,
        std::move(receiverBinding),
        expr.semanticNodeId,
    });
  });

  for (const auto &def : program_.definitions) {
    const auto state = buildDefinitionValidationState(def);
    const auto &context = state.context;
    ReturnKind returnKind = ReturnKind::Unknown;
    if (const auto returnKindIt = returnKinds_.find(def.fullPath); returnKindIt != returnKinds_.end()) {
      returnKind = returnKindIt->second;
    }

    collectedCallableSummaries_.push_back(CollectedCallableSummaryEntry{
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
    collectedCallableSummaries_.push_back(CollectedCallableSummaryEntry{
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

  for (auto &entry : collectedCallableSummaries_) {
    std::sort(entry.activeEffects.begin(), entry.activeEffects.end());
    entry.activeEffects.erase(std::unique(entry.activeEffects.begin(), entry.activeEffects.end()),
                              entry.activeEffects.end());
  }

  std::stable_sort(collectedDirectCallTargets_.begin(),
                   collectedDirectCallTargets_.end(),
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
  std::stable_sort(collectedMethodCallTargets_.begin(),
                   collectedMethodCallTargets_.end(),
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
                     if (left.methodName != right.methodName) {
                       return left.methodName < right.methodName;
                     }
                     return left.resolvedPath < right.resolvedPath;
                   });
  std::stable_sort(collectedBridgePathChoices_.begin(),
                   collectedBridgePathChoices_.end(),
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
                     if (left.collectionFamily != right.collectionFamily) {
                       return left.collectionFamily < right.collectionFamily;
                     }
                     if (left.helperName != right.helperName) {
                       return left.helperName < right.helperName;
                     }
                     return left.chosenPath < right.chosenPath;
                   });
  std::stable_sort(collectedCallableSummaries_.begin(),
                   collectedCallableSummaries_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.fullPath != right.fullPath) {
                       return left.fullPath < right.fullPath;
                     }
                     return left.isExecution < right.isExecution;
                   });
  pilotRoutingSemanticCollectorsValid_ = true;
}

std::vector<SemanticsValidator::CollectedDirectCallTargetEntry>
SemanticsValidator::takeCollectedDirectCallTargetsForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedDirectCallTargets_, {});
}

std::vector<SemanticsValidator::CollectedMethodCallTargetEntry>
SemanticsValidator::takeCollectedMethodCallTargetsForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedMethodCallTargets_, {});
}

std::vector<SemanticsValidator::CollectedBridgePathChoiceEntry>
SemanticsValidator::takeCollectedBridgePathChoicesForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedBridgePathChoices_, {});
}

std::vector<SemanticsValidator::CollectedCallableSummaryEntry>
SemanticsValidator::takeCollectedCallableSummariesForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedCallableSummaries_, {});
}

void SemanticsValidator::ensureOnErrorSnapshotFactCache() const {
  if (onErrorSnapshotFactCacheValid_) {
    return;
  }

  onErrorSnapshotCache_.clear();
  onErrorSnapshotCache_.reserve(program_.definitions.size());

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

    onErrorSnapshotCache_.push_back(OnErrorSnapshotEntry{
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

  std::stable_sort(onErrorSnapshotCache_.begin(),
                   onErrorSnapshotCache_.end(),
                   [](const auto &left, const auto &right) {
                     return left.definitionPath < right.definitionPath;
                   });
  onErrorSnapshotFactCacheValid_ = true;
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
        insertLocalBinding(activeLocals, expr.name, std::move(binding));
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
    ValidationStateScope validationStateScope(*this, buildDefinitionValidationState(def));
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      resetSnapshotInferenceCaches();
      continue;
    }
    const auto &defParams = paramsIt->second;

    ActiveLocalBindings definitionLocals;
    LocalBindingScope definitionScopeLocals(*this, definitionLocals);
    visitExprSequence(def, defParams, def.statements, definitionLocals, definitionScopeLocals);
    if (def.returnExpr.has_value()) {
      LocalBindingScope returnScope(*this, definitionLocals);
      visitExpr(def, defParams, *def.returnExpr, definitionLocals, returnScope);
    }
    resetSnapshotInferenceCaches();
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
  ensureQuerySnapshotFactCaches();
  queryFactSnapshotCacheValid_ = false;
  return std::exchange(queryFactSnapshotCache_, {});
}

std::vector<SemanticsValidator::TryValueSnapshotEntry>
SemanticsValidator::tryFactSnapshotForSemanticProduct() {
  ensureCallAndTrySnapshotFactCaches(
      true, false);
  tryValueSnapshotCacheValid_ = false;
  return std::exchange(tryValueSnapshotCache_, {});
}

std::vector<SemanticsValidator::OnErrorSnapshotEntry>
SemanticsValidator::onErrorFactSnapshotForSemanticProduct() {
  ensureOnErrorSnapshotFactCache();
  onErrorSnapshotFactCacheValid_ = false;
  return std::exchange(onErrorSnapshotCache_, {});
}

} // namespace primec::semantics

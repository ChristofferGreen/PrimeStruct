// Snapshots for tests, callable summaries / on-error collection and the semantic-product publication surface.
#include "SemanticsValidatorSnapshotHelpers.h"

namespace primec::semantics {
using namespace snapshot_detail;

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

std::vector<LocalAutoBindingSnapshotEntry>
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
          const bool hasDirectCallInitializer =
              expr.args.size() == 1 &&
              expr.args.front().kind == Expr::Kind::Call &&
              !expr.args.front().isMethodCall;
          if (!fact.directCallResolvedPath.empty()) {
            initializerDirectCallResolvedPath = fact.directCallResolvedPath;
          } else if (hasDirectCallInitializer && !fact.initializerResolvedPath.empty()) {
            initializerDirectCallResolvedPath = fact.initializerResolvedPath;
          }
          ReturnKind initializerDirectCallReturnKind = ReturnKind::Unknown;
          if (fact.hasDirectCallReturnKind) {
            initializerDirectCallReturnKind = fact.directCallReturnKind;
          } else if (hasDirectCallInitializer && fact.hasInitializerBinding) {
            initializerDirectCallReturnKind = returnKindForTypeName(
                normalizeBindingTypeName(fact.initializerBinding.typeName));
          }
          std::string initializerMethodCallResolvedPath;
          const bool hasMethodCallInitializer =
              expr.args.size() == 1 && expr.args.front().isMethodCall;
          if (!fact.methodCallResolvedPath.empty()) {
            initializerMethodCallResolvedPath = fact.methodCallResolvedPath;
          } else if (hasMethodCallInitializer && !fact.initializerResolvedPath.empty()) {
            initializerMethodCallResolvedPath = fact.initializerResolvedPath;
          }
          ReturnKind initializerMethodCallReturnKind = ReturnKind::Unknown;
          if (fact.hasMethodCallReturnKind) {
            initializerMethodCallReturnKind = fact.methodCallReturnKind;
          } else if (hasMethodCallInitializer && fact.hasInitializerBinding) {
            initializerMethodCallReturnKind = returnKindForTypeName(
                normalizeBindingTypeName(fact.initializerBinding.typeName));
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

void SemanticsValidator::collectCallableSummaryEntriesForStableRange(
    std::size_t stableOrderOffset,
    std::size_t stableOrderCount,
    std::vector<CollectedCallableSummaryEntry> &out) const {
  const std::size_t declarationCount =
      validationPlan_->definitionPrepass.declarationsInStableOrder.size();
  if (stableOrderCount == 0 || stableOrderOffset >= declarationCount) {
    return;
  }

  const std::size_t boundedCount =
      std::min(stableOrderCount, declarationCount - stableOrderOffset);
  out.reserve(out.size() + boundedCount);
  for (std::size_t stableOrdinal = 0; stableOrdinal < boundedCount; ++stableOrdinal) {
    const std::size_t stableIndex =
        validationPlan_->definitionPrepass
            .declarationsInStableOrder[stableOrderOffset + stableOrdinal]
            .stableIndex;
    const Definition &def = program_.definitions[stableIndex];
    if (hasSumTransformSnapshot(def)) {
      continue;
    }
    const auto state = buildDefinitionValidationState(def);
    const auto &context = state.context;
    ReturnKind returnKind = ReturnKind::Unknown;
    if (const auto returnKindIt = returnKinds_.find(def.fullPath);
        returnKindIt != returnKinds_.end()) {
      returnKind = returnKindIt->second;
    }

    out.push_back(CollectedCallableSummaryEntry{
        def.fullPath,
        false,
        returnKind,
        context.definitionIsCompute,
        context.definitionIsUnsafe,
        std::vector<std::string>(context.activeEffects.begin(), context.activeEffects.end()),
        snapshotCapabilities(def.transforms),
        context.resultType.has_value() && context.resultType->isResult,
        context.resultType.has_value() && context.resultType->isResult &&
            context.resultType->hasValue,
        context.resultType.has_value() && context.resultType->isResult
            ? context.resultType->valueType
            : std::string{},
        context.resultType.has_value() && context.resultType->isResult
            ? context.resultType->errorType
            : std::string{},
        context.onError.has_value(),
        context.onError.has_value() ? context.onError->handlerPath : std::string{},
        context.onError.has_value() ? context.onError->errorType : std::string{},
        context.onError.has_value() ? context.onError->boundArgs.size() : 0,
        def.semanticNodeId,
    });
  }
}

void SemanticsValidator::collectExecutionCallableSummaryEntries(
    std::vector<CollectedCallableSummaryEntry> &out) const {
  out.reserve(out.size() +
              validationPlan_->executionSlice.executionsInStableOrder.size());
  for (const auto &executionDeclaration :
       validationPlan_->executionSlice.executionsInStableOrder) {
    const Execution &exec = program_.executions[executionDeclaration.stableIndex];
    const auto state = buildExecutionValidationState(exec);
    const auto &context = state.context;
    out.push_back(CollectedCallableSummaryEntry{
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
}

void SemanticsValidator::rebindCollectedCallableSummarySemanticNodeIds(
    std::vector<CollectedCallableSummaryEntry> &entries) const {
  for (auto &entry : entries) {
    entry.semanticNodeId = 0;
    if (entry.isExecution) {
      const auto executionIt = std::find_if(
          validationPlan_->executionSlice.executionsInStableOrder.begin(),
          validationPlan_->executionSlice.executionsInStableOrder.end(),
          [&](const SemanticValidationExecutionDeclaration &executionDeclaration) {
            return executionDeclaration.fullPath == entry.fullPath;
          });
      if (executionIt != validationPlan_->executionSlice.executionsInStableOrder.end()) {
        entry.semanticNodeId =
            program_.executions[executionIt->stableIndex].semanticNodeId;
      }
      continue;
    }

    const auto defIt = std::find_if(
        program_.definitions.begin(),
        program_.definitions.end(),
        [&](const Definition &def) { return def.fullPath == entry.fullPath; });
    if (defIt != program_.definitions.end()) {
      entry.semanticNodeId = defIt->semanticNodeId;
    }
  }
}

void SemanticsValidator::rebindCollectedOnErrorSemanticNodeIds(
    std::vector<OnErrorSnapshotEntry> &entries) const {
  auto hashSemanticNodePath = [](const std::string &path) {
    constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
    constexpr uint64_t FnvPrime = 1099511628211ull;

    uint64_t hash = FnvOffsetBasis;
    for (unsigned char ch : path) {
      hash ^= static_cast<uint64_t>(ch);
      hash *= FnvPrime;
    }
    return hash == 0 ? 1 : hash;
  };
  for (auto &entry : entries) {
    entry.semanticNodeId =
        entry.definitionPath.empty()
            ? 0
            : hashSemanticNodePath("definition:" + entry.definitionPath);
  }
}

void SemanticsValidator::sortCollectedCallableSummaries(
    std::vector<CollectedCallableSummaryEntry> &entries) {
  for (auto &entry : entries) {
    std::sort(entry.activeEffects.begin(), entry.activeEffects.end());
    entry.activeEffects.erase(
        std::unique(entry.activeEffects.begin(), entry.activeEffects.end()),
        entry.activeEffects.end());
  }

  std::stable_sort(entries.begin(),
                   entries.end(),
                   [](const auto &left, const auto &right) {
                     if (left.fullPath != right.fullPath) {
                       return left.fullPath < right.fullPath;
                     }
                     return left.isExecution < right.isExecution;
                   });
}

void SemanticsValidator::collectOnErrorSnapshotEntriesForStableRange(
    std::size_t stableOrderOffset,
    std::size_t stableOrderCount,
    std::vector<OnErrorSnapshotEntry> &out) const {
  const std::size_t declarationCount =
      validationPlan_->definitionPrepass.declarationsInStableOrder.size();
  if (stableOrderCount == 0 || stableOrderOffset >= declarationCount) {
    return;
  }

  const std::size_t boundedCount =
      std::min(stableOrderCount, declarationCount - stableOrderOffset);
  out.reserve(out.size() + boundedCount);
  for (std::size_t stableOrdinal = 0; stableOrdinal < boundedCount;
       ++stableOrdinal) {
    const std::size_t stableIndex =
        validationPlan_->definitionPrepass
            .declarationsInStableOrder[stableOrderOffset + stableOrdinal]
            .stableIndex;
    const Definition &def = program_.definitions[stableIndex];
    const auto state = buildDefinitionValidationState(def);
    const auto &context = state.context;
    if (!context.onError.has_value()) {
      continue;
    }

    ReturnKind returnKind = ReturnKind::Unknown;
    if (const auto returnKindIt = returnKinds_.find(def.fullPath);
        returnKindIt != returnKinds_.end()) {
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

    out.push_back(OnErrorSnapshotEntry{
        def.fullPath,
        returnKind,
        context.onError->handlerPath,
        context.onError->errorType,
        context.onError->boundArgs.size(),
        std::move(boundArgTexts),
        context.resultType.has_value() && context.resultType->isResult &&
            context.resultType->hasValue,
        context.resultType.has_value() && context.resultType->isResult
            ? context.resultType->valueType
            : std::string{},
        context.resultType.has_value() && context.resultType->isResult
            ? context.resultType->errorType
            : std::string{},
        def.semanticNodeId,
    });
  }
}

std::vector<CollectedDirectCallTargetEntry>
SemanticsValidator::takeCollectedDirectCallTargetsForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedDirectCallTargets_, {});
}

std::vector<CollectedMethodCallTargetEntry>
SemanticsValidator::takeCollectedMethodCallTargetsForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedMethodCallTargets_, {});
}

std::vector<CollectedBridgePathChoiceEntry>
SemanticsValidator::takeCollectedBridgePathChoicesForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedBridgePathChoices_, {});
}

std::vector<CollectedCallableSummaryEntry>
SemanticsValidator::takeCollectedCallableSummariesForSemanticProduct() {
  collectPilotRoutingSemanticProductFacts();
  return std::exchange(collectedCallableSummaries_, {});
}

SemanticPublicationSurface
SemanticsValidator::takeSemanticPublicationSurfaceForSemanticProduct(
    const SemanticProductBuildConfig *buildConfig) {
  const CallSnapshotMemoScope callSnapshotMemoScope(*this);
  SemanticPublicationSurface surface;
  if (mergedWorkerPublicationFactsValid_) {
    rebindMergedWorkerPublicationFactSemanticNodeIds();
  }

  const bool needsRoutingSurface =
      isSemanticCollectorEnabled(buildConfig, "direct_call_targets") ||
      isSemanticCollectorEnabled(buildConfig, "method_call_targets") ||
      isSemanticCollectorEnabled(buildConfig, "bridge_path_choices") ||
      isSemanticCollectorEnabled(buildConfig, "callable_summaries");
  if (needsRoutingSurface) {
    collectPilotRoutingSemanticProductFacts();
    if (isSemanticCollectorEnabled(buildConfig, "direct_call_targets")) {
      surface.directCallTargets = std::exchange(collectedDirectCallTargets_, {});
    }
    if (isSemanticCollectorEnabled(buildConfig, "method_call_targets")) {
      surface.methodCallTargets = std::exchange(collectedMethodCallTargets_, {});
    }
    if (isSemanticCollectorEnabled(buildConfig, "bridge_path_choices")) {
      surface.bridgePathChoices = std::exchange(collectedBridgePathChoices_, {});
    }
    if (isSemanticCollectorEnabled(buildConfig, "callable_summaries")) {
      surface.callableSummaries = std::exchange(collectedCallableSummaries_, {});
    }
    invalidatePilotRoutingSemanticCollectors();
  }

  const bool useMergedWorkerPublicationFacts = mergedWorkerPublicationFactsValid_;
  if (isSemanticCollectorEnabled(buildConfig, "type_metadata")) {
    surface.typeMetadata = useMergedWorkerPublicationFacts
                               ? mergedWorkerPublicationFacts_.typeMetadata
                               : typeMetadataSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "struct_field_metadata")) {
    surface.structFieldMetadata = useMergedWorkerPublicationFacts
                                      ? mergedWorkerPublicationFacts_.structFieldMetadata
                                      : structFieldMetadataSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "sum_type_metadata")) {
    surface.sumTypeMetadata = useMergedWorkerPublicationFacts
                                  ? mergedWorkerPublicationFacts_.sumTypeMetadata
                                  : sumTypeMetadataSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "sum_variant_metadata")) {
    surface.sumVariantMetadata = useMergedWorkerPublicationFacts
                                     ? mergedWorkerPublicationFacts_.sumVariantMetadata
                                     : sumVariantMetadataSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "binding_facts")) {
    surface.bindingFacts = useMergedWorkerPublicationFacts
                               ? mergedWorkerPublicationFacts_.bindingFacts
                               : bindingFactSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "array_extent_facts")) {
    surface.arrayExtentFacts = useMergedWorkerPublicationFacts
                                   ? mergedWorkerPublicationFacts_.arrayExtentFacts
                                   : arrayExtentFactSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "return_facts")) {
    surface.returnFacts = useMergedWorkerPublicationFacts
                              ? mergedWorkerPublicationFacts_.returnFacts
                              : returnFactSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "local_auto_facts")) {
    surface.localAutoFacts = useMergedWorkerPublicationFacts
                                 ? mergedWorkerPublicationFacts_.localAutoFacts
                                 : localAutoFactSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "query_facts")) {
    surface.queryFacts = useMergedWorkerPublicationFacts
                             ? mergedWorkerPublicationFacts_.queryFacts
                             : queryFactSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "try_facts")) {
    surface.tryFacts = useMergedWorkerPublicationFacts
                           ? mergedWorkerPublicationFacts_.tryFacts
                           : tryFactSnapshotForSemanticProduct();
  }
  if (isSemanticCollectorEnabled(buildConfig, "on_error_facts")) {
    surface.onErrorFacts = useMergedWorkerPublicationFacts
                               ? mergedWorkerPublicationFacts_.onErrorFacts
                               : onErrorFactSnapshotForSemanticProduct();
  }

  if (!surface.callableSummaries.empty() || !surface.onErrorFacts.empty()) {
    if (mergedWorkerPublicationSeedStringsValid_ &&
        isSemanticCollectorEnabled(buildConfig, "callable_summaries") &&
        isSemanticCollectorEnabled(buildConfig, "on_error_facts")) {
      surface.callTargetSeedStrings = mergedWorkerPublicationSeedStrings_;
    } else {
      SymbolInterner publicationStringInterner;
      appendSemanticPublicationStringOrigins(publicationStringInterner,
                                             program_,
                                             validationPlan_->definitionPrepass,
                                             surface.callableSummaries,
                                             surface.onErrorFacts);
      surface.callTargetSeedStrings =
          publicationStringInterner.snapshotForWorker(0).symbolsByLocalId;
    }
  }

  releaseTransientSnapshotCaches();
  return surface;
}

} // namespace primec::semantics

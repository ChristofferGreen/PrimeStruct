#include "SemanticsValidator.h"

#include "SemanticsWorkerSymbolMerge.h"
#include "primec/SemanticsDefinitionPartitioner.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <iterator>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#endif

namespace primec::semantics {

namespace {

uint64_t captureCurrentResidentBytes() {
#if defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(),
                MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&info),
                &count) == KERN_SUCCESS) {
    return static_cast<uint64_t>(info.resident_size);
  }
  return 0;
#elif defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t pages = 0;
  uint64_t residentPages = 0;
  if (!(statm >> pages >> residentPages)) {
    return 0;
  }
  const long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0) {
    return 0;
  }
  return residentPages * static_cast<uint64_t>(pageSize);
#else
  return 0;
#endif
}

void relieveAllocatorPressure() {
#if defined(__APPLE__)
  (void)malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__) && defined(__GLIBC__)
  (void)malloc_trim(0);
#endif
}

bool isStructDefinition(const Definition &candidate) {
  for (const auto &transform : candidate.transforms) {
    if (transform.name == "sum") {
      return false;
    }
    if (isStructTransformName(transform.name)) {
      return true;
    }
  }
  return false;
}

bool isSumDefinition(const Definition &candidate) {
  for (const auto &transform : candidate.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

} // namespace

bool SemanticsValidator::publishPassesDefinitionsDiagnostic(const Expr *expr) {
  if (expr != nullptr) {
    captureExprContext(*expr);
  } else if (currentDefinitionContext_ != nullptr) {
    captureDefinitionContext(*currentDefinitionContext_);
  }
  return publishCurrentStructuredDiagnosticNow();
}

bool SemanticsValidator::validateDefinitionsFromStableIndexResolver(
    std::size_t stableCount,
    const std::function<std::size_t(std::size_t)> &resolveStableIndex) {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = shouldCollectStructuredDiagnostics();
  const bool benchmarkPerDefinitionRss =
      std::getenv("PRIMEC_BENCHMARK_SEMANTIC_PER_DEFINITION_RSS") != nullptr;
  const bool forceAllocatorReliefEachDefinition =
      std::getenv("PRIMEC_BENCHMARK_SEMANTIC_FORCE_DEFINITION_RELIEF") != nullptr;
  const bool disableAllocatorRelief =
      std::getenv("PRIMEC_DISABLE_SEMANTIC_ALLOCATOR_RELIEF") != nullptr;
  uint64_t lastAllocatorReliefRss = 0;
  auto maybeRelieveAllocatorPressure = [&](uint64_t rssNow, bool force) {
    if (disableAllocatorRelief || rssNow == 0) {
      return;
    }
    constexpr uint64_t kReliefThresholdBytes = 256ull * 1024ull * 1024ull;
    constexpr uint64_t kReliefDeltaBytes = 24ull * 1024ull * 1024ull;
    if (!force && rssNow < kReliefThresholdBytes) {
      return;
    }
    if (!force && lastAllocatorReliefRss != 0 &&
        rssNow < lastAllocatorReliefRss + kReliefDeltaBytes) {
      return;
    }
    relieveAllocatorPressure();
    lastAllocatorReliefRss = captureCurrentResidentBytes();
  };
  auto resetDefinitionInferenceCaches = [&]() {
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
  struct DefinitionRssRecord {
    std::string fullPath;
    uint64_t rssBefore = 0;
    uint64_t rssAfter = 0;
    int64_t rssDelta = 0;
    uint64_t wallNanos = 0;
    bool ok = false;
  };
  std::vector<DefinitionRssRecord> definitionRssRecords;
  if (benchmarkPerDefinitionRss) {
    definitionRssRecords.reserve(stableCount);
  }
  auto buildDefinitionValidationSliceContext =
      [&](const Definition &def, DefinitionValidationSliceContext &out) -> bool {
    out = {};
    out.definition = &def;
    out.params = &paramsByDef_[def.fullPath];
    if (!makeDefinitionValidationState(def, out.validationState)) {
      return false;
    }
    for (const auto &param : *out.params) {
      out.locals.emplace(param.name, param.binding);
    }
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt != returnKinds_.end()) {
      out.returnKind = kindIt->second;
    }
    return true;
  };
  auto validateDefinition = [&](const Definition &def) -> bool {
    auto failPassesDefinitionsDiagnostic =
        [&](const Expr *expr, std::string message) -> bool {
      if (expr != nullptr) {
        return failExprDiagnostic(*expr, std::move(message));
      }
      if (currentDefinitionContext_ != nullptr) {
        return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));
      }
      return failUncontextualizedDiagnostic(std::move(message));
    };
    DefinitionValidationSliceContext definitionContext;
    if (!buildDefinitionValidationSliceContext(def, definitionContext)) {
      if (error_.empty()) {
        return failPassesDefinitionsDiagnostic(
            nullptr,
            "definition validation context setup failed on " + def.fullPath);
      }
      return false;
    }
    WorkerLocalDefinitionValidationScope definitionScope(*this, definitionContext);
    if (!validateCapabilitiesSubset(def.transforms, def.fullPath)) {
      if (error_.empty()) {
        return failPassesDefinitionsDiagnostic(
            nullptr,
            "validateCapabilitiesSubset failed on " + def.fullPath);
      }
      return false;
    }
    if (isSumDefinition(def)) {
      return true;
    }
    const auto &defParams = *definitionContext.params;
    observeLocalMapSize(definitionContext.locals.size());
    for (const auto &param : defParams) {
      if (param.defaultExpr == nullptr) {
        continue;
      }
      if (!validateExpr(defParams, definitionContext.locals, *param.defaultExpr)) {
        if (error_.empty()) {
          return failPassesDefinitionsDiagnostic(
              param.defaultExpr,
              "default expression validation failed on " + def.fullPath);
        }
        return false;
      }
    }
    if (isLifecycleHelperName(def.fullPath) &&
        definitionContext.returnKind != ReturnKind::Void) {
      return failPassesDefinitionsDiagnostic(
          nullptr,
          "lifecycle helpers must return void: " + def.fullPath);
    }
    const std::optional<OnErrorHandler> &onErrorHandler =
        currentValidationState_.context.onError;
    if (onErrorHandler.has_value() &&
        (!currentValidationState_.context.resultType.has_value() ||
         !currentValidationState_.context.resultType->isResult) &&
        definitionContext.returnKind != ReturnKind::Int) {
      return failPassesDefinitionsDiagnostic(
          nullptr,
          "on_error requires Result or int return type on " + def.fullPath);
    }
    if (onErrorHandler.has_value() &&
        currentValidationState_.context.resultType.has_value() &&
        currentValidationState_.context.resultType->isResult &&
        !errorTypesMatch(onErrorHandler->errorType,
                         currentValidationState_.context.resultType->errorType,
                         def.namespacePrefix)) {
      return failPassesDefinitionsDiagnostic(
          nullptr,
          "on_error error type mismatch on " + def.fullPath);
    }
    if (onErrorHandler.has_value()) {
      OnErrorScope onErrorScope(*this, std::nullopt);
      for (const auto &arg : currentValidationState_.context.onError->boundArgs) {
        if (!validateExpr(defParams, definitionContext.locals, arg)) {
          if (error_.empty()) {
            return failPassesDefinitionsDiagnostic(
                &arg,
                "on_error bound-arg validation failed on " + def.fullPath);
          }
          return false;
        }
      }
    }
    OnErrorScope onErrorScope(*this, onErrorHandler);
    for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
      const Expr &stmt = def.statements[stmtIndex];
      if (!validateStatement(defParams,
                             definitionContext.locals,
                             stmt,
                             definitionContext.returnKind,
                             true,
                             true,
                             &definitionContext.sawReturn,
                             def.namespacePrefix,
                             &def.statements,
                             stmtIndex)) {
        if (error_.empty()) {
          return failPassesDefinitionsDiagnostic(
              &stmt,
              "statement validation failed on " + def.fullPath);
        }
        return false;
      }
      observeLocalMapSize(definitionContext.locals.size());
      expireReferenceBorrowsForRemainder(
          defParams,
          definitionContext.locals,
          def.statements,
          stmtIndex + 1);
    }
    if (def.returnExpr.has_value()) {
      if (!validateExpr(defParams, definitionContext.locals, *def.returnExpr)) {
        if (error_.empty()) {
          return failPassesDefinitionsDiagnostic(
              &*def.returnExpr,
              "return expression validation failed on " + def.fullPath);
        }
        return false;
      }
      definitionContext.sawReturn = true;
    }
    if (definitionContext.returnKind != ReturnKind::Void &&
        !isStructDefinition(def)) {
      bool allPathsReturn = def.returnExpr.has_value() || blockAlwaysReturns(def.statements);
      if (!allPathsReturn) {
        return failPassesDefinitionsDiagnostic(
            nullptr,
            definitionContext.sawReturn
                ? "not all control paths return in " + def.fullPath +
                      " (missing return statement)"
                : "missing return statement in " + def.fullPath);
      }
    }
    bool shouldCheckUninitialized =
        (definitionContext.returnKind == ReturnKind::Void);
    if (definitionContext.returnKind != ReturnKind::Void &&
        !isStructDefinition(def)) {
      shouldCheckUninitialized = def.returnExpr.has_value() || blockAlwaysReturns(def.statements);
    }
    if (shouldCheckUninitialized) {
      std::optional<std::string> uninitError =
          validateUninitializedDefiniteState(defParams, def.statements);
      if (uninitError.has_value()) {
        return failPassesDefinitionsDiagnostic(nullptr,
                                              *std::move(uninitError));
      }
    }
    return true;
  };

  for (std::size_t stableOrdinal = 0; stableOrdinal < stableCount;
       ++stableOrdinal) {
    const std::size_t stableIndex = resolveStableIndex(stableOrdinal);
    const Definition &def = program_.definitions[stableIndex];
    const uint64_t rssBefore = benchmarkPerDefinitionRss ? captureCurrentResidentBytes() : 0;
    const auto wallStart = benchmarkPerDefinitionRss ? std::chrono::steady_clock::now()
                                                     : std::chrono::steady_clock::time_point{};
    clearStructuredDiagnosticContext();
    if (collectDiagnostics) {
      ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));
      std::vector<SemanticDiagnosticRecord> intraDefinitionRecords;
      collectDefinitionIntraBodyCallDiagnostics(def, intraDefinitionRecords);
      if (!intraDefinitionRecords.empty()) {
        rememberFirstCollectedDiagnosticMessage(intraDefinitionRecords.front().message);
        collectedRecords.insert(collectedRecords.end(),
                                std::make_move_iterator(intraDefinitionRecords.begin()),
                                std::make_move_iterator(intraDefinitionRecords.end()));
        if (benchmarkPerDefinitionRss) {
          const uint64_t rssAfter = captureCurrentResidentBytes();
          const auto wallEnd = std::chrono::steady_clock::now();
          definitionRssRecords.push_back(DefinitionRssRecord{
              def.fullPath,
              rssBefore,
              rssAfter,
              static_cast<int64_t>(rssAfter) - static_cast<int64_t>(rssBefore),
              static_cast<uint64_t>(
                  std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count()),
              true,
          });
        }
        resetDefinitionInferenceCaches();
        continue;
      }
    }
    const bool ok = validateDefinition(def);
    if (!ok) {
      if (!collectDiagnostics) {
        if (benchmarkPerDefinitionRss) {
          const uint64_t rssAfter = captureCurrentResidentBytes();
          const auto wallEnd = std::chrono::steady_clock::now();
          definitionRssRecords.push_back(DefinitionRssRecord{
              def.fullPath,
              rssBefore,
              rssAfter,
              static_cast<int64_t>(rssAfter) - static_cast<int64_t>(rssBefore),
              static_cast<uint64_t>(
                  std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count()),
              false,
          });
        }
        resetDefinitionInferenceCaches();
        return false;
      }
      moveCurrentStructuredDiagnosticTo(collectedRecords);
    }
    if (benchmarkPerDefinitionRss) {
      const uint64_t rssAfter = captureCurrentResidentBytes();
      const auto wallEnd = std::chrono::steady_clock::now();
      definitionRssRecords.push_back(DefinitionRssRecord{
          def.fullPath,
          rssBefore,
          rssAfter,
          static_cast<int64_t>(rssAfter) - static_cast<int64_t>(rssBefore),
          static_cast<uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count()),
          ok,
      });
      maybeRelieveAllocatorPressure(rssAfter, forceAllocatorReliefEachDefinition);
    } else if ((stableOrdinal & 0x7u) == 0x7u) {
      maybeRelieveAllocatorPressure(
          captureCurrentResidentBytes(),
          forceAllocatorReliefEachDefinition);
    }
    resetDefinitionInferenceCaches();
  }

  if (benchmarkPerDefinitionRss) {
    std::vector<DefinitionRssRecord> sortedRecords = definitionRssRecords;
    std::sort(sortedRecords.begin(),
              sortedRecords.end(),
              [](const DefinitionRssRecord &left, const DefinitionRssRecord &right) {
                if (left.rssDelta != right.rssDelta) {
                  return left.rssDelta > right.rssDelta;
                }
                return left.wallNanos > right.wallNanos;
              });
    const size_t limit = std::min<size_t>(10, sortedRecords.size());
    std::cerr << "[benchmark-semantic-definition-rss-top] {\"count\":"
              << definitionRssRecords.size() << ",\"top\":[";
    for (size_t i = 0; i < limit; ++i) {
      if (i > 0) {
        std::cerr << ",";
      }
      const DefinitionRssRecord &record = sortedRecords[i];
      std::cerr << "{\"path\":\"" << record.fullPath
                << "\",\"rss_before\":" << record.rssBefore
                << ",\"rss_after\":" << record.rssAfter
                << ",\"rss_delta\":" << record.rssDelta
                << ",\"wall_nanos\":" << record.wallNanos
                << ",\"ok\":" << (record.ok ? "true" : "false")
                << "}";
    }
    std::cerr << "]}" << std::endl;
  }

  if (!finalizeCollectedStructuredDiagnostics(collectedRecords)) {
    return false;
  }

  return true;
}

bool SemanticsValidator::validateDefinitionsForStableIndices(
    const std::vector<std::size_t> &stableIndices) {
  return validateDefinitionsFromStableIndexResolver(
      stableIndices.size(),
      [&](std::size_t stableOrdinal) { return stableIndices[stableOrdinal]; });
}

bool SemanticsValidator::validateDefinitionsForStableRange(
    std::size_t stableOrderOffset,
    std::size_t stableOrderCount) {
  const std::size_t declarationCount =
      validationPlan_->definitionPrepass.declarationsInStableOrder.size();
  if (stableOrderCount == 0 || stableOrderOffset >= declarationCount) {
    return true;
  }

  const std::size_t boundedCount =
      std::min(stableOrderCount, declarationCount - stableOrderOffset);
  return validateDefinitionsFromStableIndexResolver(
      boundedCount, [&](std::size_t stableOrdinal) {
        return validationPlan_->definitionPrepass
            .declarationsInStableOrder[stableOrderOffset + stableOrdinal]
            .stableIndex;
      });
}

bool SemanticsValidator::runDefinitionValidationWorkerChunk(
    std::size_t stableOrderOffset,
    std::size_t stableOrderCount) {
  auto runStage = [&](const char *stageName, auto &&fn) {
    try {
      const bool ok = fn();
      if (!ok && error_.empty()) {
        return failUncontextualizedDiagnostic(std::string(stageName) +
                                              " failed without diagnostic");
      }
      return ok;
    } catch (...) {
      return failUncontextualizedDiagnostic(
          std::string("semantic validator exception in ") + stageName);
    }
  };
  if (!runStage("buildDefinitionMaps", [&] { return buildDefinitionMaps(); })) {
    return false;
  }
  if (!runStage("inferUnknownReturnKinds",
                [&] { return inferUnknownReturnKinds(); })) {
    return false;
  }
  if (!runStage("validateTraitConstraints",
                [&] { return validateTraitConstraints(); })) {
    return false;
  }
  if (!runStage("validateStructLayouts",
                [&] { return validateStructLayouts(); })) {
    return false;
  }
  return runStage("validateDefinitionsChunk", [&] {
    return validateDefinitionsForStableRange(stableOrderOffset,
                                             stableOrderCount);
  });
}

bool SemanticsValidator::validateDefinitions() {
  const std::size_t declarationCount =
      validationPlan_->definitionPrepass.declarationsInStableOrder.size();
  if (benchmarkSemanticDefinitionValidationWorkerCount_ <= 1 ||
      declarationCount < 2) {
    return validateDefinitionsForStableRange(0, declarationCount);
  }

  const std::size_t partitionCount = static_cast<std::size_t>(
      benchmarkSemanticDefinitionValidationWorkerCount_);
  const bool collectDiagnostics = shouldCollectStructuredDiagnostics();
  std::vector<DefinitionPartitionChunk> partitions =
      partitionDefinitionsDeterministic(validationPlan_->definitionPrepass, partitionCount);
  if (partitions.size() <= 1) {
    return validateDefinitionsForStableRange(0, declarationCount);
  }

  std::vector<std::future<SemanticDefinitionWorkerResultBundle>> workerTasks;
  workerTasks.reserve(partitions.size());
  for (DefinitionPartitionChunk &chunk : partitions) {
    if (chunk.stableOrderCount == 0) {
      continue;
    }
    workerTasks.push_back(std::async(
        std::launch::async,
        [this,
         collectDiagnostics,
         partitionKey = chunk.partitionKey,
         stableOrderOffset = chunk.stableOrderOffset,
         stableOrderCount = chunk.stableOrderCount]() {
          SemanticDefinitionWorkerResultBundle result;
          result.partitionKey = partitionKey;
          SemanticsValidator worker(program_,
                                    entryPath_,
                                    result.error,
                                    defaultEffects_,
                                    entryDefaultEffects_,
                                    collectDiagnostics ? &result.diagnostics : nullptr,
                                    collectDiagnostics,
                                    1,
                                    benchmarkSemanticPhaseCountersEnabled_,
                                    false,
                                    false,
                                    false,
                                    !benchmarkGraphLocalAutoDependencyScratchPmrEnabled_,
                                    validationPlan_);
          result.ok = worker.runDefinitionValidationWorkerChunk(
              stableOrderOffset,
              stableOrderCount);
          result.counters = worker.validationCounters();
          if (result.ok) {
            worker.collectDefinitionPublicationFactsForStableRange(
                stableOrderOffset, stableOrderCount, result.publicationFacts);
            SymbolInterner publicationStringInterner;
            appendSemanticPublicationStringOrigins(publicationStringInterner,
                                                  program_,
                                                  validationPlan_->definitionPrepass,
                                                  result.publicationFacts.callableSummaries,
                                                  result.publicationFacts.onErrorFacts);
            result.publicationStringSnapshot =
                publicationStringInterner.snapshotForWorker(partitionKey);
          }
          return result;
        }));
  }
  if (workerTasks.size() <= 1) {
    return validateDefinitionsForStableRange(0, declarationCount);
  }

  std::vector<SemanticDefinitionWorkerResultBundle> workerResults;
  workerResults.reserve(workerTasks.size());
  for (auto &task : workerTasks) {
    workerResults.push_back(task.get());
  }
  std::sort(workerResults.begin(),
            workerResults.end(),
            [](const SemanticDefinitionWorkerResultBundle &left,
               const SemanticDefinitionWorkerResultBundle &right) {
              return left.partitionKey < right.partitionKey;
            });
  if (benchmarkSemanticPhaseCountersEnabled_) {
    for (const SemanticDefinitionWorkerResultBundle &result : workerResults) {
      validationCounters_.callsVisited += result.counters.callsVisited;
      validationCounters_.peakLocalMapSize =
          std::max(validationCounters_.peakLocalMapSize,
                   result.counters.peakLocalMapSize);
    }
  }

  auto mergeWorkerPublicationFacts = [&]() {
    mergedWorkerPublicationFactsValid_ = false;
    mergedWorkerPublicationFactSemanticNodeIdsCurrent_ = false;
    mergedWorkerPublicationFacts_ = {};

    auto appendMoved = [](auto &target, auto &source) {
      target.insert(target.end(),
                    std::make_move_iterator(source.begin()),
                    std::make_move_iterator(source.end()));
    };
    for (SemanticDefinitionWorkerResultBundle &result : workerResults) {
      appendMoved(mergedWorkerPublicationFacts_.directCallTargets,
                  result.publicationFacts.directCallTargets);
      appendMoved(mergedWorkerPublicationFacts_.methodCallTargets,
                  result.publicationFacts.methodCallTargets);
      appendMoved(mergedWorkerPublicationFacts_.bridgePathChoices,
                  result.publicationFacts.bridgePathChoices);
      appendMoved(mergedWorkerPublicationFacts_.callableSummaries,
                  result.publicationFacts.callableSummaries);
      appendMoved(mergedWorkerPublicationFacts_.typeMetadata,
                  result.publicationFacts.typeMetadata);
      appendMoved(mergedWorkerPublicationFacts_.structFieldMetadata,
                  result.publicationFacts.structFieldMetadata);
      appendMoved(mergedWorkerPublicationFacts_.sumTypeMetadata,
                  result.publicationFacts.sumTypeMetadata);
      appendMoved(mergedWorkerPublicationFacts_.sumVariantMetadata,
                  result.publicationFacts.sumVariantMetadata);
      appendMoved(mergedWorkerPublicationFacts_.bindingFacts,
                  result.publicationFacts.bindingFacts);
      appendMoved(mergedWorkerPublicationFacts_.returnFacts,
                  result.publicationFacts.returnFacts);
      appendMoved(mergedWorkerPublicationFacts_.localAutoFacts,
                  result.publicationFacts.localAutoFacts);
      appendMoved(mergedWorkerPublicationFacts_.queryFacts,
                  result.publicationFacts.queryFacts);
      appendMoved(mergedWorkerPublicationFacts_.tryFacts,
                  result.publicationFacts.tryFacts);
      appendMoved(mergedWorkerPublicationFacts_.onErrorFacts,
                  result.publicationFacts.onErrorFacts);
    }

    collectExecutionCallableSummaryEntries(
        mergedWorkerPublicationFacts_.callableSummaries);
    rebindCollectedCallableSummarySemanticNodeIds(
        mergedWorkerPublicationFacts_.callableSummaries);
    sortCollectedCallableSummaries(mergedWorkerPublicationFacts_.callableSummaries);
    rebindCollectedOnErrorSemanticNodeIds(mergedWorkerPublicationFacts_.onErrorFacts);
    sortCollectedOnErrorSnapshots(mergedWorkerPublicationFacts_.onErrorFacts);

    std::stable_sort(mergedWorkerPublicationFacts_.directCallTargets.begin(),
                     mergedWorkerPublicationFacts_.directCallTargets.end(),
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
    std::stable_sort(mergedWorkerPublicationFacts_.methodCallTargets.begin(),
                     mergedWorkerPublicationFacts_.methodCallTargets.end(),
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
    std::stable_sort(mergedWorkerPublicationFacts_.bridgePathChoices.begin(),
                     mergedWorkerPublicationFacts_.bridgePathChoices.end(),
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
    std::stable_sort(mergedWorkerPublicationFacts_.typeMetadata.begin(),
                     mergedWorkerPublicationFacts_.typeMetadata.end(),
                     [](const auto &left, const auto &right) {
                       if (left.fullPath != right.fullPath) {
                         return left.fullPath < right.fullPath;
                       }
                       return left.category < right.category;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.structFieldMetadata.begin(),
                     mergedWorkerPublicationFacts_.structFieldMetadata.end(),
                     [](const auto &left, const auto &right) {
                       if (left.structPath != right.structPath) {
                         return left.structPath < right.structPath;
                       }
                       if (left.fieldIndex != right.fieldIndex) {
                         return left.fieldIndex < right.fieldIndex;
                       }
                       return left.fieldName < right.fieldName;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.sumTypeMetadata.begin(),
                     mergedWorkerPublicationFacts_.sumTypeMetadata.end(),
                     [](const auto &left, const auto &right) {
                       return left.fullPath < right.fullPath;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.sumVariantMetadata.begin(),
                     mergedWorkerPublicationFacts_.sumVariantMetadata.end(),
                     [](const auto &left, const auto &right) {
                       if (left.sumPath != right.sumPath) {
                         return left.sumPath < right.sumPath;
                       }
                       if (left.variantIndex != right.variantIndex) {
                         return left.variantIndex < right.variantIndex;
                       }
                       return left.variantName < right.variantName;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.bindingFacts.begin(),
                     mergedWorkerPublicationFacts_.bindingFacts.end(),
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
                       if (left.siteKind != right.siteKind) {
                         return left.siteKind < right.siteKind;
                       }
                       if (left.name != right.name) {
                         return left.name < right.name;
                       }
                       return left.resolvedPath < right.resolvedPath;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.returnFacts.begin(),
                     mergedWorkerPublicationFacts_.returnFacts.end(),
                     [](const auto &left, const auto &right) {
                       return left.definitionPath < right.definitionPath;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.localAutoFacts.begin(),
                     mergedWorkerPublicationFacts_.localAutoFacts.end(),
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
                       return left.bindingName < right.bindingName;
                     });
    std::stable_sort(mergedWorkerPublicationFacts_.queryFacts.begin(),
                     mergedWorkerPublicationFacts_.queryFacts.end(),
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
    std::stable_sort(mergedWorkerPublicationFacts_.tryFacts.begin(),
                     mergedWorkerPublicationFacts_.tryFacts.end(),
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
    mergedWorkerPublicationFactsValid_ = true;
    mergedWorkerPublicationFactSemanticNodeIdsCurrent_ = false;
  };

  auto mergeWorkerPublicationSeedStrings = [&]() {
    mergedWorkerPublicationSeedStringsValid_ = false;
    std::vector<std::string>().swap(mergedWorkerPublicationSeedStrings_);

    std::vector<WorkerSymbolInternerSnapshot> snapshots;
    snapshots.reserve(workerResults.size());
    for (SemanticDefinitionWorkerResultBundle &result : workerResults) {
      if (result.publicationStringSnapshot.symbolsByLocalId.empty()) {
        continue;
      }
      snapshots.push_back(std::move(result.publicationStringSnapshot));
    }

    SymbolInterner mergedPublicationStrings;
    if (!snapshots.empty()) {
      mergedPublicationStrings =
          SymbolInterner::mergeWorkerSnapshotsDeterministic(std::move(snapshots));
    }
    appendSemanticPublicationStringOrigins(mergedPublicationStrings,
                                           program_,
                                           validationPlan_->definitionPrepass,
                                           mergedWorkerPublicationFacts_.callableSummaries,
                                           mergedWorkerPublicationFacts_.onErrorFacts);
    mergedWorkerPublicationSeedStrings_ =
        mergedPublicationStrings.snapshotForWorker(0).symbolsByLocalId;
    mergedWorkerPublicationSeedStringsValid_ = true;
  };

  if (!collectDiagnostics) {
    for (const SemanticDefinitionWorkerResultBundle &result : workerResults) {
      if (result.ok) {
        continue;
      }
      if (!result.error.empty()) {
        error_ = result.error;
      }
      if (error_.empty()) {
        error_ = "parallel definition validation failed without diagnostic";
      }
      return false;
    }
    mergeWorkerPublicationFacts();
    mergeWorkerPublicationSeedStrings();
    return true;
  }

  std::vector<SemanticDiagnosticRecord> collectedRecords;
  for (SemanticDefinitionWorkerResultBundle &result : workerResults) {
    if (!result.diagnostics.records.empty()) {
      collectedRecords.insert(
          collectedRecords.end(),
          std::make_move_iterator(result.diagnostics.records.begin()),
          std::make_move_iterator(result.diagnostics.records.end()));
      continue;
    }
    if (!result.ok && !result.error.empty()) {
      SemanticDiagnosticRecord record;
      record.message = result.error;
      collectedRecords.push_back(std::move(record));
    }
  }
  if (!finalizeCollectedStructuredDiagnostics(collectedRecords)) {
    return false;
  }

  for (const SemanticDefinitionWorkerResultBundle &result : workerResults) {
    if (result.ok) {
      continue;
    }
    if (!result.error.empty()) {
      error_ = result.error;
    }
    if (error_.empty()) {
      error_ = "parallel definition validation failed without diagnostic";
    }
    return false;
  }

  mergeWorkerPublicationFacts();
  mergeWorkerPublicationSeedStrings();
  return true;
}

}  // namespace primec::semantics

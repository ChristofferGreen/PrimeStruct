#include "SemanticsValidator.h"

#include "primec/SemanticsDefinitionPartitioner.h"

#include <algorithm>
#include <future>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::publishPassesDefinitionsDiagnostic(const Expr *expr) {
  if (expr != nullptr) {
    captureExprContext(*expr);
  } else if (currentDefinitionContext_ != nullptr) {
    captureDefinitionContext(*currentDefinitionContext_);
  }
  return publishCurrentStructuredDiagnosticNow();
}

bool SemanticsValidator::validateDefinitionsForStableIndices(
    const std::vector<std::size_t> &stableIndices) {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = shouldCollectStructuredDiagnostics();
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
    DefinitionContextScope definitionScope(*this, def);
    ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));
    auto isStructDefinition = [&](const Definition &candidate) {
      for (const auto &transform : candidate.transforms) {
        if (isStructTransformName(transform.name)) {
          return true;
        }
      }
      return false;
    };
    if (!validateCapabilitiesSubset(def.transforms, def.fullPath)) {
        if (error_.empty()) {
          return failPassesDefinitionsDiagnostic(
              nullptr,
              "validateCapabilitiesSubset failed on " + def.fullPath);
        }
        return false;
      }
    std::unordered_map<std::string, BindingInfo> locals;
    const auto &defParams = paramsByDef_[def.fullPath];
    for (const auto &param : defParams) {
      locals.emplace(param.name, param.binding);
    }
    observeLocalMapSize(locals.size());
    for (const auto &param : defParams) {
      if (param.defaultExpr == nullptr) {
        continue;
      }
      if (!validateExpr(defParams, locals, *param.defaultExpr)) {
        if (error_.empty()) {
          return failPassesDefinitionsDiagnostic(
              param.defaultExpr,
              "default expression validation failed on " + def.fullPath);
        }
        return false;
      }
    }
    ReturnKind kind = ReturnKind::Unknown;
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt != returnKinds_.end()) {
      kind = kindIt->second;
    }
    if (isLifecycleHelperName(def.fullPath) && kind != ReturnKind::Void) {
      return failPassesDefinitionsDiagnostic(
          nullptr,
          "lifecycle helpers must return void: " + def.fullPath);
    }
    const std::optional<OnErrorHandler> &onErrorHandler = currentValidationState_.context.onError;
    if (onErrorHandler.has_value() &&
        (!currentValidationState_.context.resultType.has_value() ||
         !currentValidationState_.context.resultType->isResult) &&
        kind != ReturnKind::Int) {
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
        if (!validateExpr(defParams, locals, arg)) {
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
    bool sawReturn = false;
    for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
      const Expr &stmt = def.statements[stmtIndex];
      if (!validateStatement(defParams,
                             locals,
                             stmt,
                             kind,
                             true,
                             true,
                             &sawReturn,
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
      observeLocalMapSize(locals.size());
      expireReferenceBorrowsForRemainder(defParams, locals, def.statements, stmtIndex + 1);
    }
    if (def.returnExpr.has_value()) {
      if (!validateExpr(defParams, locals, *def.returnExpr)) {
        if (error_.empty()) {
          return failPassesDefinitionsDiagnostic(
              &*def.returnExpr,
              "return expression validation failed on " + def.fullPath);
        }
        return false;
      }
      sawReturn = true;
    }
    if (kind != ReturnKind::Void && !isStructDefinition(def)) {
      bool allPathsReturn = def.returnExpr.has_value() || blockAlwaysReturns(def.statements);
      if (!allPathsReturn) {
        return failPassesDefinitionsDiagnostic(
            nullptr,
            sawReturn
                ? "not all control paths return in " + def.fullPath +
                      " (missing return statement)"
                : "missing return statement in " + def.fullPath);
      }
    }
    bool shouldCheckUninitialized = (kind == ReturnKind::Void);
    if (kind != ReturnKind::Void && !isStructDefinition(def)) {
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

  for (const std::size_t stableIndex : stableIndices) {
    const Definition &def = program_.definitions[stableIndex];
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
        continue;
      }
    }
    if (!validateDefinition(def)) {
      if (!collectDiagnostics) {
        return false;
      }
      moveCurrentStructuredDiagnosticTo(collectedRecords);
    }
  }

  if (!finalizeCollectedStructuredDiagnostics(collectedRecords)) {
    return false;
  }

  return true;
}

bool SemanticsValidator::validateDefinitionsForStableRange(
    std::size_t stableOrderOffset,
    std::size_t stableOrderCount) {
  const std::size_t declarationCount =
      definitionPrepassSnapshot_.declarationsInStableOrder.size();
  if (stableOrderCount == 0 || stableOrderOffset >= declarationCount) {
    return true;
  }

  const std::size_t boundedCount =
      std::min(stableOrderCount, declarationCount - stableOrderOffset);
  std::vector<std::size_t> stableIndices;
  stableIndices.reserve(boundedCount);
  for (std::size_t i = 0; i < boundedCount; ++i) {
    stableIndices.push_back(
        definitionPrepassSnapshot_.declarationsInStableOrder[stableOrderOffset + i]
            .stableIndex);
  }
  return validateDefinitionsForStableIndices(stableIndices);
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
  auto buildAllStableIndices = [&]() {
    std::vector<std::size_t> allStableIndices;
    allStableIndices.reserve(definitionPrepassSnapshot_.declarationsInStableOrder.size());
    for (const auto &declaration : definitionPrepassSnapshot_.declarationsInStableOrder) {
      allStableIndices.push_back(declaration.stableIndex);
    }
    return allStableIndices;
  };
  const std::size_t declarationCount =
      definitionPrepassSnapshot_.declarationsInStableOrder.size();
  if (benchmarkSemanticDefinitionValidationWorkerCount_ <= 1 ||
      declarationCount < 2) {
    return validateDefinitionsForStableIndices(buildAllStableIndices());
  }

  const std::size_t partitionCount = static_cast<std::size_t>(
      benchmarkSemanticDefinitionValidationWorkerCount_);
  const bool collectDiagnostics = shouldCollectStructuredDiagnostics();
  std::vector<DefinitionPartitionChunk> partitions =
      partitionDefinitionsDeterministic(definitionPrepassSnapshot_, partitionCount);
  if (partitions.size() <= 1) {
    return validateDefinitionsForStableIndices(buildAllStableIndices());
  }

  struct WorkerChunkResult {
    uint32_t partitionKey = 0;
    bool ok = false;
    std::string error;
    SemanticDiagnosticInfo diagnostics;
    ValidationCounters counters;
  };

  std::vector<std::future<WorkerChunkResult>> workerTasks;
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
          WorkerChunkResult result;
          result.partitionKey = partitionKey;
          SemanticsValidator worker(program_,
                                    entryPath_,
                                    result.error,
                                    defaultEffects_,
                                    entryDefaultEffects_,
                                    collectDiagnostics ? &result.diagnostics : nullptr,
                                    collectDiagnostics,
                                    1,
                                    benchmarkSemanticPhaseCountersEnabled_);
          result.ok = worker.runDefinitionValidationWorkerChunk(
              stableOrderOffset,
              stableOrderCount);
          result.counters = worker.validationCounters();
          return result;
        }));
  }
  if (workerTasks.size() <= 1) {
    return validateDefinitionsForStableIndices(buildAllStableIndices());
  }

  std::vector<WorkerChunkResult> workerResults;
  workerResults.reserve(workerTasks.size());
  for (auto &task : workerTasks) {
    workerResults.push_back(task.get());
  }
  std::sort(workerResults.begin(),
            workerResults.end(),
            [](const WorkerChunkResult &left, const WorkerChunkResult &right) {
              return left.partitionKey < right.partitionKey;
            });
  if (benchmarkSemanticPhaseCountersEnabled_) {
    for (const WorkerChunkResult &result : workerResults) {
      validationCounters_.callsVisited += result.counters.callsVisited;
      validationCounters_.peakLocalMapSize =
          std::max(validationCounters_.peakLocalMapSize,
                   result.counters.peakLocalMapSize);
    }
  }

  if (!collectDiagnostics) {
    for (const WorkerChunkResult &result : workerResults) {
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
    return true;
  }

  std::vector<SemanticDiagnosticRecord> collectedRecords;
  for (WorkerChunkResult &result : workerResults) {
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

  for (const WorkerChunkResult &result : workerResults) {
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

  return true;
}

}  // namespace primec::semantics

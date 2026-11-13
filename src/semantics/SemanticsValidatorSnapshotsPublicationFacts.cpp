// Definition publication facts, worker-fact rebinding and pilot routing collection.
#include "SemanticsValidatorSnapshotHelpers.h"
#include "SemanticsValidatorPublicationFactTraits.h"

#include <string>

#include <unordered_set>

#include <unordered_map>

#include <optional>

namespace primec::semantics {
using namespace snapshot_detail;

void SemanticsValidator::collectDefinitionPublicationFactsForStableRange(
    std::size_t stableOrderOffset,
    std::size_t stableOrderCount,
    SemanticPublicationSurface &out) {
  const auto &declarations =
      validationPlan_->definitionPrepass.declarationsInStableOrder;
  const std::size_t declarationCount = declarations.size();
  if (stableOrderCount == 0 || stableOrderOffset >= declarationCount) {
    return;
  }

  const std::size_t boundedCount =
      std::min(stableOrderCount, declarationCount - stableOrderOffset);
  std::unordered_set<std::string> definitionPaths;
  definitionPaths.reserve(boundedCount);
  for (std::size_t stableOrdinal = 0; stableOrdinal < boundedCount;
       ++stableOrdinal) {
    const std::size_t stableIndex =
        declarations[stableOrderOffset + stableOrdinal].stableIndex;
    definitionPaths.insert(program_.definitions[stableIndex].fullPath);
  }

  const CallSnapshotMemoScope callSnapshotMemoScope(*this);
  skipLocalAwareCallRefinement_ = true;
  collectPilotRoutingSemanticProductFacts();
  skipLocalAwareCallRefinement_ = false;
  appendEntriesForDefinitionPaths(
      out.directCallTargets,
      std::move(collectedDirectCallTargets_),
      definitionPaths,
      [](const CollectedDirectCallTargetEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  appendEntriesForDefinitionPaths(
      out.methodCallTargets,
      std::move(collectedMethodCallTargets_),
      definitionPaths,
      [](const CollectedMethodCallTargetEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  appendEntriesForDefinitionPaths(
      out.bridgePathChoices,
      std::move(collectedBridgePathChoices_),
      definitionPaths,
      [](const CollectedBridgePathChoiceEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  invalidatePilotRoutingSemanticCollectors();

  collectCallableSummaryEntriesForStableRange(
      stableOrderOffset, stableOrderCount, out.callableSummaries);
  appendEntriesForDefinitionPaths(
      out.typeMetadata,
      typeMetadataSnapshotForSemanticProduct(),
      definitionPaths,
      [](const TypeMetadataSnapshotEntry &entry) -> const std::string & {
        return entry.fullPath;
      });
  appendEntriesForDefinitionPaths(
      out.structFieldMetadata,
      structFieldMetadataSnapshotForSemanticProduct(),
      definitionPaths,
      [](const StructFieldMetadataSnapshotEntry &entry) -> const std::string & {
        return entry.structPath;
      });
  appendEntriesForDefinitionPaths(
      out.sumTypeMetadata,
      sumTypeMetadataSnapshotForSemanticProduct(),
      definitionPaths,
      [](const SumTypeMetadataSnapshotEntry &entry) -> const std::string & {
        return entry.fullPath;
      });
  appendEntriesForDefinitionPaths(
      out.sumVariantMetadata,
      sumVariantMetadataSnapshotForSemanticProduct(),
      definitionPaths,
      [](const SumVariantMetadataSnapshotEntry &entry) -> const std::string & {
        return entry.sumPath;
      });
  appendEntriesForDefinitionPaths(
      out.bindingFacts,
      bindingFactSnapshotForSemanticProduct(),
      definitionPaths,
      [](const BindingFactSnapshotEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  appendEntriesForDefinitionPaths(
      out.arrayExtentFacts,
      arrayExtentFactSnapshotForSemanticProduct(),
      definitionPaths,
      [](const ArrayExtentFactSnapshotEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  appendEntriesForDefinitionPaths(
      out.returnFacts,
      returnFactSnapshotForSemanticProduct(),
      definitionPaths,
      [](const ReturnFactSnapshotEntry &entry) -> const std::string & {
        return entry.definitionPath;
      });
  appendEntriesForDefinitionPaths(
      out.localAutoFacts,
      localAutoFactSnapshotForSemanticProduct(),
      definitionPaths,
      [](const LocalAutoBindingSnapshotEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  appendEntriesForDefinitionPaths(
      out.queryFacts,
      queryFactSnapshotForSemanticProduct(),
      definitionPaths,
      [](const QueryFactSnapshotEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  appendEntriesForDefinitionPaths(
      out.tryFacts,
      tryFactSnapshotForSemanticProduct(),
      definitionPaths,
      [](const TryValueSnapshotEntry &entry) -> const std::string & {
        return entry.scopePath;
      });
  collectOnErrorSnapshotEntriesForStableRange(
      stableOrderOffset, stableOrderCount, out.onErrorFacts);
}

void SemanticsValidator::rebindMergedWorkerPublicationFactSemanticNodeIds() {
  if (!mergedWorkerPublicationFactsValid_ ||
      mergedWorkerPublicationFactSemanticNodeIdsCurrent_) {
    return;
  }

  rebindCollectedCallableSummarySemanticNodeIds(
      mergedWorkerPublicationFacts_.callableSummaries);
  rebindCollectedOnErrorSemanticNodeIds(mergedWorkerPublicationFacts_.onErrorFacts);

  mergedWorkerPublicationFactsValid_ = false;
  invalidatePilotRoutingSemanticCollectors();
  collectPilotRoutingSemanticProductFacts();
  auto freshDirectCallTargets = std::exchange(collectedDirectCallTargets_, {});
  auto freshMethodCallTargets = std::exchange(collectedMethodCallTargets_, {});
  auto freshBridgePathChoices = std::exchange(collectedBridgePathChoices_, {});
  invalidatePilotRoutingSemanticCollectors();

  auto freshTypeMetadata = typeMetadataSnapshotForSemanticProduct();
  auto freshStructFieldMetadata = structFieldMetadataSnapshotForSemanticProduct();
  auto freshSumTypeMetadata = sumTypeMetadataSnapshotForSemanticProduct();
  auto freshSumVariantMetadata = sumVariantMetadataSnapshotForSemanticProduct();
  auto freshBindingFacts = bindingFactSnapshotForSemanticProduct();
  auto freshArrayExtentFacts = arrayExtentFactSnapshotForSemanticProduct();
  auto freshReturnFacts = returnFactSnapshotForSemanticProduct();
  auto freshLocalAutoFacts = localAutoFactSnapshotForSemanticProduct();
  auto freshQueryFacts = queryFactSnapshotForSemanticProduct();
  auto freshTryFacts = tryFactSnapshotForSemanticProduct();
  onErrorSnapshotFactCacheValid_ = false;
  onErrorSnapshotCache_.clear();
  auto freshOnErrorFacts = onErrorFactSnapshotForSemanticProduct();

  mergedWorkerPublicationFactsValid_ = true;
  auto &merged = mergedWorkerPublicationFacts_;
  mergePublicationFactKind<DirectCallTargetFactTraits>(merged.directCallTargets,
                                                       std::move(freshDirectCallTargets));
  mergePublicationFactKind<MethodCallTargetFactTraits>(merged.methodCallTargets,
                                                       std::move(freshMethodCallTargets));
  mergePublicationFactKind<BridgePathChoiceFactTraits>(merged.bridgePathChoices,
                                                       std::move(freshBridgePathChoices));
  mergePublicationFactKind<TypeMetadataFactTraits>(merged.typeMetadata,
                                                   std::move(freshTypeMetadata));
  mergePublicationFactKind<StructFieldMetadataFactTraits>(merged.structFieldMetadata,
                                                          std::move(freshStructFieldMetadata));
  mergePublicationFactKind<SumTypeMetadataFactTraits>(merged.sumTypeMetadata,
                                                      std::move(freshSumTypeMetadata));
  mergePublicationFactKind<SumVariantMetadataFactTraits>(merged.sumVariantMetadata,
                                                         std::move(freshSumVariantMetadata));
  mergePublicationFactKind<BindingFactTraits>(merged.bindingFacts, std::move(freshBindingFacts));
  mergePublicationFactKind<ArrayExtentFactTraits>(merged.arrayExtentFacts,
                                                  std::move(freshArrayExtentFacts));
  for (auto &entry : merged.arrayExtentFacts) {
    entry.targetSemanticNodeId = entry.semanticNodeId;
  }
  mergePublicationFactKind<ReturnFactTraits>(merged.returnFacts, std::move(freshReturnFacts));
  mergePublicationFactKind<LocalAutoFactTraits>(merged.localAutoFacts,
                                                std::move(freshLocalAutoFacts));
  mergePublicationFactKind<QueryFactTraits>(merged.queryFacts, std::move(freshQueryFacts));
  mergePublicationFactKind<TryFactTraits>(merged.tryFacts, std::move(freshTryFacts));
  mergePublicationFactKind<OnErrorFactTraits>(merged.onErrorFacts, std::move(freshOnErrorFacts));
  mergedWorkerPublicationFactSemanticNodeIdsCurrent_ = true;
}

void SemanticsValidator::sortCollectedOnErrorSnapshots(
    std::vector<OnErrorSnapshotEntry> &entries) {
  sortSnapshotEntries<OnErrorFactTraits>(entries);
}

void SemanticsValidator::collectPilotRoutingSemanticProductFacts() {
  if (pilotRoutingSemanticCollectorsValid_) {
    return;
  }
  // Runs after validation, so the AST is stable.
  const CallSnapshotMemoScope callSnapshotMemoScope(*this);
  if (mergedWorkerPublicationFactsValid_) {
    rebindMergedWorkerPublicationFactSemanticNodeIds();
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
  const bool useMergedWorkerPublicationFacts = mergedWorkerPublicationFactsValid_;
  if (useMergedWorkerPublicationFacts) {
    collectedDirectCallTargets_ = mergedWorkerPublicationFacts_.directCallTargets;
    collectedMethodCallTargets_ = mergedWorkerPublicationFacts_.methodCallTargets;
    collectedBridgePathChoices_ = mergedWorkerPublicationFacts_.bridgePathChoices;
    collectedCallableSummaries_ = mergedWorkerPublicationFacts_.callableSummaries;
  }

  std::function<void(const std::string &, const Expr &)> collectDirectCallExpr;
  auto collectDirectCallExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      collectDirectCallExpr(scopePath, expr);
    }
  };

  collectDirectCallExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      std::string resolvedPath;
      if (isTaskWaitExpr(expr)) {
        resolvedPath = "/task/wait";
      } else {
        resolvedPath = preferredCollectionHelperResolvedPath(expr);
      }
      if (resolvedPath.empty()) {
        resolvedPath = resolveCalleePath(expr);
      }
      if (!resolvedPath.empty()) {
        std::string soaHelperName;
        bool usesPublicSurface = false;
        if (splitSoaSurfaceHelperPath(resolvedPath, &soaHelperName, &usesPublicSurface) &&
            usesPublicSurface) {
          const std::string preferred = preferredSoaHelperTargetForCurrentImports(soaHelperName);
          if (!preferred.empty() && preferred != resolvedPath) {
            resolvedPath = preferred;
          }
        }
      }
      if (!resolvedPath.empty()) {
        std::string canonicalResolvedPath = resolvedPath;
        if (const size_t suffix = canonicalResolvedPath.find("__t");
            suffix != std::string::npos &&
            canonicalResolvedPath.find('/', suffix) == std::string::npos) {
          canonicalResolvedPath.erase(suffix);
        }
        canonicalResolvedPath =
            canonicalizeLegacySoaGetHelperPath(canonicalResolvedPath);
        canonicalResolvedPath =
            canonicalizeLegacySoaRefHelperPath(canonicalResolvedPath);
        canonicalResolvedPath =
            canonicalizeLegacySoaToAosHelperPath(canonicalResolvedPath);
        if (!canonicalResolvedPath.empty()) {
          resolvedPath = std::move(canonicalResolvedPath);
        }
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

  if (!useMergedWorkerPublicationFacts) {
    for (const auto &def : program_.definitions) {
      DefinitionContextScope definitionScope(*this, def);
      ValidationStateScope validationStateScope(*this, buildDefinitionValidationState(def));
      collectDirectCallExprs(def.fullPath, def.parameters);
      collectDirectCallExprs(def.fullPath, def.statements);
      if (def.returnExpr.has_value()) {
        collectDirectCallExpr(def.fullPath, *def.returnExpr);
      }
    }
  }

  if (!useMergedWorkerPublicationFacts) {
    for (const auto &exec : program_.executions) {
      ExecutionContextScope executionScope(*this, exec);
      collectDirectCallExprs(exec.fullPath, exec.arguments);
      collectDirectCallExprs(exec.fullPath, exec.bodyArguments);
    }
  }

  if (!useMergedWorkerPublicationFacts && !skipLocalAwareCallRefinement_) {
    // Keys identifying a call: its semantic node id when assigned, otherwise its
    // position. Bridge entries carry no call name, so their position key omits it.
    const auto nodeKey = [](uint64_t id) { return "n" + std::to_string(id); };
    const auto directPositionKey = [](const std::string &scope, const std::string &name, int line, int column) {
      return "d" + scope + '\0' + name + '\0' + std::to_string(line) + ':' + std::to_string(column);
    };
    const auto bridgePositionKey = [](const std::string &scope, int line, int column) {
      return "b" + scope + '\0' + std::to_string(line) + ':' + std::to_string(column);
    };
    std::unordered_set<uint64_t> replacedSemanticNodeIds;
    std::unordered_set<std::string> replacedDirectPositions;
    std::unordered_set<std::string> replacedBridgePositions;
    std::vector<std::optional<CollectedDirectCallTargetEntry>> refinedDirectCallTargets;
    std::vector<std::optional<CollectedBridgePathChoiceEntry>> refinedBridgePathChoices;
    std::unordered_map<std::string, size_t> refinedDirectSlot;
    std::unordered_map<std::string, size_t> refinedBridgeSlot;
    forEachLocalAwareSnapshotCall(
        [&](const Definition &def,
            const std::vector<ParameterInfo> &defParams,
            const Expr &expr,
            const std::unordered_map<std::string, BindingInfo> &activeLocals) {
          if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
            CallSnapshotData callData;
            if (inferCallSnapshotData(defParams, activeLocals, expr, callData) &&
                !callData.resolvedPath.empty()) {
              // Refinements replace the first-pass entry for the same call. Erasing from
              // the shared vectors per call made this loop quadratic in the call count,
              // so replaced calls are recorded and removed in one pass afterwards.
              const std::string directKey = expr.semanticNodeId != 0
                                                ? nodeKey(expr.semanticNodeId)
                                                : directPositionKey(def.fullPath, expr.name, expr.sourceLine, expr.sourceColumn);
              const std::string bridgeKey = expr.semanticNodeId != 0
                                                ? nodeKey(expr.semanticNodeId)
                                                : bridgePositionKey(def.fullPath, expr.sourceLine, expr.sourceColumn);
              if (expr.semanticNodeId != 0) {
                replacedSemanticNodeIds.insert(expr.semanticNodeId);
              } else {
                replacedDirectPositions.insert(directKey);
                replacedBridgePositions.insert(bridgeKey);
              }
              if (const auto slot = refinedBridgeSlot.find(bridgeKey); slot != refinedBridgeSlot.end()) {
                refinedBridgePathChoices[slot->second].reset();
                refinedBridgeSlot.erase(slot);
              }
              if (const auto slot = refinedDirectSlot.find(directKey); slot != refinedDirectSlot.end()) {
                refinedDirectCallTargets[slot->second].reset();
                refinedDirectSlot.erase(slot);
              }
              if (const auto bridgeChoice =
                      collectionBridgeChoiceFromResolvedPath(callData.resolvedPath);
                  bridgeChoice.has_value()) {
                refinedBridgeSlot[bridgeKey] = refinedBridgePathChoices.size();
                refinedBridgePathChoices.push_back(CollectedBridgePathChoiceEntry{
                    def.fullPath,
                    bridgeChoice->first,
                    bridgeChoice->second,
                    callData.resolvedPath,
                    expr.sourceLine,
                    expr.sourceColumn,
                    expr.semanticNodeId,
                });
              }
              refinedDirectSlot[directKey] = refinedDirectCallTargets.size();
              refinedDirectCallTargets.push_back(CollectedDirectCallTargetEntry{
                  def.fullPath,
                  expr.name,
                  std::move(callData.resolvedPath),
                  expr.sourceLine,
                  expr.sourceColumn,
                  expr.semanticNodeId,
              });
            }
            return;
          }
          if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty()) {
            return;
          }

          QuerySnapshotData queryData;
          if (!inferQuerySnapshotData(defParams, activeLocals, expr, queryData) ||
              queryData.resolvedPath.empty()) {
            return;
          }

          collectedMethodCallTargets_.push_back(CollectedMethodCallTargetEntry{
              def.fullPath,
              expr.name,
              std::move(queryData.resolvedPath),
              expr.sourceLine,
              expr.sourceColumn,
              std::move(queryData.receiverBinding),
              expr.semanticNodeId,
          });
        });
    const auto isReplacedDirect = [&](const CollectedDirectCallTargetEntry &entry) {
      if (entry.semanticNodeId != 0) {
        return replacedSemanticNodeIds.count(entry.semanticNodeId) != 0;
      }
      return replacedDirectPositions.count(
                 directPositionKey(entry.scopePath, entry.callName, entry.sourceLine, entry.sourceColumn)) != 0;
    };
    const auto isReplacedBridge = [&](const CollectedBridgePathChoiceEntry &entry) {
      if (entry.semanticNodeId != 0) {
        return replacedSemanticNodeIds.count(entry.semanticNodeId) != 0;
      }
      return replacedBridgePositions.count(bridgePositionKey(entry.scopePath, entry.sourceLine, entry.sourceColumn)) != 0;
    };
    collectedDirectCallTargets_.erase(
        std::remove_if(collectedDirectCallTargets_.begin(), collectedDirectCallTargets_.end(), isReplacedDirect),
        collectedDirectCallTargets_.end());
    collectedBridgePathChoices_.erase(
        std::remove_if(collectedBridgePathChoices_.begin(), collectedBridgePathChoices_.end(), isReplacedBridge),
        collectedBridgePathChoices_.end());
    for (auto &entry : refinedDirectCallTargets) {
      if (entry.has_value()) {
        collectedDirectCallTargets_.push_back(std::move(*entry));
      }
    }
    for (auto &entry : refinedBridgePathChoices) {
      if (entry.has_value()) {
        collectedBridgePathChoices_.push_back(std::move(*entry));
      }
    }
  }

  if (useMergedWorkerPublicationFacts) {
    collectExecutionCallableSummaryEntries(collectedCallableSummaries_);
    rebindCollectedCallableSummarySemanticNodeIds(collectedCallableSummaries_);
  } else {
    collectCallableSummaryEntriesForStableRange(
        0, validationPlan_->definitionPrepass.declarationsInStableOrder.size(), collectedCallableSummaries_);
    collectExecutionCallableSummaryEntries(collectedCallableSummaries_);
  }

  sortCollectedCallableSummaries(collectedCallableSummaries_);

  sortSnapshotEntries<DirectCallTargetFactTraits>(collectedDirectCallTargets_);
  sortSnapshotEntries<MethodCallTargetFactTraits>(collectedMethodCallTargets_);
  sortSnapshotEntries<BridgePathChoiceFactTraits>(collectedBridgePathChoices_);
  pilotRoutingSemanticCollectorsValid_ = true;
}

} // namespace primec::semantics

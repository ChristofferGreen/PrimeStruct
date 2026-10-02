// Definition publication facts, worker-fact rebinding and pilot routing collection.
#include "SemanticsValidatorSnapshotHelpers.h"

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
  const auto directCallTargetSnapshotKey =
      [](const CollectedDirectCallTargetEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.callName,
                           entry.sourceLine,
                           entry.sourceColumn,
                           entry.resolvedPath);
      };
  const auto directCallTargetIdentityKey =
      [](const CollectedDirectCallTargetEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.callName,
                           entry.sourceLine,
                           entry.sourceColumn);
      };
  const bool prunedDirectCallTargets = pruneReplacedEntriesBySnapshotKey(
      mergedWorkerPublicationFacts_.directCallTargets,
      freshDirectCallTargets,
      directCallTargetIdentityKey,
      directCallTargetSnapshotKey);
  if (appendMissingEntriesBySnapshotKey(
          mergedWorkerPublicationFacts_.directCallTargets,
          freshDirectCallTargets,
          directCallTargetSnapshotKey) ||
      prunedDirectCallTargets) {
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
                       if (left.resolvedPath != right.resolvedPath) {
                         return left.resolvedPath < right.resolvedPath;
                       }
                       return left.semanticNodeId < right.semanticNodeId;
                     });
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.directCallTargets,
      std::move(freshDirectCallTargets),
      directCallTargetSnapshotKey);
  const auto methodCallTargetSnapshotKey = [](const CollectedMethodCallTargetEntry &entry) {
    return snapshotKey(entry.scopePath,
                       entry.methodName,
                       entry.sourceLine,
                       entry.sourceColumn,
                       entry.resolvedPath);
  };
  if (appendMissingEntriesBySnapshotKey(
          mergedWorkerPublicationFacts_.methodCallTargets,
          freshMethodCallTargets,
          methodCallTargetSnapshotKey)) {
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
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.methodCallTargets,
      std::move(freshMethodCallTargets),
      methodCallTargetSnapshotKey);
  const auto bridgePathChoiceSnapshotKey = [](const CollectedBridgePathChoiceEntry &entry) {
    return snapshotKey(entry.scopePath,
                       entry.collectionFamily,
                       entry.sourceLine,
                       entry.sourceColumn,
                       entry.helperName,
                       entry.chosenPath);
  };
  if (appendMissingEntriesBySnapshotKey(
          mergedWorkerPublicationFacts_.bridgePathChoices,
          freshBridgePathChoices,
          bridgePathChoiceSnapshotKey)) {
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
                       if (left.chosenPath != right.chosenPath) {
                         return left.chosenPath < right.chosenPath;
                       }
                       return left.semanticNodeId < right.semanticNodeId;
                     });
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.bridgePathChoices,
      std::move(freshBridgePathChoices),
      bridgePathChoiceSnapshotKey);
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.typeMetadata,
      std::move(freshTypeMetadata),
      [](const TypeMetadataSnapshotEntry &entry) {
        return snapshotKey(entry.fullPath, entry.category, 0, 0);
      });
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.structFieldMetadata,
      std::move(freshStructFieldMetadata),
      [](const StructFieldMetadataSnapshotEntry &entry) {
        return snapshotKey(entry.structPath,
                           entry.fieldName,
                           0,
                           0,
                           std::to_string(entry.fieldIndex));
      });
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.sumTypeMetadata,
      std::move(freshSumTypeMetadata),
      [](const SumTypeMetadataSnapshotEntry &entry) {
        return snapshotKey(entry.fullPath, std::string_view{}, 0, 0);
      });
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.sumVariantMetadata,
      std::move(freshSumVariantMetadata),
      [](const SumVariantMetadataSnapshotEntry &entry) {
        return snapshotKey(entry.sumPath,
                           entry.variantName,
                           0,
                           0,
                           std::to_string(entry.variantIndex));
      });
  const auto bindingFactSnapshotKey =
      [](const BindingFactSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.siteKind,
                           entry.sourceLine,
                           entry.sourceColumn,
                           entry.name,
                           entry.resolvedPath);
      };
  if (appendMissingEntriesBySnapshotKey(
          mergedWorkerPublicationFacts_.bindingFacts,
          freshBindingFacts,
          bindingFactSnapshotKey)) {
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
                       if (left.resolvedPath != right.resolvedPath) {
                         return left.resolvedPath < right.resolvedPath;
                       }
                       return left.semanticNodeId < right.semanticNodeId;
                     });
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.bindingFacts,
      std::move(freshBindingFacts),
      bindingFactSnapshotKey);
  const auto arrayExtentFactSnapshotKey =
      [](const ArrayExtentFactSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.siteKind,
                           entry.sourceLine,
                           entry.sourceColumn,
                           entry.targetName,
                           entry.targetResolvedPath,
                           entry.extentExpression);
      };
  const auto arrayExtentFactIdentityKey =
      [](const ArrayExtentFactSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.siteKind,
                           entry.sourceLine,
                           entry.sourceColumn,
                           entry.targetName);
      };
  const bool prunedArrayExtentFacts = pruneReplacedEntriesBySnapshotKey(
      mergedWorkerPublicationFacts_.arrayExtentFacts,
      freshArrayExtentFacts,
      arrayExtentFactIdentityKey,
      arrayExtentFactSnapshotKey);
  if (appendMissingEntriesBySnapshotKey(
          mergedWorkerPublicationFacts_.arrayExtentFacts,
          freshArrayExtentFacts,
          arrayExtentFactSnapshotKey) ||
      prunedArrayExtentFacts) {
    std::stable_sort(mergedWorkerPublicationFacts_.arrayExtentFacts.begin(),
                     mergedWorkerPublicationFacts_.arrayExtentFacts.end(),
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
                       if (left.targetName != right.targetName) {
                         return left.targetName < right.targetName;
                       }
                       if (left.extentExpression != right.extentExpression) {
                         return left.extentExpression < right.extentExpression;
                       }
                       return left.semanticNodeId < right.semanticNodeId;
                     });
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.arrayExtentFacts,
      std::move(freshArrayExtentFacts),
      arrayExtentFactSnapshotKey);
  for (auto &entry : mergedWorkerPublicationFacts_.arrayExtentFacts) {
    entry.targetSemanticNodeId = entry.semanticNodeId;
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.returnFacts,
      std::move(freshReturnFacts),
      [](const ReturnFactSnapshotEntry &entry) {
        return snapshotKey(entry.definitionPath, std::string_view{}, 0, 0);
      });
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.localAutoFacts,
      std::move(freshLocalAutoFacts),
      [](const LocalAutoBindingSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.bindingName,
                           entry.sourceLine,
                           entry.sourceColumn);
      });
  const auto queryFactSnapshotKey =
      [](const QueryFactSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.callName,
                           entry.sourceLine,
                           entry.sourceColumn,
                           entry.resolvedPath);
      };
  const auto queryFactIdentityKey =
      [](const QueryFactSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.callName,
                           entry.sourceLine,
                           entry.sourceColumn);
      };
  const bool prunedQueryFacts = pruneReplacedEntriesBySnapshotKey(
      mergedWorkerPublicationFacts_.queryFacts,
      freshQueryFacts,
      queryFactIdentityKey,
      queryFactSnapshotKey);
  if (appendMissingEntriesBySnapshotKey(
          mergedWorkerPublicationFacts_.queryFacts,
          freshQueryFacts,
          queryFactSnapshotKey) ||
      prunedQueryFacts) {
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
                       if (left.resolvedPath != right.resolvedPath) {
                         return left.resolvedPath < right.resolvedPath;
                       }
                       return left.semanticNodeId < right.semanticNodeId;
                     });
  }
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.queryFacts,
      std::move(freshQueryFacts),
      queryFactSnapshotKey);
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.tryFacts,
      std::move(freshTryFacts),
      [](const TryValueSnapshotEntry &entry) {
        return snapshotKey(entry.scopePath,
                           entry.operandResolvedPath,
                           entry.sourceLine,
                           entry.sourceColumn);
      });
  rebindSemanticNodeIdsBySnapshotKey(
      mergedWorkerPublicationFacts_.onErrorFacts,
      std::move(freshOnErrorFacts),
      [](const OnErrorSnapshotEntry &entry) {
        return snapshotKey(entry.definitionPath, std::string_view{}, 0, 0);
      });
  mergedWorkerPublicationFactSemanticNodeIdsCurrent_ = true;
}

void SemanticsValidator::sortCollectedOnErrorSnapshots(
    std::vector<OnErrorSnapshotEntry> &entries) {
  std::stable_sort(entries.begin(),
                   entries.end(),
                   [](const auto &left, const auto &right) {
                     if (left.definitionPath != right.definitionPath) {
                       return left.definitionPath < right.definitionPath;
                     }
                     return left.semanticNodeId < right.semanticNodeId;
                   });
}

void SemanticsValidator::collectPilotRoutingSemanticProductFacts() {
  if (pilotRoutingSemanticCollectorsValid_) {
    return;
  }
  // Runs after validation, so the AST is stable (TODO-5382).
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
    forEachLocalAwareSnapshotCall(
        [&](const Definition &def,
            const std::vector<ParameterInfo> &defParams,
            const Expr &expr,
            const std::unordered_map<std::string, BindingInfo> &activeLocals) {
          if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
            CallSnapshotData callData;
            if (inferCallSnapshotData(defParams, activeLocals, expr, callData) &&
                !callData.resolvedPath.empty()) {
              collectedDirectCallTargets_.erase(
                  std::remove_if(
                      collectedDirectCallTargets_.begin(),
                      collectedDirectCallTargets_.end(),
                      [&](const CollectedDirectCallTargetEntry &entry) {
                        if (expr.semanticNodeId != 0) {
                          return entry.semanticNodeId == expr.semanticNodeId;
                        }
                        return entry.scopePath == def.fullPath &&
                               entry.callName == expr.name &&
                               entry.sourceLine == expr.sourceLine &&
                               entry.sourceColumn == expr.sourceColumn;
                      }),
                  collectedDirectCallTargets_.end());
              collectedBridgePathChoices_.erase(
                  std::remove_if(
                      collectedBridgePathChoices_.begin(),
                      collectedBridgePathChoices_.end(),
                      [&](const CollectedBridgePathChoiceEntry &entry) {
                        if (expr.semanticNodeId != 0) {
                          return entry.semanticNodeId == expr.semanticNodeId;
                        }
                        return entry.scopePath == def.fullPath &&
                               entry.sourceLine == expr.sourceLine &&
                               entry.sourceColumn == expr.sourceColumn;
                      }),
                  collectedBridgePathChoices_.end());
              if (const auto bridgeChoice =
                      collectionBridgeChoiceFromResolvedPath(callData.resolvedPath);
                  bridgeChoice.has_value()) {
                collectedBridgePathChoices_.push_back(CollectedBridgePathChoiceEntry{
                    def.fullPath,
                    bridgeChoice->first,
                    bridgeChoice->second,
                    callData.resolvedPath,
                    expr.sourceLine,
                    expr.sourceColumn,
                    expr.semanticNodeId,
                });
              }
              collectedDirectCallTargets_.push_back(CollectedDirectCallTargetEntry{
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
                     if (left.resolvedPath != right.resolvedPath) {
                       return left.resolvedPath < right.resolvedPath;
                     }
                     return left.semanticNodeId < right.semanticNodeId;
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
                     if (left.resolvedPath != right.resolvedPath) {
                       return left.resolvedPath < right.resolvedPath;
                     }
                     return left.semanticNodeId < right.semanticNodeId;
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
                     if (left.chosenPath != right.chosenPath) {
                       return left.chosenPath < right.chosenPath;
                     }
                     return left.semanticNodeId < right.semanticNodeId;
                   });
  pilotRoutingSemanticCollectorsValid_ = true;
}

} // namespace primec::semantics

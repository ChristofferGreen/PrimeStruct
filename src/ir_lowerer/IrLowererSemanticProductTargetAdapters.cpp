#include "IrLowererSemanticProductTargetAdapters.h"

#include <optional>

namespace primec::ir_lowerer {

namespace {

template <typename FactT>
const FactT *findDefinitionScopedSemanticFact(const std::unordered_map<uint64_t, const FactT *> &factsByDefinitionId,
                                              const Definition &definition) {
  if (definition.semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = factsByDefinitionId.find(definition.semanticNodeId); it != factsByDefinitionId.end()) {
    return it->second;
  }
  return nullptr;
}

template <typename FactT>
const FactT *findExpressionScopedSemanticFact(const std::unordered_map<uint64_t, const FactT *> &factsByExpr,
                                              const Expr &expr) {
  if (expr.semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = factsByExpr.find(expr.semanticNodeId); it != factsByExpr.end()) {
    return it->second;
  }
  return nullptr;
}

template <typename EntryT>
const EntryT *findSemanticProductEntryByPublishedIndex(const std::vector<EntryT> &entries,
                                                       std::size_t entryIndex) {
  if (entryIndex >= entries.size()) {
    return nullptr;
  }
  return &entries[entryIndex];
}

template <typename KeyT, typename EntryT>
void populateSemanticFactIndex(std::unordered_map<KeyT, const EntryT *> &destination,
                               const std::unordered_map<KeyT, std::size_t> &publishedIndices,
                               const std::vector<EntryT> &entries) {
  destination.reserve(publishedIndices.size());
  for (const auto &[key, entryIndex] : publishedIndices) {
    if (const auto *entry = findSemanticProductEntryByPublishedIndex(entries, entryIndex);
        entry != nullptr) {
      destination.insert_or_assign(key, entry);
    }
  }
}

uint64_t makeLocalAutoInitPathBindingNameKey(SymbolId initializerPathId, SymbolId bindingNameId) {
  return (static_cast<uint64_t>(initializerPathId) << 32) |
         static_cast<uint64_t>(bindingNameId);
}

uint64_t makeQueryFactResolvedPathCallNameKey(SymbolId resolvedPathId, SymbolId callNameId) {
  return (static_cast<uint64_t>(resolvedPathId) << 32) |
         static_cast<uint64_t>(callNameId);
}

uint64_t makeTryFactOperandPathSourceKey(SymbolId operandPathId, int sourceLine, int sourceColumn) {
  const uint64_t lineBits = static_cast<uint64_t>(
      static_cast<uint32_t>(sourceLine > 0 ? sourceLine : 0));
  const uint64_t columnBits = static_cast<uint64_t>(
      static_cast<uint32_t>(sourceColumn > 0 ? sourceColumn : 0));
  return (static_cast<uint64_t>(operandPathId) << 32) ^
         (lineBits * 1315423911ULL) ^
         columnBits;
}

struct SemanticProductIndexBuilder {
  const SemanticProgram *semanticProgram = nullptr;

  SemanticProductIndex build() const {
    SemanticProductIndex index;
    if (semanticProgram == nullptr) {
      return index;
    }
    buildOnErrorIndex(index);
    buildReturnIndex(index);
    buildLocalAutoIndex(index);
    buildQueryIndex(index);
    buildTryIndex(index);
    buildCollectionSpecializationIndex(index);
    buildBindingIndex(index);
    return index;
  }

  void buildOnErrorIndex(SemanticProductIndex &index) const {
    if (!semanticProgram->publishedRoutingLookups.onErrorFactIndicesByDefinitionId.empty() ||
        !semanticProgram->publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.empty()) {
      populateSemanticFactIndex(index.onErrorFactsByDefinitionId,
                                semanticProgram->publishedRoutingLookups.onErrorFactIndicesByDefinitionId,
                                semanticProgram->onErrorFacts);
      populateSemanticFactIndex(index.onErrorFactsByDefinitionPathId,
                                semanticProgram->publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId,
                                semanticProgram->onErrorFacts);
      return;
    }
    const auto onErrorFacts = semanticProgramOnErrorFactView(*semanticProgram);
    index.onErrorFactsByDefinitionId.reserve(onErrorFacts.size());
    index.onErrorFactsByDefinitionPathId.reserve(onErrorFacts.size());
    for (const auto *entry : onErrorFacts) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.onErrorFactsByDefinitionId.insert_or_assign(entry->semanticNodeId, entry);
      }
      if (entry->definitionPathId == InvalidSymbolId) {
        continue;
      }
      const std::string_view definitionPath =
          semanticProgramResolveCallTargetString(*semanticProgram, entry->definitionPathId);
      if (!definitionPath.empty()) {
        index.onErrorFactsByDefinitionPathId.insert_or_assign(entry->definitionPathId, entry);
      }
    }
  }

  void buildReturnIndex(SemanticProductIndex &index) const {
    const auto returnFacts = semanticProgramReturnFactView(*semanticProgram);
    index.returnFactsByDefinitionId.reserve(returnFacts.size());
    index.returnFactsByDefinitionPathId.reserve(returnFacts.size());
    for (const auto *entry : returnFacts) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.returnFactsByDefinitionId.insert_or_assign(entry->semanticNodeId, entry);
      }
      if (entry->definitionPathId == InvalidSymbolId) {
        continue;
      }
      const std::string_view definitionPath =
          semanticProgramResolveCallTargetString(*semanticProgram, entry->definitionPathId);
      if (!definitionPath.empty()) {
        index.returnFactsByDefinitionPathId.insert_or_assign(entry->definitionPathId, entry);
      }
    }
  }

  void buildLocalAutoIndex(SemanticProductIndex &index) const {
    if (!semanticProgram->publishedRoutingLookups.localAutoFactIndicesByExpr.empty() ||
        !semanticProgram->publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.empty()) {
      populateSemanticFactIndex(index.localAutoFactsByExpr,
                                semanticProgram->publishedRoutingLookups.localAutoFactIndicesByExpr,
                                semanticProgram->localAutoFacts);
      populateSemanticFactIndex(index.localAutoFactsByInitPathAndBindingNameId,
                                semanticProgram->publishedRoutingLookups
                                    .localAutoFactIndicesByInitPathAndBindingNameId,
                                semanticProgram->localAutoFacts);
      return;
    }
    const auto localAutoFacts = semanticProgramLocalAutoFactView(*semanticProgram);
    index.localAutoFactsByExpr.reserve(localAutoFacts.size());
    index.localAutoFactsByInitPathAndBindingNameId.reserve(localAutoFacts.size());
    for (const auto *entry : localAutoFacts) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.localAutoFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
      if (entry->initializerResolvedPathId == InvalidSymbolId ||
          entry->bindingNameId == InvalidSymbolId) {
        continue;
      }
      index.localAutoFactsByInitPathAndBindingNameId.insert_or_assign(
          makeLocalAutoInitPathBindingNameKey(entry->initializerResolvedPathId, entry->bindingNameId),
          entry);
    }
  }

  void buildQueryIndex(SemanticProductIndex &index) const {
    if (!semanticProgram->publishedRoutingLookups.queryFactIndicesByExpr.empty() ||
        !semanticProgram->publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.empty()) {
      populateSemanticFactIndex(index.queryFactsByExpr,
                                semanticProgram->publishedRoutingLookups.queryFactIndicesByExpr,
                                semanticProgram->queryFacts);
      populateSemanticFactIndex(index.queryFactsByResolvedPathAndCallNameId,
                                semanticProgram->publishedRoutingLookups
                                    .queryFactIndicesByResolvedPathAndCallNameId,
                                semanticProgram->queryFacts);
      return;
    }
    const auto queryFacts = semanticProgramQueryFactView(*semanticProgram);
    index.queryFactsByExpr.reserve(queryFacts.size());
    index.queryFactsByResolvedPathAndCallNameId.reserve(queryFacts.size());
    for (const auto *entry : queryFacts) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.queryFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
      if (entry->resolvedPathId == InvalidSymbolId ||
          entry->callNameId == InvalidSymbolId) {
        continue;
      }
      index.queryFactsByResolvedPathAndCallNameId.insert_or_assign(
          makeQueryFactResolvedPathCallNameKey(entry->resolvedPathId, entry->callNameId),
          entry);
    }
  }

  void buildTryIndex(SemanticProductIndex &index) const {
    if (!semanticProgram->publishedRoutingLookups.tryFactIndicesByExpr.empty() ||
        !semanticProgram->publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.empty()) {
      populateSemanticFactIndex(index.tryFactsByExpr,
                                semanticProgram->publishedRoutingLookups.tryFactIndicesByExpr,
                                semanticProgram->tryFacts);
      populateSemanticFactIndex(index.tryFactsByOperandPathAndSource,
                                semanticProgram->publishedRoutingLookups.tryFactIndicesByOperandPathAndSource,
                                semanticProgram->tryFacts);
      return;
    }
    const auto tryFacts = semanticProgramTryFactView(*semanticProgram);
    index.tryFactsByExpr.reserve(tryFacts.size());
    index.tryFactsByOperandPathAndSource.reserve(tryFacts.size());
    for (const auto *entry : tryFacts) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.tryFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
      if (entry->operandResolvedPathId == InvalidSymbolId ||
          entry->sourceLine <= 0 ||
          entry->sourceColumn <= 0) {
        continue;
      }
      index.tryFactsByOperandPathAndSource.insert_or_assign(
          makeTryFactOperandPathSourceKey(entry->operandResolvedPathId,
                                          entry->sourceLine,
                                          entry->sourceColumn),
          entry);
    }
  }

  void buildBindingIndex(SemanticProductIndex &index) const {
    const auto bindingFacts = semanticProgramBindingFactView(*semanticProgram);
    index.bindingFactsByExpr.reserve(bindingFacts.size());
    for (const auto *entry : bindingFacts) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.bindingFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
    }
  }

  void buildCollectionSpecializationIndex(SemanticProductIndex &index) const {
    if (!semanticProgram->publishedRoutingLookups.collectionSpecializationIndicesByExpr.empty()) {
      populateSemanticFactIndex(
          index.collectionSpecializationsByExpr,
          semanticProgram->publishedRoutingLookups.collectionSpecializationIndicesByExpr,
          semanticProgram->collectionSpecializations);
      return;
    }
    const auto collectionSpecializations = semanticProgramCollectionSpecializationView(*semanticProgram);
    index.collectionSpecializationsByExpr.reserve(collectionSpecializations.size());
    for (const auto *entry : collectionSpecializations) {
      if (entry == nullptr) {
        continue;
      }
      if (entry->semanticNodeId != 0) {
        index.collectionSpecializationsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
    }
  }
};

} // namespace

SemanticProductIndex buildSemanticProductIndex(const SemanticProgram *semanticProgram) {
  return SemanticProductIndexBuilder{semanticProgram}.build();
}

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;
  adapter.semanticProgram = semanticProgram;
  adapter.publishedRoutingLookups = &semanticProgram->publishedRoutingLookups;
  adapter.semanticIndex = buildSemanticProductIndex(semanticProgram);

  return adapter;
}

std::string findSemanticProductDirectCallTarget(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (semanticProgram == nullptr) {
    return {};
  }
  auto resolvePathId = [&](SymbolId pathId) -> std::string {
    if (pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
  };
  if (expr.semanticNodeId != 0) {
    if (const auto pathId = semanticProgramLookupPublishedDirectCallTargetId(*semanticProgram,
                                                                             expr.semanticNodeId);
        pathId.has_value()) {
      return resolvePathId(*pathId);
    }
  }
  return {};
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductDirectCallTarget(adapter.semanticProgram, expr);
}

std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr) {
    return std::nullopt;
  }
  if (expr.semanticNodeId != 0) {
    return semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
        *semanticProgram, expr.semanticNodeId);
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductDirectCallStdlibSurfaceId(adapter.semanticProgram, expr);
}

std::string findSemanticProductMethodCallTarget(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (semanticProgram == nullptr) {
    return {};
  }
  auto resolvePathId = [&](SymbolId pathId) -> std::string {
    if (pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
  };
  if (expr.semanticNodeId != 0) {
    if (const auto pathId = semanticProgramLookupPublishedMethodCallTargetId(*semanticProgram,
                                                                             expr.semanticNodeId);
        pathId.has_value()) {
      return resolvePathId(*pathId);
    }
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductMethodCallTarget(adapter.semanticProgram, expr);
}

std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr) {
    return std::nullopt;
  }
  if (expr.semanticNodeId != 0) {
    return semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
        *semanticProgram, expr.semanticNodeId);
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductMethodCallStdlibSurfaceId(adapter.semanticProgram, expr);
}

std::string findSemanticProductBridgePathChoice(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto pathId = semanticProgramLookupPublishedBridgePathChoiceId(*semanticProgram,
                                                                           expr.semanticNodeId);
      pathId.has_value() && *pathId != InvalidSymbolId) {
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(*semanticProgram, *pathId);
    if (!chosenPath.empty()) {
      return std::string(chosenPath);
    }
  }
  return {};
}

std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductBridgePathChoice(adapter.semanticProgram, expr);
}

std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return std::nullopt;
  }
  return semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
      *semanticProgram, expr.semanticNodeId);
}

std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter.semanticProgram, expr);
}

const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProgram *semanticProgram,
                                                                        const std::string &fullPath) {
  if (fullPath.empty() || semanticProgram == nullptr) {
    return nullptr;
  }
  return semanticProgramLookupPublishedCallableSummary(*semanticProgram, fullPath);
}

const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProductTargetAdapter &adapter,
                                                                        const std::string &fullPath) {
  return findSemanticProductCallableSummary(adapter.semanticProgram, fullPath);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  return findDefinitionScopedSemanticFact(semanticIndex.onErrorFactsByDefinitionId, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Definition &definition) {
  return findSemanticProductOnErrorFactBySemanticId(adapter.semanticIndex, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
            *semanticProgram, definition.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductOnErrorFactBySemanticId(semanticIndex, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition) {
  return findSemanticProductOnErrorFact(adapter.semanticProgram, adapter.semanticIndex, definition);
}

const SemanticProgramReturnFact *findSemanticProductReturnFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  return findDefinitionScopedSemanticFact(semanticIndex.returnFactsByDefinitionId, definition);
}

const SemanticProgramReturnFact *findSemanticProductReturnFact(
    const SemanticProgram *,
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  return findSemanticProductReturnFactBySemanticId(semanticIndex, definition);
}

const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const Definition &definition) {
  return findSemanticProductReturnFact(adapter.semanticProgram, adapter.semanticIndex, definition);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.localAutoFactsByExpr, expr);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductLocalAutoFactBySemanticId(adapter.semanticIndex, expr);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedLocalAutoFactBySemanticId(
            *semanticProgram, expr.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr) {
  return findSemanticProductLocalAutoFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.queryFactsByExpr, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductQueryFactBySemanticId(adapter.semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedQueryFactBySemanticId(
            *semanticProgram, expr.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductQueryFactBySemanticId(semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr) {
  return findSemanticProductQueryFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.tryFactsByExpr, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductTryFactBySemanticId(adapter.semanticIndex, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedTryFactBySemanticId(
            *semanticProgram, expr.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductTryFactBySemanticId(semanticIndex, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr) {
  return findSemanticProductTryFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductIndex &semanticIndex,
                                                                const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(semanticIndex.bindingFactsByExpr, expr);
      fact != nullptr) {
    return fact;
  }
  return nullptr;
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr) {
  return findSemanticProductBindingFact(adapter.semanticIndex, expr);
}

const SemanticProgramBindingFact *findSemanticProductBindingFactByScopeAndName(
    const SemanticProductTargetAdapter &adapter,
    std::string_view scopePath,
    std::string_view name) {
  if (adapter.semanticProgram == nullptr || scopePath.empty() || name.empty()) {
    return nullptr;
  }
  const auto bindingFacts = semanticProgramBindingFactView(*adapter.semanticProgram);
  for (const auto *entry : bindingFacts) {
    if (entry == nullptr) {
      continue;
    }
    if (entry->scopePath == scopePath && entry->name == name) {
      return entry;
    }
  }
  return nullptr;
}

const SemanticProgramSumTypeMetadata *findSemanticProductSumTypeMetadata(
    const SemanticProductTargetAdapter &adapter,
    std::string_view fullPath) {
  if (adapter.semanticProgram == nullptr || fullPath.empty()) {
    return nullptr;
  }
  for (const auto &entry : adapter.semanticProgram->sumTypeMetadata) {
    if (entry.fullPath == fullPath) {
      return &entry;
    }
  }
  return nullptr;
}

const SemanticProgramCollectionSpecialization *findSemanticProductCollectionSpecialization(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.collectionSpecializationsByExpr, expr);
}

const SemanticProgramCollectionSpecialization *findSemanticProductCollectionSpecialization(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductCollectionSpecialization(adapter.semanticIndex, expr);
}

} // namespace primec::ir_lowerer

#include "IrLowererSemanticProductTargetAdapters.h"

#include <algorithm>
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

std::optional<SymbolId> resolveSemanticExprPathId(const SemanticProductTargetAdapter &adapter,
                                                  const Expr &expr) {
  if (adapter.semanticProgram == nullptr) {
    return std::nullopt;
  }

  auto lookupPathId = [&](const std::string &resolvedPath) -> std::optional<SymbolId> {
    if (resolvedPath.empty()) {
      return std::nullopt;
    }
    return semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, resolvedPath);
  };

  if (const auto directPathId =
          lookupPathId(findSemanticProductDirectCallTarget(adapter, expr));
      directPathId.has_value()) {
    return directPathId;
  }
  if (const auto methodPathId =
          lookupPathId(findSemanticProductMethodCallTarget(adapter, expr));
      methodPathId.has_value()) {
    return methodPathId;
  }
  if (const auto bridgePathId =
          lookupPathId(findSemanticProductBridgePathChoice(adapter, expr));
      bridgePathId.has_value()) {
    return bridgePathId;
  }
  if (!expr.name.empty() && expr.name.front() == '/') {
    return lookupPathId(expr.name);
  }
  return std::nullopt;
}

std::optional<SymbolId> resolveLocalAutoInitializerPathId(const SemanticProductTargetAdapter &adapter,
                                                          const Expr &bindingExpr) {
  if (bindingExpr.args.empty()) {
    return std::nullopt;
  }

  const Expr &initializerExpr = bindingExpr.args.front();
  if (const auto initializerPathId = resolveSemanticExprPathId(adapter, initializerExpr);
      initializerPathId.has_value()) {
    return initializerPathId;
  }
  return std::nullopt;
}

struct SemanticProductIndexBuilder {
  const SemanticProgram *semanticProgram = nullptr;

  SemanticProductIndex build() const {
    SemanticProductIndex index;
    if (semanticProgram == nullptr) {
      return index;
    }
    buildDirectCallIndex(index);
    buildMethodCallIndex(index);
    buildBridgePathChoiceIndex(index);
    buildOnErrorIndex(index);
    buildReturnIndex(index);
    buildLocalAutoIndex(index);
    buildQueryIndex(index);
    buildTryIndex(index);
    buildBindingIndex(index);
    return index;
  }

  void buildDirectCallIndex(SemanticProductIndex &index) const {
    const auto directCallTargets = semanticProgramDirectCallTargetView(*semanticProgram);
    index.directCallTargetIdsByExpr.reserve(directCallTargets.size());
    for (const auto *entry : directCallTargets) {
      if (entry->semanticNodeId == 0 || entry->resolvedPathId == InvalidSymbolId) {
        continue;
      }
      const std::string_view resolvedPath =
          semanticProgramResolveCallTargetString(*semanticProgram, entry->resolvedPathId);
      if (!resolvedPath.empty()) {
        index.directCallTargetIdsByExpr.insert_or_assign(entry->semanticNodeId, entry->resolvedPathId);
      }
    }
  }

  void buildMethodCallIndex(SemanticProductIndex &index) const {
    const auto methodCallTargets = semanticProgramMethodCallTargetView(*semanticProgram);
    index.methodCallTargetIdsByExpr.reserve(methodCallTargets.size());
    for (const auto *entry : methodCallTargets) {
      if (entry->semanticNodeId == 0 || entry->resolvedPathId == InvalidSymbolId) {
        continue;
      }
      const std::string_view resolvedPath =
          semanticProgramResolveCallTargetString(*semanticProgram, entry->resolvedPathId);
      if (!resolvedPath.empty()) {
        index.methodCallTargetIdsByExpr.insert_or_assign(entry->semanticNodeId, entry->resolvedPathId);
      }
    }
  }

  void buildBridgePathChoiceIndex(SemanticProductIndex &index) const {
    const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
    index.bridgePathChoiceIdsByExpr.reserve(bridgePathChoices.size());
    for (const auto *entry : bridgePathChoices) {
      const std::string_view helperName =
          semanticProgramBridgePathChoiceHelperName(*semanticProgram, *entry);
      const std::string_view chosenPath =
          semanticProgramResolveCallTargetString(*semanticProgram, entry->chosenPathId);
      if (entry->semanticNodeId != 0 &&
          entry->chosenPathId != InvalidSymbolId &&
          entry->helperNameId != InvalidSymbolId &&
          !helperName.empty() &&
          !chosenPath.empty()) {
        index.bridgePathChoiceIdsByExpr.insert_or_assign(entry->semanticNodeId, entry->chosenPathId);
      }
    }
  }

  void buildOnErrorIndex(SemanticProductIndex &index) const {
    const auto onErrorFacts = semanticProgramOnErrorFactView(*semanticProgram);
    index.onErrorFactsByDefinitionId.reserve(onErrorFacts.size());
    index.onErrorFactsByDefinitionPathId.reserve(onErrorFacts.size());
    for (const auto *entry : onErrorFacts) {
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
    const auto localAutoFacts = semanticProgramLocalAutoFactView(*semanticProgram);
    index.localAutoFactsByExpr.reserve(localAutoFacts.size());
    index.localAutoFactsByInitPathAndBindingNameId.reserve(localAutoFacts.size());
    for (const auto *entry : localAutoFacts) {
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
    const auto queryFacts = semanticProgramQueryFactView(*semanticProgram);
    index.queryFactsByExpr.reserve(queryFacts.size());
    index.queryFactsByResolvedPathAndCallNameId.reserve(queryFacts.size());
    for (const auto *entry : queryFacts) {
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
    const auto tryFacts = semanticProgramTryFactView(*semanticProgram);
    index.tryFactsByExpr.reserve(tryFacts.size());
    for (const auto *entry : tryFacts) {
      if (entry->semanticNodeId != 0) {
        index.tryFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
    }
  }

  void buildBindingIndex(SemanticProductIndex &index) const {
    const auto bindingFacts = semanticProgramBindingFactView(*semanticProgram);
    index.bindingFactsByExpr.reserve(bindingFacts.size());
    for (const auto *entry : bindingFacts) {
      if (entry->semanticNodeId != 0) {
        index.bindingFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
      }
    }
  }
};

std::optional<SymbolId> lookupDirectCallTargetId(const SemanticProductIndex &index,
                                                 uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it = index.directCallTargetIdsByExpr.find(semanticNodeId);
      it != index.directCallTargetIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<SymbolId> lookupMethodCallTargetId(const SemanticProductIndex &index,
                                                 uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it = index.methodCallTargetIdsByExpr.find(semanticNodeId);
      it != index.methodCallTargetIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<SymbolId> lookupBridgePathChoiceId(const SemanticProductIndex &index,
                                                 uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it = index.bridgePathChoiceIdsByExpr.find(semanticNodeId);
      it != index.bridgePathChoiceIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

} // namespace

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;
  adapter.semanticProgram = semanticProgram;
  adapter.semanticIndex = SemanticProductIndexBuilder{semanticProgram}.build();

  const auto callableSummaries = semanticProgramCallableSummaryView(*semanticProgram);
  adapter.callableSummariesByPathId.reserve(callableSummaries.size());
  for (const auto *entry : callableSummaries) {
    if (entry->fullPathId == InvalidSymbolId) {
      continue;
    }
    const std::string_view fullPath =
        semanticProgramCallableSummaryFullPath(*semanticProgram, *entry);
    if (!fullPath.empty()) {
      adapter.callableSummariesByPathId.insert_or_assign(entry->fullPathId, entry);
    }
  }

  adapter.typeMetadataByPath.reserve(semanticProgram->typeMetadata.size());
  adapter.orderedStructTypeMetadata.reserve(semanticProgram->typeMetadata.size());
  for (const auto &entry : semanticProgram->typeMetadata) {
    if (!entry.fullPath.empty()) {
      adapter.typeMetadataByPath[entry.fullPath] = &entry;
      if (entry.category == "struct" || entry.category == "pod" || entry.category == "handle" ||
          entry.category == "gpu_lane") {
        adapter.orderedStructTypeMetadata.push_back(&entry);
      }
    }
  }

  adapter.structFieldMetadataByStructPath.reserve(semanticProgram->structFieldMetadata.size());
  for (const auto &entry : semanticProgram->structFieldMetadata) {
    if (!entry.structPath.empty()) {
      adapter.structFieldMetadataByStructPath[entry.structPath].push_back(&entry);
    }
  }
  for (auto &[structPath, entries] : adapter.structFieldMetadataByStructPath) {
    (void)structPath;
    std::stable_sort(entries.begin(),
                     entries.end(),
                     [](const SemanticProgramStructFieldMetadata *left,
                        const SemanticProgramStructFieldMetadata *right) {
                       if (left->fieldIndex != right->fieldIndex) {
                         return left->fieldIndex < right->fieldIndex;
                       }
                       return left->fieldName < right->fieldName;
                     });
  }

  return adapter;
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  if (adapter.semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto pathId = lookupDirectCallTargetId(adapter.semanticIndex, expr.semanticNodeId);
      pathId.has_value() && *pathId != InvalidSymbolId) {
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*adapter.semanticProgram, *pathId);
    if (!resolvedPath.empty()) {
      return std::string(resolvedPath);
    }
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  if (expr.semanticNodeId == 0 || adapter.semanticProgram == nullptr) {
    return {};
  }
  if (const auto pathId = lookupMethodCallTargetId(adapter.semanticIndex, expr.semanticNodeId);
      pathId.has_value()) {
    if (*pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*adapter.semanticProgram, *pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
  }
  return {};
}

std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  if (adapter.semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto pathId = lookupBridgePathChoiceId(adapter.semanticIndex, expr.semanticNodeId);
      pathId.has_value() && *pathId != InvalidSymbolId) {
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(*adapter.semanticProgram, *pathId);
    if (!chosenPath.empty()) {
      return std::string(chosenPath);
    }
  }
  return {};
}

const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProductTargetAdapter &adapter,
                                                                        const std::string &fullPath) {
  if (fullPath.empty() || adapter.semanticProgram == nullptr) {
    return nullptr;
  }
  const auto fullPathId = semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, fullPath);
  if (!fullPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = adapter.callableSummariesByPathId.find(*fullPathId);
      it != adapter.callableSummariesByPathId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition) {
  if (const auto *fact = findDefinitionScopedSemanticFact(adapter.semanticIndex.onErrorFactsByDefinitionId, definition);
      fact != nullptr) {
    return fact;
  }
  if (adapter.semanticProgram == nullptr || definition.fullPath.empty()) {
    return nullptr;
  }
  const auto definitionPathId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, definition.fullPath);
  if (!definitionPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = adapter.semanticIndex.onErrorFactsByDefinitionPathId.find(*definitionPathId);
      it != adapter.semanticIndex.onErrorFactsByDefinitionPathId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(const SemanticProductTargetAdapter &adapter,
                                                                  const std::string &fullPath) {
  if (fullPath.empty()) {
    return nullptr;
  }
  if (const auto it = adapter.typeMetadataByPath.find(fullPath); it != adapter.typeMetadataByPath.end()) {
    return it->second;
  }
  return nullptr;
}

const std::vector<const SemanticProgramStructFieldMetadata *> *findSemanticProductStructFieldMetadata(
    const SemanticProductTargetAdapter &adapter,
    const std::string &structPath) {
  if (structPath.empty()) {
    return nullptr;
  }
  if (const auto it = adapter.structFieldMetadataByStructPath.find(structPath);
      it != adapter.structFieldMetadataByStructPath.end()) {
    return &it->second;
  }
  return nullptr;
}

const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const Definition &definition) {
  if (const auto *fact = findDefinitionScopedSemanticFact(adapter.semanticIndex.returnFactsByDefinitionId,
                                                          definition);
      fact != nullptr) {
    return fact;
  }
  if (adapter.semanticProgram == nullptr || definition.fullPath.empty()) {
    return nullptr;
  }
  const auto definitionPathId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, definition.fullPath);
  if (!definitionPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = adapter.semanticIndex.returnFactsByDefinitionPathId.find(*definitionPathId);
      it != adapter.semanticIndex.returnFactsByDefinitionPathId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.semanticIndex.localAutoFactsByExpr, expr);
      fact != nullptr) {
    return fact;
  }
  if (adapter.semanticProgram == nullptr || !expr.isBinding || expr.args.size() != 1 ||
      expr.name.empty()) {
    return nullptr;
  }
  const auto bindingNameId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, expr.name);
  if (!bindingNameId.has_value()) {
    return nullptr;
  }
  const auto initializerPathId = resolveLocalAutoInitializerPathId(adapter, expr);
  if (!initializerPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = adapter.semanticIndex.localAutoFactsByInitPathAndBindingNameId.find(
          makeLocalAutoInitPathBindingNameKey(*initializerPathId, *bindingNameId));
      it != adapter.semanticIndex.localAutoFactsByInitPathAndBindingNameId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.semanticIndex.queryFactsByExpr, expr);
      fact != nullptr) {
    return fact;
  }
  if (adapter.semanticProgram == nullptr || expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return nullptr;
  }
  const auto callNameId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, expr.name);
  if (!callNameId.has_value()) {
    return nullptr;
  }
  const auto resolvedPathId = resolveSemanticExprPathId(adapter, expr);
  if (!resolvedPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = adapter.semanticIndex.queryFactsByResolvedPathAndCallNameId.find(
          makeQueryFactResolvedPathCallNameKey(*resolvedPathId, *callNameId));
      it != adapter.semanticIndex.queryFactsByResolvedPathAndCallNameId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.semanticIndex.tryFactsByExpr, expr);
      fact != nullptr) {
    return fact;
  }
  if (adapter.semanticProgram == nullptr || expr.kind != Expr::Kind::Call || expr.name != "try" ||
      expr.args.empty() || expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return nullptr;
  }
  const auto operandPathId = resolveSemanticExprPathId(adapter, expr.args.front());
  if (!operandPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = adapter.semanticIndex.tryFactsByOperandPathAndSource.find(
          makeTryFactOperandPathSourceKey(*operandPathId, expr.sourceLine, expr.sourceColumn));
      it != adapter.semanticIndex.tryFactsByOperandPathAndSource.end()) {
    return it->second;
  }
  const auto tryFacts = semanticProgramTryFactView(*adapter.semanticProgram);
  for (const auto *entry : tryFacts) {
    if (entry->operandResolvedPathId == *operandPathId &&
        entry->sourceLine == expr.sourceLine &&
        entry->sourceColumn == expr.sourceColumn) {
      return entry;
    }
  }
  return nullptr;
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.semanticIndex.bindingFactsByExpr, expr);
      fact != nullptr) {
    return fact;
  }
  return nullptr;
}

} // namespace primec::ir_lowerer

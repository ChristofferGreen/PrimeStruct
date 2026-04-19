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

std::optional<SymbolId> resolveSemanticExprPathId(const SemanticProgram *semanticProgram,
                                                  const Expr &expr) {
  if (semanticProgram == nullptr) {
    return std::nullopt;
  }

  auto lookupPathId = [&](const std::string &resolvedPath) -> std::optional<SymbolId> {
    if (resolvedPath.empty()) {
      return std::nullopt;
    }
    return semanticProgramLookupCallTargetStringId(*semanticProgram, resolvedPath);
  };

  if (const auto directPathId =
          lookupPathId(findSemanticProductDirectCallTarget(semanticProgram, expr));
      directPathId.has_value()) {
    return directPathId;
  }
  if (const auto methodPathId =
          lookupPathId(findSemanticProductMethodCallTarget(semanticProgram, expr));
      methodPathId.has_value()) {
    return methodPathId;
  }
  if (const auto bridgePathId =
          lookupPathId(findSemanticProductBridgePathChoice(semanticProgram, expr));
      bridgePathId.has_value()) {
    return bridgePathId;
  }
  if (!expr.name.empty() && expr.name.front() == '/') {
    return lookupPathId(expr.name);
  }
  return std::nullopt;
}

std::optional<SymbolId> resolveLocalAutoInitializerPathId(const SemanticProgram *semanticProgram,
                                                          const Expr &bindingExpr) {
  if (bindingExpr.args.empty()) {
    return std::nullopt;
  }

  const Expr &initializerExpr = bindingExpr.args.front();
  if (const auto initializerPathId = resolveSemanticExprPathId(semanticProgram, initializerExpr);
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
    buildOnErrorIndex(index);
    buildReturnIndex(index);
    buildLocalAutoIndex(index);
    buildQueryIndex(index);
    buildTryIndex(index);
    buildBindingIndex(index);
    return index;
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
    index.tryFactsByOperandPathAndSource.reserve(tryFacts.size());
    for (const auto *entry : tryFacts) {
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
      if (entry->semanticNodeId != 0) {
        index.bindingFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
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
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto pathId = semanticProgramLookupPublishedDirectCallTargetId(*semanticProgram,
                                                                           expr.semanticNodeId);
      pathId.has_value() && *pathId != InvalidSymbolId) {
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, *pathId);
    if (!resolvedPath.empty()) {
      return std::string(resolvedPath);
    }
  }
  return {};
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductDirectCallTarget(adapter.semanticProgram, expr);
}

std::string findSemanticProductMethodCallTarget(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (expr.semanticNodeId == 0 || semanticProgram == nullptr) {
    return {};
  }
  if (const auto pathId = semanticProgramLookupPublishedMethodCallTargetId(*semanticProgram,
                                                                           expr.semanticNodeId);
      pathId.has_value()) {
    if (*pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, *pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductMethodCallTarget(adapter.semanticProgram, expr);
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
    const SemanticProductTargetAdapter &adapter,
    const Definition &definition) {
  return findDefinitionScopedSemanticFact(adapter.semanticIndex.onErrorFactsByDefinitionId, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition) {
  if (const auto *fact = findSemanticProductOnErrorFactBySemanticId(adapter, definition);
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
  if (const auto *fact = findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr);
      fact != nullptr) {
    return fact;
  }
  if (semanticProgram == nullptr || !expr.isBinding || expr.args.size() != 1 ||
      expr.name.empty()) {
    return nullptr;
  }
  const auto bindingNameId =
      semanticProgramLookupCallTargetStringId(*semanticProgram, expr.name);
  if (!bindingNameId.has_value()) {
    return nullptr;
  }
  const auto initializerPathId = resolveLocalAutoInitializerPathId(semanticProgram, expr);
  if (!initializerPathId.has_value()) {
    return nullptr;
  }
  if (const auto it = semanticIndex.localAutoFactsByInitPathAndBindingNameId.find(
          makeLocalAutoInitPathBindingNameKey(*initializerPathId, *bindingNameId));
      it != semanticIndex.localAutoFactsByInitPathAndBindingNameId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr) {
  return findSemanticProductLocalAutoFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.semanticIndex.queryFactsByExpr, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr) {
  if (const auto *fact = findSemanticProductQueryFactBySemanticId(adapter, expr);
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

const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.semanticIndex.tryFactsByExpr, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr) {
  if (const auto *fact = findSemanticProductTryFactBySemanticId(adapter, expr);
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
  return nullptr;
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

} // namespace primec::ir_lowerer

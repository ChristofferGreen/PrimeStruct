#include "IrLowererSemanticProductTargetAdapters.h"

#include <algorithm>

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

} // namespace

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;

  const auto directCallTargets = semanticProgramDirectCallTargetView(*semanticProgram);
  adapter.directCallTargetsByExpr.reserve(directCallTargets.size());
  for (const auto *entry : directCallTargets) {
    if (entry->semanticNodeId != 0 && !entry->resolvedPath.empty()) {
      adapter.directCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, entry->resolvedPath);
    }
  }

  const auto methodCallTargets = semanticProgramMethodCallTargetView(*semanticProgram);
  adapter.methodCallTargetsByExpr.reserve(methodCallTargets.size());
  for (const auto *entry : methodCallTargets) {
    if (entry->semanticNodeId != 0 && !entry->resolvedPath.empty()) {
      adapter.methodCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, entry->resolvedPath);
    }
  }

  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  adapter.bridgePathChoicesByExpr.reserve(bridgePathChoices.size());
  for (const auto *entry : bridgePathChoices) {
    if (entry->semanticNodeId != 0 && !entry->chosenPath.empty()) {
      adapter.bridgePathChoicesByExpr.insert_or_assign(entry->semanticNodeId, entry->chosenPath);
    }
  }

  const auto callableSummaries = semanticProgramCallableSummaryView(*semanticProgram);
  adapter.callableSummariesByPath.reserve(callableSummaries.size());
  for (const auto *entry : callableSummaries) {
    if (!entry->fullPath.empty()) {
      adapter.callableSummariesByPath[entry->fullPath] = entry;
    }
  }

  adapter.onErrorFactsByDefinitionId.reserve(semanticProgram->onErrorFacts.size());
  for (const auto &entry : semanticProgram->onErrorFacts) {
    if (entry.semanticNodeId != 0) {
      adapter.onErrorFactsByDefinitionId.insert_or_assign(entry.semanticNodeId, &entry);
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

  adapter.returnFactsByDefinitionId.reserve(semanticProgram->returnFacts.size());
  for (const auto &entry : semanticProgram->returnFacts) {
    if (entry.semanticNodeId != 0) {
      adapter.returnFactsByDefinitionId.insert_or_assign(entry.semanticNodeId, &entry);
    }
  }

  adapter.localAutoFactsByExpr.reserve(semanticProgram->localAutoFacts.size());
  for (const auto &entry : semanticProgram->localAutoFacts) {
    if (entry.semanticNodeId != 0) {
      adapter.localAutoFactsByExpr.insert_or_assign(entry.semanticNodeId, &entry);
    }
  }

  adapter.queryFactsByExpr.reserve(semanticProgram->queryFacts.size());
  for (const auto &entry : semanticProgram->queryFacts) {
    if (entry.semanticNodeId != 0) {
      adapter.queryFactsByExpr.insert_or_assign(entry.semanticNodeId, &entry);
    }
  }

  adapter.tryFactsByExpr.reserve(semanticProgram->tryFacts.size());
  for (const auto &entry : semanticProgram->tryFacts) {
    if (entry.semanticNodeId != 0) {
      adapter.tryFactsByExpr.insert_or_assign(entry.semanticNodeId, &entry);
    }
  }

  adapter.bindingFactsByExpr.reserve(semanticProgram->bindingFacts.size());
  for (const auto &entry : semanticProgram->bindingFacts) {
    if (entry.semanticNodeId != 0) {
      adapter.bindingFactsByExpr.insert_or_assign(entry.semanticNodeId, &entry);
    }
  }

  return adapter;
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  if (expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto it = adapter.directCallTargetsByExpr.find(expr.semanticNodeId);
      it != adapter.directCallTargetsByExpr.end()) {
    return it->second;
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  if (expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto it = adapter.methodCallTargetsByExpr.find(expr.semanticNodeId);
      it != adapter.methodCallTargetsByExpr.end()) {
    return it->second;
  }
  return {};
}

std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  if (expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto it = adapter.bridgePathChoicesByExpr.find(expr.semanticNodeId);
      it != adapter.bridgePathChoicesByExpr.end()) {
    return it->second;
  }
  return {};
}

const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProductTargetAdapter &adapter,
                                                                        const std::string &fullPath) {
  if (fullPath.empty()) {
    return nullptr;
  }
  if (const auto it = adapter.callableSummariesByPath.find(fullPath); it != adapter.callableSummariesByPath.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition) {
  return findDefinitionScopedSemanticFact(adapter.onErrorFactsByDefinitionId, definition);
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
  return findDefinitionScopedSemanticFact(adapter.returnFactsByDefinitionId, definition);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.localAutoFactsByExpr, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.queryFactsByExpr, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.tryFactsByExpr, expr);
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.bindingFactsByExpr, expr);
}

} // namespace primec::ir_lowerer

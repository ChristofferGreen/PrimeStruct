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

} // namespace

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;
  adapter.semanticProgram = semanticProgram;

  const auto directCallTargets = semanticProgramDirectCallTargetView(*semanticProgram);
  adapter.directCallTargetsByExpr.reserve(directCallTargets.size());
  for (const auto *entry : directCallTargets) {
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->resolvedPathId);
    if (entry->semanticNodeId != 0 && !resolvedPath.empty()) {
      adapter.directCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, std::string(resolvedPath));
    }
  }

  const auto methodCallTargets = semanticProgramMethodCallTargetView(*semanticProgram);
  adapter.methodCallTargetIdsByExpr.reserve(methodCallTargets.size());
  for (const auto *entry : methodCallTargets) {
    if (entry->semanticNodeId == 0 || entry->resolvedPathId == InvalidSymbolId) {
      continue;
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->resolvedPathId);
    if (!resolvedPath.empty()) {
      adapter.methodCallTargetIdsByExpr.insert_or_assign(entry->semanticNodeId, entry->resolvedPathId);
    }
  }

  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  adapter.bridgePathChoicesByExpr.reserve(bridgePathChoices.size());
  for (const auto *entry : bridgePathChoices) {
    const std::string_view helperName =
        semanticProgramBridgePathChoiceHelperName(*semanticProgram, *entry);
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->chosenPathId);
    if (entry->semanticNodeId != 0 &&
        entry->helperNameId != InvalidSymbolId &&
        !helperName.empty() &&
        !chosenPath.empty()) {
      adapter.bridgePathChoicesByExpr.insert_or_assign(entry->semanticNodeId, std::string(chosenPath));
    }
  }

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

  const auto onErrorFacts = semanticProgramOnErrorFactView(*semanticProgram);
  adapter.onErrorFactsByDefinitionId.reserve(onErrorFacts.size());
  adapter.onErrorFactsByDefinitionPathId.reserve(onErrorFacts.size());
  for (const auto *entry : onErrorFacts) {
    if (entry->semanticNodeId != 0) {
      adapter.onErrorFactsByDefinitionId.insert_or_assign(entry->semanticNodeId, entry);
    }
    const std::string_view definitionPath =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->definitionPathId);
    if (entry->definitionPathId != InvalidSymbolId && !definitionPath.empty()) {
      adapter.onErrorFactsByDefinitionPathId.insert_or_assign(entry->definitionPathId, entry);
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

  const auto returnFacts = semanticProgramReturnFactView(*semanticProgram);
  adapter.returnFactsByDefinitionId.reserve(returnFacts.size());
  adapter.returnFactsByDefinitionPathId.reserve(returnFacts.size());
  for (const auto *entry : returnFacts) {
    if (entry->semanticNodeId != 0) {
      adapter.returnFactsByDefinitionId.insert_or_assign(entry->semanticNodeId, entry);
    }
    const std::string_view definitionPath =
        semanticProgramReturnFactDefinitionPath(*semanticProgram, *entry);
    if (entry->definitionPathId != InvalidSymbolId && !definitionPath.empty()) {
      adapter.returnFactsByDefinitionPathId.insert_or_assign(entry->definitionPathId, entry);
    }
  }

  const auto localAutoFacts = semanticProgramLocalAutoFactView(*semanticProgram);
  adapter.localAutoFactsByExpr.reserve(localAutoFacts.size());
  adapter.localAutoFactsByInitPathAndBindingNameId.reserve(localAutoFacts.size());
  for (const auto *entry : localAutoFacts) {
    if (entry->semanticNodeId != 0) {
      adapter.localAutoFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
    const std::string_view initializerResolvedPath =
        semanticProgramLocalAutoFactInitializerResolvedPath(*semanticProgram, *entry);
    const std::string_view bindingName =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->bindingNameId);
    if (entry->initializerResolvedPathId != InvalidSymbolId &&
        !initializerResolvedPath.empty() &&
        entry->bindingNameId != InvalidSymbolId &&
        !bindingName.empty()) {
      adapter.localAutoFactsByInitPathAndBindingNameId.insert_or_assign(
          makeLocalAutoInitPathBindingNameKey(entry->initializerResolvedPathId, entry->bindingNameId),
          entry);
    }
  }

  const auto queryFacts = semanticProgramQueryFactView(*semanticProgram);
  adapter.queryFactsByExpr.reserve(queryFacts.size());
  adapter.queryFactsByResolvedPathAndCallNameId.reserve(queryFacts.size());
  for (const auto *entry : queryFacts) {
    if (entry->semanticNodeId != 0) {
      adapter.queryFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
    const std::string_view resolvedPath =
        semanticProgramQueryFactResolvedPath(*semanticProgram, *entry);
    const std::string_view callName =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->callNameId);
    if (entry->resolvedPathId != InvalidSymbolId &&
        !resolvedPath.empty() &&
        entry->callNameId != InvalidSymbolId &&
        !callName.empty()) {
      adapter.queryFactsByResolvedPathAndCallNameId.insert_or_assign(
          makeQueryFactResolvedPathCallNameKey(entry->resolvedPathId, entry->callNameId),
          entry);
    }
  }

  const auto tryFacts = semanticProgramTryFactView(*semanticProgram);
  adapter.tryFactsByExpr.reserve(tryFacts.size());
  adapter.tryFactsByOperandPathAndSource.reserve(tryFacts.size());
  for (const auto *entry : tryFacts) {
    if (entry->semanticNodeId != 0) {
      adapter.tryFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
    const std::string_view operandResolvedPath =
        semanticProgramTryFactOperandResolvedPath(*semanticProgram, *entry);
    if (entry->operandResolvedPathId != InvalidSymbolId &&
        !operandResolvedPath.empty() &&
        entry->sourceLine > 0 &&
        entry->sourceColumn > 0) {
      adapter.tryFactsByOperandPathAndSource.insert_or_assign(
          makeTryFactOperandPathSourceKey(entry->operandResolvedPathId,
                                          entry->sourceLine,
                                          entry->sourceColumn),
          entry);
    }
  }

  const auto bindingFacts = semanticProgramBindingFactView(*semanticProgram);
  adapter.bindingFactsByExpr.reserve(bindingFacts.size());
  for (const auto *entry : bindingFacts) {
    if (entry->semanticNodeId != 0) {
      adapter.bindingFactsByExpr.insert_or_assign(entry->semanticNodeId, entry);
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
  if (expr.semanticNodeId == 0 || adapter.semanticProgram == nullptr) {
    return {};
  }
  if (const auto it = adapter.methodCallTargetIdsByExpr.find(expr.semanticNodeId);
      it != adapter.methodCallTargetIdsByExpr.end()) {
    const SymbolId pathId = it->second;
    if (pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*adapter.semanticProgram, pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
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
  if (const auto *fact = findDefinitionScopedSemanticFact(adapter.onErrorFactsByDefinitionId, definition);
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
  if (const auto it = adapter.onErrorFactsByDefinitionPathId.find(*definitionPathId);
      it != adapter.onErrorFactsByDefinitionPathId.end()) {
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
  if (const auto *fact = findDefinitionScopedSemanticFact(adapter.returnFactsByDefinitionId, definition);
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
  if (const auto it = adapter.returnFactsByDefinitionPathId.find(*definitionPathId);
      it != adapter.returnFactsByDefinitionPathId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.localAutoFactsByExpr, expr);
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
  if (const auto it = adapter.localAutoFactsByInitPathAndBindingNameId.find(
          makeLocalAutoInitPathBindingNameKey(*initializerPathId, *bindingNameId));
      it != adapter.localAutoFactsByInitPathAndBindingNameId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.queryFactsByExpr, expr);
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
  if (const auto it = adapter.queryFactsByResolvedPathAndCallNameId.find(
          makeQueryFactResolvedPathCallNameKey(*resolvedPathId, *callNameId));
      it != adapter.queryFactsByResolvedPathAndCallNameId.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(adapter.tryFactsByExpr, expr);
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
  if (const auto it = adapter.tryFactsByOperandPathAndSource.find(
          makeTryFactOperandPathSourceKey(*operandPathId, expr.sourceLine, expr.sourceColumn));
      it != adapter.tryFactsByOperandPathAndSource.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr) {
  return findExpressionScopedSemanticFact(adapter.bindingFactsByExpr, expr);
}

} // namespace primec::ir_lowerer

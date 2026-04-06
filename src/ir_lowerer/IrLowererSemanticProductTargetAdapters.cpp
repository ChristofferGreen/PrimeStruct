#include "IrLowererSemanticProductTargetAdapters.h"

#include <algorithm>

namespace primec::ir_lowerer {

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;

  adapter.directCallTargetsByExpr.reserve(semanticProgram->directCallTargets.size());
  for (const auto &entry : semanticProgram->directCallTargets) {
    if (entry.semanticNodeId != 0 && !entry.resolvedPath.empty()) {
      adapter.directCallTargetsByExpr.insert_or_assign(entry.semanticNodeId, entry.resolvedPath);
    }
  }

  adapter.methodCallTargetsByExpr.reserve(semanticProgram->methodCallTargets.size());
  for (const auto &entry : semanticProgram->methodCallTargets) {
    if (entry.semanticNodeId != 0 && !entry.resolvedPath.empty()) {
      adapter.methodCallTargetsByExpr.insert_or_assign(entry.semanticNodeId, entry.resolvedPath);
    }
  }

  adapter.bridgePathChoicesByExpr.reserve(semanticProgram->bridgePathChoices.size());
  for (const auto &entry : semanticProgram->bridgePathChoices) {
    if (entry.semanticNodeId != 0 && !entry.chosenPath.empty()) {
      adapter.bridgePathChoicesByExpr.insert_or_assign(entry.semanticNodeId, entry.chosenPath);
    }
  }

  adapter.callableSummariesByPath.reserve(semanticProgram->callableSummaries.size());
  for (const auto &entry : semanticProgram->callableSummaries) {
    if (!entry.fullPath.empty()) {
      adapter.callableSummariesByPath[entry.fullPath] = &entry;
    }
  }

  adapter.onErrorFactsByDefinitionPath.reserve(semanticProgram->onErrorFacts.size());
  for (const auto &entry : semanticProgram->onErrorFacts) {
    if (!entry.definitionPath.empty()) {
      adapter.onErrorFactsByDefinitionPath[entry.definitionPath] = &entry;
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

  adapter.returnFactsByDefinitionPath.reserve(semanticProgram->returnFacts.size());
  for (const auto &entry : semanticProgram->returnFacts) {
    if (!entry.definitionPath.empty()) {
      adapter.returnFactsByDefinitionPath[entry.definitionPath] = &entry;
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
                                                                const std::string &definitionPath) {
  if (definitionPath.empty()) {
    return nullptr;
  }
  if (const auto it = adapter.onErrorFactsByDefinitionPath.find(definitionPath);
      it != adapter.onErrorFactsByDefinitionPath.end()) {
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
                                                              const std::string &definitionPath) {
  if (definitionPath.empty()) {
    return nullptr;
  }
  if (const auto it = adapter.returnFactsByDefinitionPath.find(definitionPath);
      it != adapter.returnFactsByDefinitionPath.end()) {
    return it->second;
  }
  return nullptr;
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr) {
  if (expr.semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = adapter.bindingFactsByExpr.find(expr.semanticNodeId);
      it != adapter.bindingFactsByExpr.end()) {
    return it->second;
  }
  return nullptr;
}

} // namespace primec::ir_lowerer

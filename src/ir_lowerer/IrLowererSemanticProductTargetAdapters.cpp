#include "IrLowererSemanticProductTargetAdapters.h"

#include <algorithm>
#include <string_view>

namespace primec::ir_lowerer {
namespace {

std::string makeTargetLookupKey(int sourceLine, int sourceColumn, std::string_view name) {
  if (sourceLine <= 0 || sourceColumn <= 0 || name.empty()) {
    return {};
  }
  return std::to_string(sourceLine) + ":" + std::to_string(sourceColumn) + ":" + std::string(name);
}

std::string makeTargetLookupKey(const Expr &expr, std::string_view name) {
  return makeTargetLookupKey(expr.sourceLine, expr.sourceColumn, name);
}

template <typename Value>
void insertSemanticExprLookup(std::unordered_map<std::string, Value> &valuesByKey,
                              std::string key,
                              Value value) {
  if (key.empty()) {
    return;
  }
  const auto it = valuesByKey.find(key);
  if (it == valuesByKey.end()) {
    valuesByKey.emplace(std::move(key), std::move(value));
    return;
  }
  if (it->second == value) {
    return;
  }
  it->second = Value{};
}

std::string normalizedSemanticProductBridgeHelperName(const Expr &expr) {
  auto stripTemplateSuffix = [](std::string &helperName) {
    const size_t specializationSuffix = helperName.find("__t");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
  };

  auto consumePrefixedHelper = [&](std::string_view prefix) -> std::string {
    if (expr.name.rfind(prefix, 0) != 0) {
      return {};
    }
    std::string helperName = expr.name.substr(prefix.size());
    stripTemplateSuffix(helperName);
    return helperName;
  };

  if (std::string helperName = consumePrefixedHelper("/vector/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/std/collections/vector/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/map/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/std/collections/map/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/soa_vector/"); !helperName.empty()) {
    return helperName;
  }
  if (std::string helperName = consumePrefixedHelper("/std/collections/soa_vector/"); !helperName.empty()) {
    return helperName;
  }
  return {};
}

} // namespace

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;

  adapter.directCallTargetsByExpr.reserve(semanticProgram->directCallTargets.size());
  for (const auto &entry : semanticProgram->directCallTargets) {
    insertSemanticExprLookup(adapter.directCallTargetsByExpr,
                             makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.callName),
                             entry.resolvedPath);
  }

  adapter.methodCallTargetsByExpr.reserve(semanticProgram->methodCallTargets.size());
  for (const auto &entry : semanticProgram->methodCallTargets) {
    insertSemanticExprLookup(adapter.methodCallTargetsByExpr,
                             makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.methodName),
                             entry.resolvedPath);
  }

  adapter.bridgePathChoicesByExpr.reserve(semanticProgram->bridgePathChoices.size());
  for (const auto &entry : semanticProgram->bridgePathChoices) {
    insertSemanticExprLookup(adapter.bridgePathChoicesByExpr,
                             makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.helperName),
                             entry.chosenPath);
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
    insertSemanticExprLookup(adapter.bindingFactsByExpr,
                             makeTargetLookupKey(entry.sourceLine, entry.sourceColumn, entry.name),
                             &entry);
  }

  return adapter;
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  const std::string key = makeTargetLookupKey(expr, expr.name);
  if (key.empty()) {
    return {};
  }
  if (const auto it = adapter.directCallTargetsByExpr.find(key); it != adapter.directCallTargetsByExpr.end()) {
    return it->second;
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  const std::string key = makeTargetLookupKey(expr, expr.name);
  if (key.empty()) {
    return {};
  }
  if (const auto it = adapter.methodCallTargetsByExpr.find(key); it != adapter.methodCallTargetsByExpr.end()) {
    return it->second;
  }
  return {};
}

std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  const std::string helperName = normalizedSemanticProductBridgeHelperName(expr);
  if (helperName.empty()) {
    return {};
  }
  const std::string key = makeTargetLookupKey(expr, helperName);
  if (key.empty()) {
    return {};
  }
  if (const auto it = adapter.bridgePathChoicesByExpr.find(key); it != adapter.bridgePathChoicesByExpr.end()) {
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
  const std::string key = makeTargetLookupKey(expr, expr.name);
  if (key.empty()) {
    return nullptr;
  }
  if (const auto it = adapter.bindingFactsByExpr.find(key); it != adapter.bindingFactsByExpr.end()) {
    return it->second;
  }
  return nullptr;
}

} // namespace primec::ir_lowerer

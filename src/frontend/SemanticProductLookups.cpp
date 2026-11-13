#include "primec/frontend/SemanticProduct.h"

#include "primec/support/CompileArena.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include "SemanticProductInternal.h"

namespace primec {
using namespace semantic_product_detail;

namespace {

// The fact `key` names in `index`, or null when `index` has no entry for it.
template <typename Key, typename Entry>
const Entry *lookupFactByKey(const std::unordered_map<Key, std::size_t> &index,
                             const std::vector<Entry> &entries,
                             std::type_identity_t<Key> key) {
  const auto it = index.find(key);
  return it == index.end() ? nullptr : lookupPublishedSemanticEntryByIndex(entries, it->second);
}

} // namespace

const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  return lookupFactByKey(semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId,
                         semanticProgram.onErrorFacts,
                         semanticNodeId);
}

const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(
    const SemanticProgram &semanticProgram,
    SymbolId definitionPathId) {
  if (definitionPathId == InvalidSymbolId) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId,
      semanticProgram.onErrorFacts,
      definitionPathId);
}

const SemanticProgramReturnFact *semanticProgramLookupPublishedReturnFactByDefinitionPathId(
    const SemanticProgram &semanticProgram,
    SymbolId definitionPathId) {
  if (definitionPathId == InvalidSymbolId) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId,
      semanticProgram.returnFacts,
      definitionPathId);
}

const SemanticProgramSumTypeMetadata *semanticProgramLookupPublishedSumTypeMetadataByPathId(
    const SemanticProgram &semanticProgram,
    SymbolId fullPathId) {
  if (fullPathId == InvalidSymbolId) {
    return nullptr;
  }
  return lookupFactByKey(semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId,
                         semanticProgram.sumTypeMetadata,
                         fullPathId);
}

const SemanticProgramSumVariantMetadata *
semanticProgramLookupPublishedSumVariantMetadataBySumPathAndVariantNameId(
    const SemanticProgram &semanticProgram, SymbolId sumPathId, SymbolId variantNameId) {
  if (sumPathId == InvalidSymbolId || variantNameId == InvalidSymbolId) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.sumVariantMetadataIndicesBySumPathAndVariantNameId,
      semanticProgram.sumVariantMetadata,
      makeSumVariantMetadataSumPathVariantNameKey(sumPathId, variantNameId));
}

const SemanticProgramCollectionSpecialization *
semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
    const SemanticProgram &semanticProgram, uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr,
      semanticProgram.collectionSpecializations,
      semanticNodeId);
}

const SemanticProgramArrayExtentFact *
semanticProgramLookupPublishedArrayExtentFactBySemanticId(const SemanticProgram &semanticProgram,
                                                          uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  return lookupFactByKey(semanticProgram.publishedRoutingLookups.arrayExtentFactIndicesByExpr,
                         semanticProgram.arrayExtentFacts,
                         semanticNodeId);
}

const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  return lookupFactByKey(semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr,
                         semanticProgram.localAutoFacts,
                         semanticNodeId);
}

const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
    const SemanticProgram &semanticProgram,
    SymbolId initializerPathId,
    SymbolId bindingNameId) {
  if (initializerPathId == InvalidSymbolId || bindingNameId == InvalidSymbolId) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId,
      semanticProgram.localAutoFacts,
      makeLocalAutoInitPathBindingNameKey(initializerPathId, bindingNameId));
}

const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  return lookupFactByKey(semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr,
                         semanticProgram.queryFacts,
                         semanticNodeId);
}

const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
    const SemanticProgram &semanticProgram,
    SymbolId resolvedPathId,
    SymbolId callNameId) {
  if (resolvedPathId == InvalidSymbolId || callNameId == InvalidSymbolId) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId,
      semanticProgram.queryFacts,
      makeQueryFactResolvedPathCallNameKey(resolvedPathId, callNameId));
}

const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  return lookupFactByKey(semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr,
                         semanticProgram.tryFacts,
                         semanticNodeId);
}

const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactByOperandPathAndSource(
    const SemanticProgram &semanticProgram,
    SymbolId operandPathId,
    int sourceLine,
    int sourceColumn) {
  if (operandPathId == InvalidSymbolId || sourceLine <= 0 || sourceColumn <= 0) {
    return nullptr;
  }
  return lookupFactByKey(
      semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource,
      semanticProgram.tryFacts,
      makeTryFactOperandPathSourceKey(operandPathId, sourceLine, sourceColumn));
}

const SemanticProgramTypeMetadata *semanticProgramLookupTypeMetadata(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath) {
  if (fullPath.empty()) {
    return nullptr;
  }
  for (const auto &entry : semanticProgram.typeMetadata) {
    if (entry.fullPath == fullPath) {
      return &entry;
    }
  }
  return nullptr;
}

std::vector<const SemanticProgramTypeMetadata *>
semanticProgramStructTypeMetadataView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramTypeMetadata *> view;
  view.reserve(semanticProgram.typeMetadata.size());
  for (const auto &entry : semanticProgram.typeMetadata) {
    if (entry.category == "struct" || entry.category == "pod" || entry.category == "handle" ||
        entry.category == "gpu_lane") {
      view.push_back(&entry);
    }
  }
  return view;
}

std::vector<const SemanticProgramStructFieldMetadata *>
semanticProgramStructFieldMetadataView(const SemanticProgram &semanticProgram,
                                       std::string_view structPath) {
  std::vector<const SemanticProgramStructFieldMetadata *> view;
  if (structPath.empty()) {
    return view;
  }
  for (const auto &entry : semanticProgram.structFieldMetadata) {
    if (entry.structPath == structPath) {
      view.push_back(&entry);
    }
  }
  std::stable_sort(view.begin(),
                   view.end(),
                   [](const SemanticProgramStructFieldMetadata *left,
                      const SemanticProgramStructFieldMetadata *right) {
                     if (left->fieldIndex != right->fieldIndex) {
                       return left->fieldIndex < right->fieldIndex;
                     }
                     return left->fieldName < right->fieldName;
                   });
  return view;
}

std::string_view semanticProgramDirectCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramDirectCallTarget &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramMethodCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramMethodCallTarget &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramBridgePathChoiceHelperName(
    const SemanticProgram &semanticProgram,
    const SemanticProgramBridgePathChoice &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.helperNameId);
}

std::string_view semanticProgramCallableSummaryFullPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramCallableSummary &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.fullPathId);
}

std::string_view semanticProgramBindingFactResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramBindingFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramReturnFactDefinitionPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramReturnFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
}

std::string_view semanticProgramArrayExtentFactTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramArrayExtentFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.targetResolvedPathId);
}

std::string_view semanticProgramLocalAutoFactInitializerResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramLocalAutoFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.initializerResolvedPathId);
}

std::string_view semanticProgramQueryFactResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramQueryFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramTryFactOperandResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramTryFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.operandResolvedPathId);
}

std::string_view semanticProgramOnErrorFactDefinitionPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramOnErrorFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
}

std::string_view semanticProgramOnErrorFactHandlerPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramOnErrorFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.handlerPathId);
}

std::string formatSemanticStringListFromIds(const SemanticProgram &semanticProgram,
                                            const std::vector<SymbolId> &ids,
                                            const std::vector<std::string> &fallbackValues) {
  if (ids.empty()) {
    return formatSemanticStringList(fallbackValues);
  }
  std::vector<std::string> resolvedValues;
  resolvedValues.reserve(ids.size());
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, ids[i]);
    if (!resolved.empty()) {
      resolvedValues.emplace_back(resolved);
    } else if (i < fallbackValues.size()) {
      resolvedValues.push_back(fallbackValues[i]);
    } else {
      resolvedValues.emplace_back();
    }
  }
  return formatSemanticStringList(resolvedValues);
}

namespace {

using ModuleArtifacts = SemanticProgramModuleResolvedArtifacts;

// The entries of one fact kind in module order: each module's entries from its index list, or all
// of them in published order when no module lists any.
template <typename Entry>
std::vector<const Entry *>
moduleFactView(const SemanticProgram &semanticProgram,
               const std::vector<Entry> SemanticProgram::*factsMember,
               const std::vector<std::size_t> ModuleArtifacts::*indicesMember) {
  const std::vector<Entry> &facts = semanticProgram.*factsMember;
  std::vector<const Entry *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += (module.*indicesMember).size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.*indicesMember) {
        if (entryIndex < facts.size()) {
          entries.push_back(&facts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || facts.empty()) {
      return entries;
    }
  }

  entries.reserve(facts.size());
  for (const auto &entry : facts) {
    entries.push_back(&entry);
  }
  return entries;
}

} // namespace

std::vector<const SemanticProgramDirectCallTarget *>
semanticProgramDirectCallTargetView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::directCallTargets,
                        &ModuleArtifacts::directCallTargetIndices);
}

std::vector<const SemanticProgramMethodCallTarget *>
semanticProgramMethodCallTargetView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::methodCallTargets,
                        &ModuleArtifacts::methodCallTargetIndices);
}

std::vector<const SemanticProgramBridgePathChoice *>
semanticProgramBridgePathChoiceView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::bridgePathChoices,
                        &ModuleArtifacts::bridgePathChoiceIndices);
}

std::vector<const SemanticProgramCallableSummary *>
semanticProgramCallableSummaryView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::callableSummaries,
                        &ModuleArtifacts::callableSummaryIndices);
}

std::vector<const SemanticProgramBindingFact *>
semanticProgramBindingFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(
      semanticProgram, &SemanticProgram::bindingFacts, &ModuleArtifacts::bindingFactIndices);
}

std::vector<const SemanticProgramCollectionSpecialization *>
semanticProgramCollectionSpecializationView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::collectionSpecializations,
                        &ModuleArtifacts::collectionSpecializationIndices);
}

std::vector<const SemanticProgramArrayExtentFact *>
semanticProgramArrayExtentFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::arrayExtentFacts,
                        &ModuleArtifacts::arrayExtentFactIndices);
}

std::vector<const SemanticProgramReturnFact *>
semanticProgramReturnFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(
      semanticProgram, &SemanticProgram::returnFacts, &ModuleArtifacts::returnFactIndices);
}

std::vector<const SemanticProgramLocalAutoFact *>
semanticProgramLocalAutoFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(
      semanticProgram, &SemanticProgram::localAutoFacts, &ModuleArtifacts::localAutoFactIndices);
}

std::vector<const SemanticProgramQueryFact *>
semanticProgramQueryFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(
      semanticProgram, &SemanticProgram::queryFacts, &ModuleArtifacts::queryFactIndices);
}

std::vector<const SemanticProgramTryFact *>
semanticProgramTryFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(
      semanticProgram, &SemanticProgram::tryFacts, &ModuleArtifacts::tryFactIndices);
}

std::vector<const SemanticProgramOnErrorFact *>
semanticProgramOnErrorFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(
      semanticProgram, &SemanticProgram::onErrorFacts, &ModuleArtifacts::onErrorFactIndices);
}

std::vector<const SemanticProgramRequirementPredicateFact *>
semanticProgramRequirementPredicateFactView(const SemanticProgram &semanticProgram) {
  return moduleFactView(semanticProgram,
                        &SemanticProgram::requirementPredicateFacts,
                        &ModuleArtifacts::requirementPredicateFactIndices);
}

} // namespace primec

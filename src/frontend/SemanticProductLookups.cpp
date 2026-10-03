#include "primec/frontend/SemanticProduct.h"

#include "primec/support/CompileArena.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>
#include "SemanticProductInternal.h"

namespace primec {
using namespace semantic_product_detail;

const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.onErrorFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(
    const SemanticProgram &semanticProgram,
    SymbolId definitionPathId) {
  if (definitionPathId == InvalidSymbolId) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.find(
              definitionPathId);
      it != semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.onErrorFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramReturnFact *semanticProgramLookupPublishedReturnFactByDefinitionPathId(
    const SemanticProgram &semanticProgram,
    SymbolId definitionPathId) {
  if (definitionPathId == InvalidSymbolId) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId.find(
              definitionPathId);
      it != semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.returnFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramSumTypeMetadata *semanticProgramLookupPublishedSumTypeMetadataByPathId(
    const SemanticProgram &semanticProgram,
    SymbolId fullPathId) {
  if (fullPathId == InvalidSymbolId) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId.find(
              fullPathId);
      it != semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.sumTypeMetadata, it->second);
  }
  return nullptr;
}

const SemanticProgramSumVariantMetadata *
semanticProgramLookupPublishedSumVariantMetadataBySumPathAndVariantNameId(
    const SemanticProgram &semanticProgram,
    SymbolId sumPathId,
    SymbolId variantNameId) {
  if (sumPathId == InvalidSymbolId || variantNameId == InvalidSymbolId) {
    return nullptr;
  }
  const uint64_t compositeKey =
      makeSumVariantMetadataSumPathVariantNameKey(sumPathId, variantNameId);
  if (const auto it =
          semanticProgram.publishedRoutingLookups
              .sumVariantMetadataIndicesBySumPathAndVariantNameId.find(compositeKey);
      it != semanticProgram.publishedRoutingLookups
                .sumVariantMetadataIndicesBySumPathAndVariantNameId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.sumVariantMetadata,
                                               it->second);
  }
  return nullptr;
}

const SemanticProgramCollectionSpecialization *
semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.find(
              semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.collectionSpecializations,
                                               it->second);
  }
  return nullptr;
}

const SemanticProgramArrayExtentFact *
semanticProgramLookupPublishedArrayExtentFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.arrayExtentFactIndicesByExpr.find(
              semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.arrayExtentFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.arrayExtentFacts,
                                               it->second);
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.localAutoFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
    const SemanticProgram &semanticProgram,
    SymbolId initializerPathId,
    SymbolId bindingNameId) {
  if (initializerPathId == InvalidSymbolId || bindingNameId == InvalidSymbolId) {
    return nullptr;
  }
  const uint64_t compositeKey =
      makeLocalAutoInitPathBindingNameKey(initializerPathId, bindingNameId);
  if (const auto it =
          semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.find(
              compositeKey);
      it != semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.localAutoFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.queryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
    const SemanticProgram &semanticProgram,
    SymbolId resolvedPathId,
    SymbolId callNameId) {
  if (resolvedPathId == InvalidSymbolId || callNameId == InvalidSymbolId) {
    return nullptr;
  }
  const uint64_t compositeKey = makeQueryFactResolvedPathCallNameKey(resolvedPathId, callNameId);
  if (const auto it =
          semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.find(
              compositeKey);
      it != semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.queryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.tryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactByOperandPathAndSource(
    const SemanticProgram &semanticProgram,
    SymbolId operandPathId,
    int sourceLine,
    int sourceColumn) {
  if (operandPathId == InvalidSymbolId || sourceLine <= 0 || sourceColumn <= 0) {
    return nullptr;
  }
  const uint64_t compositeKey =
      makeTryFactOperandPathSourceKey(operandPathId, sourceLine, sourceColumn);
  if (const auto it =
          semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.find(
              compositeKey);
      it != semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.tryFacts, it->second);
  }
  return nullptr;
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

std::vector<const SemanticProgramDirectCallTarget *>
semanticProgramDirectCallTargetView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramDirectCallTarget *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.directCallTargetIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.directCallTargetIndices) {
        if (entryIndex < semanticProgram.directCallTargets.size()) {
          entries.push_back(&semanticProgram.directCallTargets[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.directCallTargets.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.directCallTargets.size());
  for (const auto &entry : semanticProgram.directCallTargets) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramMethodCallTarget *>
semanticProgramMethodCallTargetView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramMethodCallTarget *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.methodCallTargetIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.methodCallTargetIndices) {
        if (entryIndex < semanticProgram.methodCallTargets.size()) {
          entries.push_back(&semanticProgram.methodCallTargets[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.methodCallTargets.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.methodCallTargets.size());
  for (const auto &entry : semanticProgram.methodCallTargets) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramBridgePathChoice *>
semanticProgramBridgePathChoiceView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramBridgePathChoice *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.bridgePathChoiceIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.bridgePathChoiceIndices) {
        if (entryIndex < semanticProgram.bridgePathChoices.size()) {
          entries.push_back(&semanticProgram.bridgePathChoices[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.bridgePathChoices.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.bridgePathChoices.size());
  for (const auto &entry : semanticProgram.bridgePathChoices) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramCallableSummary *>
semanticProgramCallableSummaryView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramCallableSummary *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.callableSummaryIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.callableSummaryIndices) {
        if (entryIndex < semanticProgram.callableSummaries.size()) {
          entries.push_back(&semanticProgram.callableSummaries[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.callableSummaries.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.callableSummaries.size());
  for (const auto &entry : semanticProgram.callableSummaries) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramBindingFact *>
semanticProgramBindingFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramBindingFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.bindingFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.bindingFactIndices) {
        if (entryIndex < semanticProgram.bindingFacts.size()) {
          entries.push_back(&semanticProgram.bindingFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.bindingFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.bindingFacts.size());
  for (const auto &entry : semanticProgram.bindingFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramCollectionSpecialization *>
semanticProgramCollectionSpecializationView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramCollectionSpecialization *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.collectionSpecializationIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.collectionSpecializationIndices) {
        if (entryIndex < semanticProgram.collectionSpecializations.size()) {
          entries.push_back(&semanticProgram.collectionSpecializations[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.collectionSpecializations.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.collectionSpecializations.size());
  for (const auto &entry : semanticProgram.collectionSpecializations) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramArrayExtentFact *>
semanticProgramArrayExtentFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramArrayExtentFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.arrayExtentFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.arrayExtentFactIndices) {
        if (entryIndex < semanticProgram.arrayExtentFacts.size()) {
          entries.push_back(&semanticProgram.arrayExtentFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.arrayExtentFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.arrayExtentFacts.size());
  for (const auto &entry : semanticProgram.arrayExtentFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramReturnFact *>
semanticProgramReturnFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramReturnFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.returnFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.returnFactIndices) {
        if (entryIndex < semanticProgram.returnFacts.size()) {
          entries.push_back(&semanticProgram.returnFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.returnFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.returnFacts.size());
  for (const auto &entry : semanticProgram.returnFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramLocalAutoFact *>
semanticProgramLocalAutoFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramLocalAutoFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.localAutoFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.localAutoFactIndices) {
        if (entryIndex < semanticProgram.localAutoFacts.size()) {
          entries.push_back(&semanticProgram.localAutoFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.localAutoFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.localAutoFacts.size());
  for (const auto &entry : semanticProgram.localAutoFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramQueryFact *>
semanticProgramQueryFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramQueryFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.queryFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.queryFactIndices) {
        if (entryIndex < semanticProgram.queryFacts.size()) {
          entries.push_back(&semanticProgram.queryFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.queryFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.queryFacts.size());
  for (const auto &entry : semanticProgram.queryFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramTryFact *>
semanticProgramTryFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramTryFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.tryFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.tryFactIndices) {
        if (entryIndex < semanticProgram.tryFacts.size()) {
          entries.push_back(&semanticProgram.tryFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.tryFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.tryFacts.size());
  for (const auto &entry : semanticProgram.tryFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramOnErrorFact *>
semanticProgramOnErrorFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramOnErrorFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.onErrorFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.onErrorFactIndices) {
        if (entryIndex < semanticProgram.onErrorFacts.size()) {
          entries.push_back(&semanticProgram.onErrorFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.onErrorFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.onErrorFacts.size());
  for (const auto &entry : semanticProgram.onErrorFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

std::vector<const SemanticProgramRequirementPredicateFact *>
semanticProgramRequirementPredicateFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramRequirementPredicateFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.requirementPredicateFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : module.requirementPredicateFactIndices) {
        if (entryIndex < semanticProgram.requirementPredicateFacts.size()) {
          entries.push_back(&semanticProgram.requirementPredicateFacts[entryIndex]);
        }
      }
    }
    if (!entries.empty() || semanticProgram.requirementPredicateFacts.empty()) {
      return entries;
    }
  }

  entries.reserve(semanticProgram.requirementPredicateFacts.size());
  for (const auto &entry : semanticProgram.requirementPredicateFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

} // namespace primec

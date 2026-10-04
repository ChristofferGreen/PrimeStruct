#include "SemanticPublicationBuilders.h"

#include "RequirementPredicateFacts.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "SemanticPublicationBuildersInternal.h"

namespace primec {
namespace semantics {
namespace publicationBuilders {


void publishSumVariantMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SumVariantMetadataSnapshotEntry> &sumVariantMetadata) {
  if (sumVariantMetadata.empty()) {
    return;
  }
  state.semanticProgram.sumVariantMetadata.reserve(sumVariantMetadata.size());
  state.semanticProgram.publishedRoutingLookups
      .sumVariantMetadataIndicesBySumPathAndVariantNameId.reserve(
          state.semanticProgram.publishedRoutingLookups
              .sumVariantMetadataIndicesBySumPathAndVariantNameId.size() +
          sumVariantMetadata.size());
  for (const auto &entry : sumVariantMetadata) {
    state.semanticProgram.sumVariantMetadata.push_back(SemanticProgramSumVariantMetadata{
        entry.sumPath,
        entry.variantName,
        entry.variantIndex,
        entry.tagValue,
        entry.hasPayload,
        entry.payloadTypeText,
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
    const std::size_t entryIndex =
        state.semanticProgram.sumVariantMetadata.size() - 1;
    const SymbolId sumPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.sumPath);
    const SymbolId variantNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.variantName);
    if (sumPathId != InvalidSymbolId && variantNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups
          .sumVariantMetadataIndicesBySumPathAndVariantNameId.insert_or_assign(
              makeSumVariantMetadataSumPathVariantNameKey(sumPathId, variantNameId),
              entryIndex);
    }
  }
}

void publishSemanticMetadataFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface) {
  publishTypeMetadataFacts(state, publicationSurface.typeMetadata);
  publishStructFieldMetadataFacts(state, publicationSurface.structFieldMetadata);
  publishSumTypeMetadataFacts(state, publicationSurface.sumTypeMetadata);
  publishSumVariantMetadataFacts(state, publicationSurface.sumVariantMetadata);
}

void publishBindingFacts(
    SemanticPublicationBuilderState &state,
    std::vector<BindingFactSnapshotEntry> bindingFacts) {
  if (bindingFacts.empty()) {
    return;
  }
  state.semanticProgram.bindingFacts.reserve(bindingFacts.size());
  state.semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.size() +
      bindingFacts.size());
  state.semanticProgram.collectionSpecializations.reserve(
      state.semanticProgram.collectionSpecializations.size() + bindingFacts.size());
  state.semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.size() +
      bindingFacts.size());
  for (auto &snapshotEntry : bindingFacts) {
    SemanticProgramBindingFact entry;
    entry.scopePath = std::move(snapshotEntry.scopePath);
    entry.siteKind = std::move(snapshotEntry.siteKind);
    entry.name = std::move(snapshotEntry.name);
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.isMutable = snapshotEntry.binding.isMutable;
    entry.isEntryArgString = snapshotEntry.binding.isEntryArgString;
    entry.isUnsafeReference = snapshotEntry.binding.isUnsafeReference;
    entry.referenceRoot = std::move(snapshotEntry.binding.referenceRoot);
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.siteKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.siteKind);
    entry.nameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.name);
    const std::string resolvedPath =
        snapshotEntry.resolvedPath.empty()
            ? fallbackBindingResolvedPathForSemanticProduct(entry.scopePath, entry.name)
            : std::move(snapshotEntry.resolvedPath);
    entry.resolvedPathId = semanticProgramInternCallTargetString(state.semanticProgram, resolvedPath);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.referenceRootId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.referenceRoot);
    const std::string moduleScopePath =
        entry.scopePathId != InvalidSymbolId
            ? std::string(semanticProgramResolveCallTargetString(state.semanticProgram, entry.scopePathId))
            : entry.scopePath;
    auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);
    publishCollectionSpecializationForBinding(state, entry, moduleScopePath);
    releaseInternedField(entry.scopePath, entry.scopePathId);
    releaseInternedField(entry.siteKind, entry.siteKindId);
    releaseInternedField(entry.name, entry.nameId);
    releaseInternedField(entry.referenceRoot, entry.referenceRootId);
    state.semanticProgram.bindingFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.bindingFacts.size() - 1;
    module.bindingFactIndices.push_back(entryIndex);
    if (state.semanticProgram.bindingFacts.back().semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
          state.semanticProgram.bindingFacts.back().semanticNodeId,
          entryIndex);
    }
  }
}

void publishReturnFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<ReturnFactSnapshotEntry> &returnFacts) {
  if (returnFacts.empty()) {
    return;
  }
  state.semanticProgram.returnFacts.reserve(returnFacts.size());
  state.semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId.reserve(
      state.semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId.size() +
      returnFacts.size());
  state.semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId.reserve(
      state.semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId.size() +
      returnFacts.size());
  for (const auto &snapshotEntry : returnFacts) {
    SemanticProgramReturnFact entry;
    entry.returnKind = returnKindSnapshotName(snapshotEntry.kind);
    entry.structPath = snapshotEntry.structPath;
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.isMutable = snapshotEntry.binding.isMutable;
    entry.isEntryArgString = snapshotEntry.binding.isEntryArgString;
    entry.isUnsafeReference = snapshotEntry.binding.isUnsafeReference;
    entry.referenceRoot = snapshotEntry.binding.referenceRoot;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.definitionPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.definitionPath);
    entry.returnKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.returnKind);
    entry.structPathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.structPath);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.referenceRootId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.referenceRoot);
    state.semanticProgram.returnFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.returnFacts.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).returnFactIndices.push_back(
        entryIndex);
    if (state.semanticProgram.returnFacts.back().semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
          .insert_or_assign(state.semanticProgram.returnFacts.back().semanticNodeId,
                            entryIndex);
    }
    if (state.semanticProgram.returnFacts.back().definitionPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId
          .insert_or_assign(state.semanticProgram.returnFacts.back().definitionPathId,
                            entryIndex);
    }
  }
}

void publishArrayExtentFacts(
    SemanticPublicationBuilderState &state,
    std::vector<ArrayExtentFactSnapshotEntry> arrayExtentFacts) {
  if (arrayExtentFacts.empty()) {
    return;
  }
  state.semanticProgram.arrayExtentFacts.reserve(arrayExtentFacts.size());
  state.semanticProgram.publishedRoutingLookups.arrayExtentFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.arrayExtentFactIndicesByExpr.size() +
      arrayExtentFacts.size());
  for (auto &snapshotEntry : arrayExtentFacts) {
    const std::string bindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    ArrayExtentDraft draft;
    if (!classifyArrayExtentBinding(bindingTypeText, draft)) {
      continue;
    }

    SemanticProgramArrayExtentFact entry;
    entry.scopePath = std::move(snapshotEntry.scopePath);
    entry.siteKind = std::move(snapshotEntry.siteKind);
    entry.targetName = std::move(snapshotEntry.targetName);
    entry.targetResolvedPath = std::move(snapshotEntry.targetResolvedPath);
    entry.bindingTypeText = bindingTypeText;
    entry.elementTypeText = snapshotEntry.elementTypeText.empty()
                                ? draft.elementTypeText
                                : std::move(snapshotEntry.elementTypeText);
    entry.extentExpression = std::move(snapshotEntry.extentExpression);
    entry.isReference = snapshotEntry.isReference || draft.isReference;
    entry.hasStaticExtent = snapshotEntry.hasStaticExtent;
    entry.staticExtent = snapshotEntry.staticExtent;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.targetSemanticNodeId = snapshotEntry.targetSemanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.siteKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.siteKind);
    entry.targetNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.targetName);
    entry.targetResolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.targetResolvedPath);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.elementTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.elementTypeText);
    entry.extentExpressionId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.extentExpression);
    const std::string_view moduleScopePath =
        entry.scopePathId != InvalidSymbolId
            ? semanticProgramResolveCallTargetString(state.semanticProgram, entry.scopePathId)
            : std::string_view(entry.scopePath);
    auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);
    state.semanticProgram.arrayExtentFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.arrayExtentFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.arrayExtentFacts.back();
    module.arrayExtentFactIndices.push_back(entryIndex);
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.arrayExtentFactIndicesByExpr
          .insert_or_assign(publishedEntry.semanticNodeId, entryIndex);
    }
  }
}

void publishLocalAutoFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<LocalAutoBindingSnapshotEntry> &localAutoFacts) {
  if (localAutoFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.size() +
      localAutoFacts.size());
  state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.reserve(
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.size() +
      localAutoFacts.size());
  state.semanticProgram.localAutoFacts.reserve(localAutoFacts.size());
  for (const auto &snapshotEntry : localAutoFacts) {
    SemanticProgramLocalAutoFact entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.bindingName = snapshotEntry.bindingName;
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.initializerBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerBinding);
    entry.initializerReceiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerReceiverBinding);
    entry.initializerQueryTypeText = snapshotEntry.initializerQueryTypeText;
    entry.initializerResultHasValue = snapshotEntry.initializerResultHasValue;
    entry.initializerResultValueType = snapshotEntry.initializerResultValueType;
    entry.initializerResultErrorType = snapshotEntry.initializerResultErrorType;
    entry.initializerHasTry = snapshotEntry.initializerHasTry;
    entry.initializerTryOperandResolvedPath = snapshotEntry.initializerTryOperandResolvedPath;
    entry.initializerTryOperandBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerTryOperandBinding);
    entry.initializerTryOperandReceiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerTryOperandReceiverBinding);
    entry.initializerTryOperandQueryTypeText = snapshotEntry.initializerTryOperandQueryTypeText;
    entry.initializerTryValueType = snapshotEntry.initializerTryValueType;
    entry.initializerTryErrorType = snapshotEntry.initializerTryErrorType;
    entry.initializerTryContextReturnKind =
        returnKindSnapshotName(snapshotEntry.initializerTryContextReturnKind);
    entry.initializerTryOnErrorHandlerPath = snapshotEntry.initializerTryOnErrorHandlerPath;
    entry.initializerTryOnErrorErrorType = snapshotEntry.initializerTryOnErrorErrorType;
    entry.initializerTryOnErrorBoundArgCount = snapshotEntry.initializerTryOnErrorBoundArgCount;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.initializerDirectCallResolvedPath = snapshotEntry.initializerDirectCallResolvedPath;
    entry.initializerDirectCallReturnKind =
        snapshotEntry.initializerDirectCallReturnKind != ReturnKind::Unknown
            ? returnKindSnapshotName(snapshotEntry.initializerDirectCallReturnKind)
            : std::string{};
    entry.initializerMethodCallResolvedPath = snapshotEntry.initializerMethodCallResolvedPath;
    entry.initializerMethodCallReturnKind =
        snapshotEntry.initializerMethodCallReturnKind != ReturnKind::Unknown
            ? returnKindSnapshotName(snapshotEntry.initializerMethodCallReturnKind)
            : std::string{};
    entry.initializerStdlibSurfaceId =
        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerResolvedPath);
    entry.initializerDirectCallStdlibSurfaceId =
        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerDirectCallResolvedPath);
    entry.initializerMethodCallStdlibSurfaceId =
        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerMethodCallResolvedPath);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.bindingNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingName);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.initializerResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, snapshotEntry.initializerResolvedPath);
    entry.initializerBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerBindingTypeText);
    entry.initializerReceiverBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerReceiverBindingTypeText);
    entry.initializerQueryTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerQueryTypeText);
    entry.initializerResultValueTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerResultValueType);
    entry.initializerResultErrorTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerResultErrorType);
    entry.initializerTryOperandResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandResolvedPath);
    entry.initializerTryOperandBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandBindingTypeText);
    entry.initializerTryOperandReceiverBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandReceiverBindingTypeText);
    entry.initializerTryOperandQueryTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandQueryTypeText);
    entry.initializerTryValueTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.initializerTryValueType);
    entry.initializerTryErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.initializerTryErrorType);
    entry.initializerTryContextReturnKindId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryContextReturnKind);
    entry.initializerTryOnErrorHandlerPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOnErrorHandlerPath);
    entry.initializerTryOnErrorErrorTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOnErrorErrorType);
    entry.initializerDirectCallResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerDirectCallResolvedPath);
    entry.initializerDirectCallReturnKindId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerDirectCallReturnKind);
    entry.initializerMethodCallResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerMethodCallResolvedPath);
    entry.initializerMethodCallReturnKindId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerMethodCallReturnKind);
    state.semanticProgram.localAutoFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.localAutoFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.localAutoFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.initializerResolvedPathId != InvalidSymbolId &&
        publishedEntry.bindingNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId
          .insert_or_assign(makeLocalAutoInitPathBindingNameKey(publishedEntry.initializerResolvedPathId,
                                                                publishedEntry.bindingNameId),
                            entryIndex);
    }
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath).localAutoFactIndices.push_back(
        entryIndex);
  }
}

void publishQueryFacts(SemanticPublicationBuilderState &state,
                       std::vector<QueryFactSnapshotEntry> queryFacts) {
  if (queryFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.size() +
      queryFacts.size());
  state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.reserve(
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.size() +
      queryFacts.size());
  state.semanticProgram.queryFacts.reserve(queryFacts.size());
  for (auto &snapshotEntry : queryFacts) {
    SemanticProgramQueryFact entry;
    entry.scopePath = std::move(snapshotEntry.scopePath);
    entry.callName = std::move(snapshotEntry.callName);
    entry.queryTypeText = std::move(snapshotEntry.typeText);
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.receiverBindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.receiverBinding);
    entry.hasResultType = snapshotEntry.hasResultType;
    entry.resultTypeHasValue = snapshotEntry.resultTypeHasValue;
    entry.resultValueType = std::move(snapshotEntry.resultValueType);
    entry.resultErrorType = std::move(snapshotEntry.resultErrorType);
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.callNameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.callName);
    entry.resolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.resolvedPath);
    entry.queryTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.queryTypeText);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.receiverBindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.receiverBindingTypeText);
    entry.resultValueTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultValueType);
    entry.resultErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultErrorType);
    const std::string_view moduleScopePath =
        entry.scopePathId != InvalidSymbolId
            ? semanticProgramResolveCallTargetString(state.semanticProgram, entry.scopePathId)
            : std::string_view(entry.scopePath);
    auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);
    releaseInternedField(entry.scopePath, entry.scopePathId);
    releaseInternedField(entry.callName, entry.callNameId);
    state.semanticProgram.queryFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.queryFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.queryFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.sourceLine != 0 && publishedEntry.sourceColumn != 0) {
      state.semanticProgram.publishedRoutingLookups
          .queryFactIndicesBySourcePosition[makeQueryFactSourcePositionKey(
              publishedEntry.sourceLine, publishedEntry.sourceColumn)]
          .push_back(entryIndex);
    }
    if (publishedEntry.resolvedPathId != InvalidSymbolId &&
        publishedEntry.callNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
          .insert_or_assign(makeQueryFactResolvedPathCallNameKey(publishedEntry.resolvedPathId,
                                                                 publishedEntry.callNameId),
                            entryIndex);
    }
    module.queryFactIndices.push_back(entryIndex);
  }
}

void publishTryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<TryValueSnapshotEntry> &tryFacts) {
  if (tryFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.size() + tryFacts.size());
  state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.reserve(
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.size() +
      tryFacts.size());
  state.semanticProgram.tryFacts.reserve(tryFacts.size());
  for (const auto &snapshotEntry : tryFacts) {
    SemanticProgramTryFact entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.operandBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.operandBinding);
    entry.operandReceiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.operandReceiverBinding);
    entry.operandQueryTypeText = snapshotEntry.operandQueryTypeText;
    entry.valueType = snapshotEntry.valueType;
    entry.errorType = snapshotEntry.errorType;
    entry.contextReturnKind = returnKindSnapshotName(snapshotEntry.contextReturnKind);
    entry.onErrorHandlerPath = snapshotEntry.onErrorHandlerPath;
    entry.onErrorErrorType = snapshotEntry.onErrorErrorType;
    entry.onErrorBoundArgCount = snapshotEntry.onErrorBoundArgCount;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.operandResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, snapshotEntry.operandResolvedPath);
    entry.operandBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.operandBindingTypeText);
    entry.operandReceiverBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.operandReceiverBindingTypeText);
    entry.operandQueryTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.operandQueryTypeText);
    entry.valueTypeId = semanticProgramInternCallTargetString(state.semanticProgram, entry.valueType);
    entry.errorTypeId = semanticProgramInternCallTargetString(state.semanticProgram, entry.errorType);
    entry.contextReturnKindId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.contextReturnKind);
    entry.onErrorHandlerPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorHandlerPath);
    entry.onErrorErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorErrorType);
    state.semanticProgram.tryFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.tryFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.tryFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.operandResolvedPathId != InvalidSymbolId &&
        publishedEntry.sourceLine > 0 &&
        publishedEntry.sourceColumn > 0) {
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource
          .insert_or_assign(makeTryFactOperandPathSourceKey(publishedEntry.operandResolvedPathId,
                                                            publishedEntry.sourceLine,
                                                            publishedEntry.sourceColumn),
                            entryIndex);
    }
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath).tryFactIndices.push_back(entryIndex);
  }
}

void publishOnErrorFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<OnErrorSnapshotEntry> &onErrorFacts) {
  if (onErrorFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.reserve(
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.size() +
      onErrorFacts.size());
  state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.reserve(
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.size() +
      onErrorFacts.size());
  state.semanticProgram.onErrorFacts.reserve(onErrorFacts.size());
  for (const auto &snapshotEntry : onErrorFacts) {
    SemanticProgramOnErrorFact entry;
    entry.definitionPath = snapshotEntry.definitionPath;
    entry.returnKind = returnKindSnapshotName(snapshotEntry.returnKind);
    entry.errorType = snapshotEntry.errorType;
    entry.boundArgCount = snapshotEntry.boundArgCount;
    entry.boundArgTexts = snapshotEntry.boundArgTexts;
    entry.returnResultHasValue = snapshotEntry.returnResultHasValue;
    entry.returnResultValueType = snapshotEntry.returnResultValueType;
    entry.returnResultErrorType = snapshotEntry.returnResultErrorType;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.definitionPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.definitionPath);
    entry.returnKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.returnKind);
    entry.handlerPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.handlerPath);
    entry.errorTypeId = semanticProgramInternCallTargetString(state.semanticProgram, entry.errorType);
    entry.boundArgTextIds.reserve(entry.boundArgTexts.size());
    for (const auto &boundArgText : entry.boundArgTexts) {
      entry.boundArgTextIds.push_back(
          semanticProgramInternCallTargetString(state.semanticProgram, boundArgText));
    }
    entry.returnResultValueTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.returnResultValueType);
    entry.returnResultErrorTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.returnResultErrorType);
    state.semanticProgram.onErrorFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.onErrorFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.onErrorFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.definitionPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId
          .insert_or_assign(publishedEntry.definitionPathId, entryIndex);
    }
    state.ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).onErrorFactIndices.push_back(
        entryIndex);
  }
}

void publishSemanticScopedFactFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface) {
  publishBindingFacts(state, std::move(publicationSurface.bindingFacts));
  publishArrayExtentFacts(state, std::move(publicationSurface.arrayExtentFacts));
  publishReturnFacts(state, publicationSurface.returnFacts);
  publishLocalAutoFacts(state, publicationSurface.localAutoFacts);
  publishQueryFacts(state, std::move(publicationSurface.queryFacts));
  publishTryFacts(state, publicationSurface.tryFacts);
  publishOnErrorFacts(state, publicationSurface.onErrorFacts);
}

void finalizeSemanticModuleArtifacts(SemanticPublicationBuilderState &state) {
  std::sort(state.semanticProgram.moduleResolvedArtifacts.begin(),
            state.semanticProgram.moduleResolvedArtifacts.end(),
            [&state](const SemanticProgramModuleResolvedArtifacts &left,
                     const SemanticProgramModuleResolvedArtifacts &right) {
              const std::size_t leftImportOrder =
                  semanticSourceUnitImportOrderKeyForModuleKey(
                      left.identity.moduleKey,
                      state.semanticProgram.sourceImports,
                      state.semanticProgram.imports);
              const std::size_t rightImportOrder =
                  semanticSourceUnitImportOrderKeyForModuleKey(
                      right.identity.moduleKey,
                      state.semanticProgram.sourceImports,
                      state.semanticProgram.imports);
              if (leftImportOrder != rightImportOrder) {
                return leftImportOrder < rightImportOrder;
              }
              return left.identity.stableOrder < right.identity.stableOrder;
            });
  for (std::size_t moduleIndex = 0;
       moduleIndex < state.semanticProgram.moduleResolvedArtifacts.size();
       ++moduleIndex) {
    state.semanticProgram.moduleResolvedArtifacts[moduleIndex].identity.stableOrder = moduleIndex;
  }
}

} // namespace publicationBuilders
} // namespace semantics
} // namespace primec

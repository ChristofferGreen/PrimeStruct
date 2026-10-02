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


RequirementPredicateDefinitionContext makeRequirementPredicateDefinitionContext(
    const Program &program,
    const Definition &definition,
    const SemanticPublicationSurface &publicationSurface) {
  RequirementPredicateDefinitionContext context;
  context.definitionPath = definition.fullPath;
  context.namespacePrefix = definition.namespacePrefix;
  context.templateArgs = definition.templateArgs;
  context.compileTimeEffects = definitionCompileTimeEffects(definition);
  for (const auto &entry : publicationSurface.typeMetadata) {
    if (entry.category == "sum") {
      context.sumNames.insert(entry.fullPath);
    } else if (entry.category == "struct" || entry.category == "enum") {
      context.structNames.insert(entry.fullPath);
    }
  }
  for (const auto &entry : publicationSurface.sumTypeMetadata) {
    context.sumNames.insert(entry.fullPath);
  }

  auto hasTransform = [](const auto &transforms, std::string_view name) {
    for (const auto &transform : transforms) {
      if (transform.name == name) {
        return true;
      }
    }
    return false;
  };
  auto returnTypeTextForDefinition = [&](const Definition &candidate) {
    for (const auto &entry : publicationSurface.returnFacts) {
      if (entry.definitionPath != candidate.fullPath) {
        continue;
      }
      if (!entry.binding.typeName.empty()) {
        return bindingTypeTextForSemanticProduct(entry.binding);
      }
      if (!entry.structPath.empty()) {
        return entry.structPath;
      }
      return returnKindSnapshotName(entry.kind);
    }
    for (const auto &transform : candidate.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        return transform.templateArgs.front();
      }
    }
    return std::string{};
  };

  context.params.reserve(definition.parameters.size());
  for (const auto &param : definition.parameters) {
    BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (parseBindingInfo(param,
                         definition.namespacePrefix,
                         context.structNames,
                         context.importAliases,
                         binding,
                         restrictType,
                         parseError,
                         &context.sumNames,
                         nullptr,
                         /*allowCapabilityArg=*/true)) {
      context.params.push_back(ParameterInfo{param.name, std::move(binding), nullptr});
    }
  }

  context.callables.reserve(program.definitions.size());
  for (const auto &candidate : program.definitions) {
    const std::string returnType = returnTypeTextForDefinition(candidate);
    if (!candidate.parameters.empty() || !returnType.empty()) {
      RequirementPredicateDefinitionContext::CallableFact callable;
      callable.fullPath = candidate.fullPath;
      callable.namespacePrefix = candidate.namespacePrefix;
      callable.templateArgs = candidate.templateArgs;
      callable.returnType = returnType;
      callable.isPrivate = hasTransform(candidate.transforms, "private");
      for (const auto &transform : candidate.transforms) {
        if (transform.name == "effects" && transform.templateArgs.empty()) {
          callable.effectNames.insert(callable.effectNames.end(),
                                      transform.arguments.begin(),
                                      transform.arguments.end());
        }
      }
      callable.hasReturnExpr = candidate.returnExpr.has_value();
      if (candidate.returnExpr.has_value() &&
          candidate.returnExpr->kind == Expr::Kind::BoolLiteral) {
        callable.returnExprIsBoolLiteral = true;
        callable.returnBoolValue = candidate.returnExpr->boolValue;
      }
      callable.parameterTypes.reserve(candidate.parameters.size());
      bool paramsOk = true;
      for (const auto &param : candidate.parameters) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        std::string parseError;
        if (!parseBindingInfo(param,
                              candidate.namespacePrefix,
                              context.structNames,
                              context.importAliases,
                              binding,
                              restrictType,
                              parseError,
                              &context.sumNames,
                              nullptr,
                              /*allowCapabilityArg=*/true)) {
          paramsOk = false;
          break;
        }
        callable.parameterTypes.push_back(bindingTypeTextForSemanticProduct(binding));
      }
      if (paramsOk && !callable.returnType.empty()) {
        context.callables.push_back(std::move(callable));
      }
    }

    if (context.structNames.count(candidate.fullPath) == 0) {
      continue;
    }
    for (const auto &transform : candidate.transforms) {
      std::vector<std::string> traitNames;
      if (transform.name == "collection_type") {
        traitNames.push_back("Collection");
      } else if (transform.name == "key_value_type") {
        traitNames.push_back("Collection");
        traitNames.push_back("KeyValue");
      }
      for (const auto &traitName : traitNames) {
        RequirementPredicateDefinitionContext::StructTraitFact trait;
        trait.structPath = candidate.fullPath;
        trait.traitName = traitName;
        trait.isPrivate = hasTransform(candidate.transforms, "private");
        context.structTraits.push_back(std::move(trait));
      }
    }
    for (const auto &stmt : candidate.statements) {
      if (!stmt.isBinding || hasTransform(stmt.transforms, "static") ||
          isCompileTimeTypeBinding(stmt)) {
        continue;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (!parseBindingInfo(stmt,
                            candidate.namespacePrefix,
                            context.structNames,
                            context.importAliases,
                            binding,
                            restrictType,
                            parseError,
                            &context.sumNames)) {
        continue;
      }
      RequirementPredicateDefinitionContext::StructFieldFact field;
      field.structPath = candidate.fullPath;
      field.fieldName = stmt.name;
      field.typeText = bindingTypeTextForSemanticProduct(binding);
      field.isPrivate = hasTransform(stmt.transforms, "private");
      context.structFields.push_back(std::move(field));
    }
  }
  return context;
}

void publishRequirementPredicateFacts(SemanticPublicationBuilderState &state,
                                      const SemanticPublicationSurface &publicationSurface) {
  if (!state.isCollectorEnabled("requirementPredicateFacts")) {
    return;
  }
  std::size_t factCount = 0;
  for (const auto &definition : state.program.definitions) {
    for (const auto &transform : definition.transforms) {
      if (transform.name == "require") {
        factCount += transform.arguments.size();
      }
    }
  }
  if (factCount == 0) {
    return;
  }

  state.semanticProgram.requirementPredicateFacts.reserve(factCount);
  for (const auto &definition : state.program.definitions) {
    const std::vector<std::string> compileTimeEffects =
        definitionCompileTimeEffects(definition);
    for (const auto &transform : definition.transforms) {
      if (transform.name != "require") {
        continue;
      }
      const RequirementPredicateDefinitionContext context =
          makeRequirementPredicateDefinitionContext(state.program, definition, publicationSurface);
      for (const auto &argument : transform.arguments) {
        SemanticProgramRequirementPredicateFact fact =
            classifyRequirementPredicateFact(definition,
                                             transform,
                                             compileTimeEffects,
                                             argument,
                                             context);
        fact.definitionPathId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.definitionPath);
        fact.predicateKindId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.predicateKind);
        fact.predicateNameId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.predicateName);
        fact.relationOperatorId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.relationOperator);
        fact.sourceTextId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.sourceText);
        fact.compileTimeEffectIds.reserve(fact.compileTimeEffects.size());
        for (const auto &effect : fact.compileTimeEffects) {
          fact.compileTimeEffectIds.push_back(
              semanticProgramInternCallTargetString(state.semanticProgram, effect));
        }
        fact.evaluationOutcomeId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.evaluationOutcome);
        fact.evaluationDiagnosticId =
            semanticProgramInternCallTargetString(state.semanticProgram, fact.evaluationDiagnostic);
        for (auto &operand : fact.operands) {
          operand.kindId =
              semanticProgramInternCallTargetString(state.semanticProgram, operand.kind);
          operand.textId =
              semanticProgramInternCallTargetString(state.semanticProgram, operand.text);
          operand.stableHandleId =
              semanticProgramInternCallTargetString(state.semanticProgram,
                                                    operand.stableHandle);
        }
        state.semanticProgram.requirementPredicateFacts.push_back(std::move(fact));
        const std::size_t factIndex =
            state.semanticProgram.requirementPredicateFacts.size() - 1;
        state.ensureModuleResolvedArtifacts(definition.fullPath)
            .requirementPredicateFactIndices.push_back(factIndex);
      }
    }
  }
}

void publishDirectCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedDirectCallTargetEntry> &directCallTargets) {
  if (directCallTargets.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.reserve(
      directCallTargets.size());
  state.semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.reserve(
      directCallTargets.size());
  state.semanticProgram.directCallTargets.reserve(directCallTargets.size());
  for (const auto &snapshotEntry : directCallTargets) {
    SemanticProgramDirectCallTarget entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.callName = snapshotEntry.callName;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.callNameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.callName);
    entry.resolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.resolvedPath);
    entry.stdlibSurfaceId = classifyPublishedStdlibSurfaceId(snapshotEntry.resolvedPath);
    state.semanticProgram.directCallTargets.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.directCallTargets.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath)
        .directCallTargetIndices.push_back(entryIndex);
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.directCallTargets.back().resolvedPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          state.semanticProgram.directCallTargets.back().resolvedPathId);
    }
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.directCallTargets.back().stdlibSurfaceId.has_value()) {
      state.semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          *state.semanticProgram.directCallTargets.back().stdlibSurfaceId);
    }
  }
}

void publishMethodCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedMethodCallTargetEntry> &methodCallTargets) {
  if (methodCallTargets.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.reserve(
      methodCallTargets.size());
  state.semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.reserve(
      methodCallTargets.size());
  state.semanticProgram.methodCallTargets.reserve(methodCallTargets.size());
  for (const auto &snapshotEntry : methodCallTargets) {
    SemanticProgramMethodCallTarget entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.methodName = snapshotEntry.methodName;
    entry.receiverTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.receiverBinding);
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.methodNameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.methodName);
    entry.receiverTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.receiverTypeText);
    entry.resolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.resolvedPath);
    entry.stdlibSurfaceId = classifyPublishedStdlibSurfaceId(snapshotEntry.resolvedPath);
    state.semanticProgram.methodCallTargets.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.methodCallTargets.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath)
        .methodCallTargetIndices.push_back(entryIndex);
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.methodCallTargets.back().resolvedPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          state.semanticProgram.methodCallTargets.back().resolvedPathId);
    }
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.methodCallTargets.back().stdlibSurfaceId.has_value()) {
      state.semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          *state.semanticProgram.methodCallTargets.back().stdlibSurfaceId);
    }
  }
}

void publishBridgePathChoiceFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedBridgePathChoiceEntry> &bridgePathChoices) {
  if (bridgePathChoices.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.reserve(
      bridgePathChoices.size());
  state.semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.reserve(
      bridgePathChoices.size());
  state.semanticProgram.bridgePathChoices.reserve(bridgePathChoices.size());
  for (const auto &snapshotEntry : bridgePathChoices) {
    SemanticProgramBridgePathChoice entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.collectionFamily = snapshotEntry.collectionFamily;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.collectionFamilyId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.collectionFamily);
    entry.helperNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.helperName);
    entry.chosenPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.chosenPath);
    entry.stdlibSurfaceId = classifyPublishedStdlibSurfaceId(snapshotEntry.chosenPath);
    state.semanticProgram.bridgePathChoices.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.bridgePathChoices.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath)
        .bridgePathChoiceIndices.push_back(entryIndex);
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.bridgePathChoices.back().chosenPathId != InvalidSymbolId &&
        state.semanticProgram.bridgePathChoices.back().helperNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          state.semanticProgram.bridgePathChoices.back().chosenPathId);
    }
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.bridgePathChoices.back().stdlibSurfaceId.has_value()) {
      state.semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr
          .insert_or_assign(snapshotEntry.semanticNodeId,
                            *state.semanticProgram.bridgePathChoices.back().stdlibSurfaceId);
    }
  }
}

void publishCallableSummaryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedCallableSummaryEntry> &callableSummaries) {
  if (callableSummaries.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.reserve(
      callableSummaries.size());
  state.semanticProgram.callableSummaries.reserve(callableSummaries.size());
  for (const auto &snapshotEntry : callableSummaries) {
    SemanticProgramCallableSummary entry;
    entry.isExecution = snapshotEntry.isExecution;
    entry.returnKind = returnKindSnapshotName(snapshotEntry.returnKind);
    entry.isCompute = snapshotEntry.isCompute;
    entry.isUnsafe = snapshotEntry.isUnsafe;
    entry.activeEffects = snapshotEntry.activeEffects;
    entry.activeCapabilities = snapshotEntry.activeCapabilities;
    entry.hasResultType = snapshotEntry.hasResultType;
    entry.resultTypeHasValue = snapshotEntry.resultTypeHasValue;
    entry.resultValueType = snapshotEntry.resultValueType;
    entry.resultErrorType = snapshotEntry.resultErrorType;
    entry.hasOnError = snapshotEntry.hasOnError;
    entry.onErrorHandlerPath = snapshotEntry.onErrorHandlerPath;
    entry.onErrorErrorType = snapshotEntry.onErrorErrorType;
    entry.onErrorBoundArgCount = snapshotEntry.onErrorBoundArgCount;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.fullPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.fullPath);
    entry.returnKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.returnKind);
    entry.activeEffectIds.reserve(entry.activeEffects.size());
    for (const auto &activeEffect : entry.activeEffects) {
      entry.activeEffectIds.push_back(
          semanticProgramInternCallTargetString(state.semanticProgram, activeEffect));
    }
    entry.activeCapabilityIds.reserve(entry.activeCapabilities.size());
    for (const auto &activeCapability : entry.activeCapabilities) {
      entry.activeCapabilityIds.push_back(
          semanticProgramInternCallTargetString(state.semanticProgram, activeCapability));
    }
    entry.resultValueTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultValueType);
    entry.resultErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultErrorType);
    entry.onErrorHandlerPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorHandlerPath);
    entry.onErrorErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorErrorType);
    state.semanticProgram.callableSummaries.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.callableSummaries.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.fullPath)
        .callableSummaryIndices.push_back(entryIndex);
    if (state.semanticProgram.callableSummaries.back().fullPathId != InvalidSymbolId) {
      auto &callableSummaryIndicesByPathId =
          state.semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId;
      if (snapshotEntry.isExecution) {
        callableSummaryIndicesByPathId.try_emplace(
            state.semanticProgram.callableSummaries.back().fullPathId,
            entryIndex);
      } else {
        callableSummaryIndicesByPathId.insert_or_assign(
            state.semanticProgram.callableSummaries.back().fullPathId,
            entryIndex);
      }
    }
  }
}

void publishSemanticRoutingFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface) {
  publishDirectCallTargetFacts(state, publicationSurface.directCallTargets);
  publishMethodCallTargetFacts(state, publicationSurface.methodCallTargets);
  publishBridgePathChoiceFacts(state, publicationSurface.bridgePathChoices);
  publishCallableSummaryFacts(state, publicationSurface.callableSummaries);
}

void publishTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<TypeMetadataSnapshotEntry> &typeMetadata) {
  if (typeMetadata.empty()) {
    return;
  }
  state.semanticProgram.typeMetadata.reserve(typeMetadata.size());
  for (const auto &entry : typeMetadata) {
    state.semanticProgram.typeMetadata.push_back(SemanticProgramTypeMetadata{
        entry.fullPath,
        entry.category,
        entry.isPublic,
        entry.hasNoPadding,
        entry.hasPlatformIndependentPadding,
        entry.hasExplicitAlignment,
        entry.explicitAlignmentBytes,
        entry.fieldCount,
        entry.enumValueCount,
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
  }
}

void publishStructFieldMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<StructFieldMetadataSnapshotEntry> &structFieldMetadata) {
  if (structFieldMetadata.empty()) {
    return;
  }
  state.semanticProgram.structFieldMetadata.reserve(structFieldMetadata.size());
  for (const auto &entry : structFieldMetadata) {
    state.semanticProgram.structFieldMetadata.push_back(SemanticProgramStructFieldMetadata{
        entry.structPath,
        entry.fieldName,
        entry.fieldIndex,
        bindingTypeTextForSemanticProduct(entry.binding),
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
  }
}

void publishSumTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SumTypeMetadataSnapshotEntry> &sumTypeMetadata) {
  if (sumTypeMetadata.empty()) {
    return;
  }
  state.semanticProgram.sumTypeMetadata.reserve(sumTypeMetadata.size());
  state.semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId.reserve(
      state.semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId.size() +
      sumTypeMetadata.size());
  for (const auto &entry : sumTypeMetadata) {
    state.semanticProgram.sumTypeMetadata.push_back(SemanticProgramSumTypeMetadata{
        entry.fullPath,
        entry.isPublic,
        entry.activeTagTypeText,
        entry.payloadStorageText,
        entry.variantCount,
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
    const std::size_t entryIndex = state.semanticProgram.sumTypeMetadata.size() - 1;
    const SymbolId fullPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.fullPath);
    if (fullPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId
          .insert_or_assign(fullPathId, entryIndex);
    }
  }
}

} // namespace publicationBuilders
} // namespace semantics
} // namespace primec

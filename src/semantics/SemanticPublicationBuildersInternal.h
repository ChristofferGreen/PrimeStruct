#pragma once

// Internal helpers of SemanticPublicationBuilders*.cpp (TODO-5384): types and declarations; the
// definitions live in the SemanticPublicationBuilders*.cpp units.
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

namespace primec {
namespace semantics {
namespace publicationBuilders {


std::string returnKindSnapshotName(ReturnKind kind);

std::string bindingTypeTextForSemanticProduct(const BindingInfo &binding);

uint64_t makeSemanticProvenanceHandle(uint64_t semanticNodeId);

std::string normalizeSemanticModulePathKey(std::string_view path);

bool semanticSourceUnitImportMatchesPath(std::string_view importPath, std::string_view path);

std::string semanticSourceUnitModuleKeyForPath(
    std::string_view path,
    const std::vector<std::string> &sourceImports,
    const std::vector<std::string> &imports);

std::size_t semanticSourceUnitImportOrderKeyForModuleKey(
    std::string_view moduleKey,
    const std::vector<std::string> &sourceImports,
    const std::vector<std::string> &imports);

std::string fallbackBindingResolvedPathForSemanticProduct(std::string_view scopePath,
                                                          std::string_view bindingName);

std::optional<StdlibSurfaceId> classifyPublishedStdlibSurfaceId(std::string_view resolvedPath);

bool isSoftwareNumericName(std::string_view name);

bool isReflectionMetadataQueryName(std::string_view name);

bool isReflectionMetadataQueryPath(std::string_view path);

bool isRuntimeReflectionPath(std::string_view path);

bool splitTopLevelTemplateArgs(std::string_view text, std::vector<std::string> &out);

std::string findSoftwareNumericType(std::string_view typeName);

std::string scanTransformsForSoftwareNumeric(const std::vector<Transform> &transforms);

std::string scanExprForSoftwareNumeric(const Expr &expr);

std::string scanExprForRuntimeReflectionQuery(const Expr &expr);

void releaseInternedField(std::string &text, SymbolId id);

uint64_t makeLocalAutoInitPathBindingNameKey(SymbolId initializerPathId, SymbolId bindingNameId);

uint64_t makeQueryFactResolvedPathCallNameKey(SymbolId resolvedPathId, SymbolId callNameId);

uint64_t makeTryFactOperandPathSourceKey(SymbolId operandPathId, int sourceLine, int sourceColumn);

uint64_t makeSumVariantMetadataSumPathVariantNameKey(SymbolId sumPathId, SymbolId variantNameId);

SemanticProgramRequirementPredicateFact classifyRequirementPredicateFact(
    const Definition &definition,
    const Transform &transform,
    std::vector<std::string> compileTimeEffects,
    std::string sourceText,
    const RequirementPredicateDefinitionContext &context);

std::vector<std::string> definitionCompileTimeEffects(
    const Definition &definition);

struct SemanticPublicationBuilderState {
  const Program &program;
  const std::string &entryPath;
  const SemanticProductBuildConfig *buildConfig = nullptr;
  SemanticProgram semanticProgram;
  std::unordered_map<std::string, std::size_t> moduleIndexByKey;

  SemanticPublicationBuilderState(const Program &programIn,
                                  const std::string &entryPathIn,
                                  const SemanticProductBuildConfig *buildConfigIn)
      : program(programIn), entryPath(entryPathIn), buildConfig(buildConfigIn) {}

  bool isCollectorEnabled(std::string_view collectorFamily) const {
    if (buildConfig == nullptr) {
      return true;
    }
    if (buildConfig->disableAllCollectors) {
      return false;
    }
    if (!buildConfig->collectorAllowlistSpecified) {
      return true;
    }
    return std::find(buildConfig->collectorAllowlist.begin(),
                     buildConfig->collectorAllowlist.end(),
                     collectorFamily) != buildConfig->collectorAllowlist.end();
  }

  SemanticProgramModuleResolvedArtifacts &ensureModuleResolvedArtifacts(std::string_view scopePath) {
    const std::string moduleKey = semanticSourceUnitModuleKeyForPath(
        scopePath, semanticProgram.sourceImports, semanticProgram.imports);
    const auto it = moduleIndexByKey.find(moduleKey);
    if (it != moduleIndexByKey.end()) {
      return semanticProgram.moduleResolvedArtifacts[it->second];
    }
    const std::size_t moduleIndex = semanticProgram.moduleResolvedArtifacts.size();
    moduleIndexByKey.emplace(moduleKey, moduleIndex);
    semanticProgram.moduleResolvedArtifacts.push_back(SemanticProgramModuleResolvedArtifacts{});
    auto &module = semanticProgram.moduleResolvedArtifacts.back();
    module.identity.moduleKey = moduleKey;
    module.identity.stableOrder = moduleIndex;
    return module;
  }
};

struct CollectionSpecializationDraft {
  std::string collectionFamily;
  std::string elementTypeText;
  std::string keyTypeText;
  std::string valueTypeText;
  bool isReference = false;
  bool isPointer = false;
};

struct ArrayExtentDraft {
  std::string elementTypeText;
  bool isReference = false;
};

std::string collectionTypeRootForPublication(std::string_view collectionName,
                                             bool leadingSlash = false);

std::string experimentalCollectionTypeForPublication(std::string_view collectionName,
                                                     std::string_view typeName,
                                                     bool leadingSlash = false);

bool matchesStdlibSurfaceRootForPublication(std::string_view typeName,
                                            const StdlibSurfaceMetadata *metadata);

bool isUnspecializedExperimentalKeyValueBackingTypeForPublication(std::string typeName);

std::string normalizeCollectionSpecializationTypeName(std::string typeName);

bool classifyCollectionSpecialization(std::string typeText,
                                      CollectionSpecializationDraft &draftOut);

bool classifyArrayExtentBinding(std::string typeText, ArrayExtentDraft &draftOut);

std::string mangleSemanticTemplateArgsSuffix(const std::vector<std::string> &args);

std::string collectionSpecializationStructPath(const CollectionSpecializationDraft &draft);

void publishCollectionSpecializationForBinding(
    SemanticPublicationBuilderState &state,
    const SemanticProgramBindingFact &bindingEntry,
    std::string_view moduleScopePath);

void initializeSemanticProgramPublicationShell(SemanticPublicationBuilderState &state);

void preseedSemanticProgramCallTargetStrings(SemanticPublicationBuilderState &state,
                                             const SemanticPublicationSurface &publicationSurface);

void publishRoutingLookupIndexes(SemanticPublicationBuilderState &state);

void publishLowererPreflightFacts(SemanticPublicationBuilderState &state);

RequirementPredicateDefinitionContext makeRequirementPredicateDefinitionContext(
    const Program &program,
    const Definition &definition,
    const SemanticPublicationSurface &publicationSurface);

void publishRequirementPredicateFacts(SemanticPublicationBuilderState &state,
                                      const SemanticPublicationSurface &publicationSurface);

void publishDirectCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedDirectCallTargetEntry> &directCallTargets);

void publishMethodCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedMethodCallTargetEntry> &methodCallTargets);

void publishBridgePathChoiceFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedBridgePathChoiceEntry> &bridgePathChoices);

void publishCallableSummaryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedCallableSummaryEntry> &callableSummaries);

void publishSemanticRoutingFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface);

void publishTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<TypeMetadataSnapshotEntry> &typeMetadata);

void publishStructFieldMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<StructFieldMetadataSnapshotEntry> &structFieldMetadata);

void publishSumTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SumTypeMetadataSnapshotEntry> &sumTypeMetadata);

void publishSumVariantMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SumVariantMetadataSnapshotEntry> &sumVariantMetadata);

void publishSemanticMetadataFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface);

void publishBindingFacts(
    SemanticPublicationBuilderState &state,
    std::vector<BindingFactSnapshotEntry> bindingFacts);

void publishReturnFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<ReturnFactSnapshotEntry> &returnFacts);

void publishArrayExtentFacts(
    SemanticPublicationBuilderState &state,
    std::vector<ArrayExtentFactSnapshotEntry> arrayExtentFacts);

void publishLocalAutoFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<LocalAutoBindingSnapshotEntry> &localAutoFacts);

void publishQueryFacts(SemanticPublicationBuilderState &state,
                       std::vector<QueryFactSnapshotEntry> queryFacts);

void publishTryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<TryValueSnapshotEntry> &tryFacts);

void publishOnErrorFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<OnErrorSnapshotEntry> &onErrorFacts);

void publishSemanticScopedFactFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface);

void finalizeSemanticModuleArtifacts(SemanticPublicationBuilderState &state);

} // namespace publicationBuilders
} // namespace semantics
} // namespace primec

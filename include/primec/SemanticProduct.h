#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/StdlibSurfaceRegistry.h"
#include "primec/SymbolInterner.h"
#include "primec/semantic_product/DirectCallFacts.h"
#include "primec/semantic_product/MethodCallFacts.h"

namespace primec {

inline constexpr uint32_t SemanticProductContractVersionV1 = 1;
inline constexpr uint32_t SemanticProductContractVersionV2 = 2;
inline constexpr uint32_t SemanticProductContractVersionCurrent =
    SemanticProductContractVersionV2;

enum class SemanticProgramFactOwnership {
  AstProvenance,
  SemanticProduct,
  DerivedIndex,
};

struct SemanticProgramFactFamilyInfo {
  std::string_view name;
  SemanticProgramFactOwnership ownership = SemanticProgramFactOwnership::SemanticProduct;
  std::string_view description;
};

struct SemanticProgramStringHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }

  std::size_t operator()(const std::string &value) const noexcept {
    return (*this)(std::string_view(value));
  }

  std::size_t operator()(const char *value) const noexcept {
    return (*this)(std::string_view(value));
  }
};

struct SemanticProgramStringEqual {
  using is_transparent = void;

  bool operator()(std::string_view left, std::string_view right) const noexcept {
    return left == right;
  }
};

struct SemanticProgramDefinition {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramExecution {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramBridgePathChoice {
  std::string scopePath;
  std::string collectionFamily;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId collectionFamilyId = InvalidSymbolId;
  SymbolId helperNameId = InvalidSymbolId;
  SymbolId chosenPathId = InvalidSymbolId;
  std::optional<StdlibSurfaceId> stdlibSurfaceId;
};

struct SemanticProgramCallableSummary {
  bool isExecution = false;
  std::string returnKind;
  bool isCompute = false;
  bool isUnsafe = false;
  std::vector<std::string> activeEffects;
  std::vector<std::string> activeCapabilities;
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueType;
  std::string resultErrorType;
  bool hasOnError = false;
  std::string onErrorHandlerPath;
  std::string onErrorErrorType;
  std::size_t onErrorBoundArgCount = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId fullPathId = InvalidSymbolId;
  SymbolId returnKindId = InvalidSymbolId;
  std::vector<SymbolId> activeEffectIds = {};
  std::vector<SymbolId> activeCapabilityIds = {};
  SymbolId resultValueTypeId = InvalidSymbolId;
  SymbolId resultErrorTypeId = InvalidSymbolId;
  SymbolId onErrorHandlerPathId = InvalidSymbolId;
  SymbolId onErrorErrorTypeId = InvalidSymbolId;
};

struct SemanticProgramTypeMetadata {
  std::string fullPath;
  std::string category;
  bool isPublic = false;
  bool hasNoPadding = false;
  bool hasPlatformIndependentPadding = false;
  bool hasExplicitAlignment = false;
  uint32_t explicitAlignmentBytes = 0;
  std::size_t fieldCount = 0;
  std::size_t enumValueCount = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramStructFieldMetadata {
  std::string structPath;
  std::string fieldName;
  std::size_t fieldIndex = 0;
  std::string bindingTypeText;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramSumTypeMetadata {
  std::string fullPath;
  bool isPublic = false;
  std::string activeTagTypeText;
  std::string payloadStorageText;
  std::size_t variantCount = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramSumVariantMetadata {
  std::string sumPath;
  std::string variantName;
  std::size_t variantIndex = 0;
  uint32_t tagValue = 0;
  std::string payloadTypeText;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramCollectionSpecialization {
  std::string scopePath;
  std::string siteKind;
  std::string name;
  std::string collectionFamily;
  std::string bindingTypeText;
  std::string elementTypeText;
  std::string keyTypeText;
  std::string valueTypeText;
  bool isReference = false;
  bool isPointer = false;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId siteKindId = InvalidSymbolId;
  SymbolId nameId = InvalidSymbolId;
  SymbolId collectionFamilyId = InvalidSymbolId;
  SymbolId bindingTypeTextId = InvalidSymbolId;
  SymbolId elementTypeTextId = InvalidSymbolId;
  SymbolId keyTypeTextId = InvalidSymbolId;
  SymbolId valueTypeTextId = InvalidSymbolId;
  std::optional<StdlibSurfaceId> helperSurfaceId;
  std::optional<StdlibSurfaceId> constructorSurfaceId;
};

struct SemanticProgramBindingFact {
  std::string scopePath;
  std::string siteKind;
  std::string name;
  std::string bindingTypeText;
  bool isMutable = false;
  bool isEntryArgString = false;
  bool isUnsafeReference = false;
  std::string referenceRoot;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId siteKindId = InvalidSymbolId;
  SymbolId nameId = InvalidSymbolId;
  SymbolId resolvedPathId = InvalidSymbolId;
  SymbolId bindingTypeTextId = InvalidSymbolId;
  SymbolId referenceRootId = InvalidSymbolId;
};

struct SemanticProgramReturnFact {
  std::string returnKind;
  std::string structPath;
  std::string bindingTypeText;
  bool isMutable = false;
  bool isEntryArgString = false;
  bool isUnsafeReference = false;
  std::string referenceRoot;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId definitionPathId = InvalidSymbolId;
  SymbolId returnKindId = InvalidSymbolId;
  SymbolId structPathId = InvalidSymbolId;
  SymbolId bindingTypeTextId = InvalidSymbolId;
  SymbolId referenceRootId = InvalidSymbolId;
};

struct SemanticProgramLocalAutoFact {
  std::string scopePath;
  std::string bindingName;
  std::string bindingTypeText;
  std::string initializerBindingTypeText;
  std::string initializerReceiverBindingTypeText;
  std::string initializerQueryTypeText;
  bool initializerResultHasValue = false;
  std::string initializerResultValueType;
  std::string initializerResultErrorType;
  bool initializerHasTry = false;
  std::string initializerTryOperandResolvedPath;
  std::string initializerTryOperandBindingTypeText;
  std::string initializerTryOperandReceiverBindingTypeText;
  std::string initializerTryOperandQueryTypeText;
  std::string initializerTryValueType;
  std::string initializerTryErrorType;
  std::string initializerTryContextReturnKind;
  std::string initializerTryOnErrorHandlerPath;
  std::string initializerTryOnErrorErrorType;
  std::size_t initializerTryOnErrorBoundArgCount = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  std::string initializerDirectCallResolvedPath;
  std::string initializerDirectCallReturnKind;
  std::string initializerMethodCallResolvedPath;
  std::string initializerMethodCallReturnKind;
  std::optional<StdlibSurfaceId> initializerStdlibSurfaceId;
  std::optional<StdlibSurfaceId> initializerDirectCallStdlibSurfaceId;
  std::optional<StdlibSurfaceId> initializerMethodCallStdlibSurfaceId;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId bindingNameId = InvalidSymbolId;
  SymbolId bindingTypeTextId = InvalidSymbolId;
  SymbolId initializerResolvedPathId = InvalidSymbolId;
  SymbolId initializerBindingTypeTextId = InvalidSymbolId;
  SymbolId initializerReceiverBindingTypeTextId = InvalidSymbolId;
  SymbolId initializerQueryTypeTextId = InvalidSymbolId;
  SymbolId initializerResultValueTypeId = InvalidSymbolId;
  SymbolId initializerResultErrorTypeId = InvalidSymbolId;
  SymbolId initializerTryOperandResolvedPathId = InvalidSymbolId;
  SymbolId initializerTryOperandBindingTypeTextId = InvalidSymbolId;
  SymbolId initializerTryOperandReceiverBindingTypeTextId = InvalidSymbolId;
  SymbolId initializerTryOperandQueryTypeTextId = InvalidSymbolId;
  SymbolId initializerTryValueTypeId = InvalidSymbolId;
  SymbolId initializerTryErrorTypeId = InvalidSymbolId;
  SymbolId initializerTryContextReturnKindId = InvalidSymbolId;
  SymbolId initializerTryOnErrorHandlerPathId = InvalidSymbolId;
  SymbolId initializerTryOnErrorErrorTypeId = InvalidSymbolId;
  SymbolId initializerDirectCallResolvedPathId = InvalidSymbolId;
  SymbolId initializerDirectCallReturnKindId = InvalidSymbolId;
  SymbolId initializerMethodCallResolvedPathId = InvalidSymbolId;
  SymbolId initializerMethodCallReturnKindId = InvalidSymbolId;
};

struct SemanticProgramQueryFact {
  std::string scopePath;
  std::string callName;
  std::string queryTypeText;
  std::string bindingTypeText;
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueType;
  std::string resultErrorType;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId callNameId = InvalidSymbolId;
  SymbolId resolvedPathId = InvalidSymbolId;
  SymbolId queryTypeTextId = InvalidSymbolId;
  SymbolId bindingTypeTextId = InvalidSymbolId;
  SymbolId receiverBindingTypeTextId = InvalidSymbolId;
  SymbolId resultValueTypeId = InvalidSymbolId;
  SymbolId resultErrorTypeId = InvalidSymbolId;
};

struct SemanticProgramTryFact {
  std::string scopePath;
  std::string operandBindingTypeText;
  std::string operandReceiverBindingTypeText;
  std::string operandQueryTypeText;
  std::string valueType;
  std::string errorType;
  std::string contextReturnKind;
  std::string onErrorHandlerPath;
  std::string onErrorErrorType;
  std::size_t onErrorBoundArgCount = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId operandResolvedPathId = InvalidSymbolId;
  SymbolId operandBindingTypeTextId = InvalidSymbolId;
  SymbolId operandReceiverBindingTypeTextId = InvalidSymbolId;
  SymbolId operandQueryTypeTextId = InvalidSymbolId;
  SymbolId valueTypeId = InvalidSymbolId;
  SymbolId errorTypeId = InvalidSymbolId;
  SymbolId contextReturnKindId = InvalidSymbolId;
  SymbolId onErrorHandlerPathId = InvalidSymbolId;
  SymbolId onErrorErrorTypeId = InvalidSymbolId;
};

struct SemanticProgramOnErrorFact {
  std::string definitionPath;
  std::string returnKind;
  std::string errorType;
  std::size_t boundArgCount = 0;
  std::vector<std::string> boundArgTexts;
  bool returnResultHasValue = false;
  std::string returnResultValueType;
  std::string returnResultErrorType;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId definitionPathId = InvalidSymbolId;
  SymbolId returnKindId = InvalidSymbolId;
  SymbolId handlerPathId = InvalidSymbolId;
  SymbolId errorTypeId = InvalidSymbolId;
  std::vector<SymbolId> boundArgTextIds = {};
  SymbolId returnResultValueTypeId = InvalidSymbolId;
  SymbolId returnResultErrorTypeId = InvalidSymbolId;
};

struct SemanticProgramModuleIdentity {
  std::string moduleKey;
  std::size_t stableOrder = 0;
};

struct SemanticProgramModuleResolvedArtifacts {
  SemanticProgramModuleIdentity identity;
  std::vector<std::size_t> directCallTargetIndices;
  std::vector<std::size_t> methodCallTargetIndices;
  std::vector<std::size_t> bridgePathChoiceIndices;
  std::vector<std::size_t> callableSummaryIndices;
  std::vector<std::size_t> bindingFactIndices;
  std::vector<std::size_t> returnFactIndices;
  std::vector<std::size_t> collectionSpecializationIndices;
  std::vector<std::size_t> localAutoFactIndices;
  std::vector<std::size_t> queryFactIndices;
  std::vector<std::size_t> tryFactIndices;
  std::vector<std::size_t> onErrorFactIndices;
};

struct SemanticProgramPublishedRoutingLookups {
  std::unordered_map<SymbolId, std::size_t> definitionIndicesByPathId;
  std::unordered_map<SymbolId, SymbolId> importAliasTargetPathIdsByNameId;
  std::unordered_map<uint64_t, SymbolId> directCallTargetIdsByExpr;
  std::unordered_map<uint64_t, SymbolId> methodCallTargetIdsByExpr;
  std::unordered_map<uint64_t, SymbolId> bridgePathChoiceIdsByExpr;
  std::unordered_map<uint64_t, StdlibSurfaceId> directCallStdlibSurfaceIdsByExpr;
  std::unordered_map<uint64_t, StdlibSurfaceId> methodCallStdlibSurfaceIdsByExpr;
  std::unordered_map<uint64_t, StdlibSurfaceId> bridgePathChoiceStdlibSurfaceIdsByExpr;
  std::unordered_map<SymbolId, std::size_t> callableSummaryIndicesByPathId;
  std::unordered_map<uint64_t, std::size_t> collectionSpecializationIndicesByExpr;
  std::unordered_map<uint64_t, std::size_t> onErrorFactIndicesByDefinitionId;
  std::unordered_map<SymbolId, std::size_t> onErrorFactIndicesByDefinitionPathId;
  std::unordered_map<uint64_t, std::size_t> localAutoFactIndicesByExpr;
  std::unordered_map<uint64_t, std::size_t> localAutoFactIndicesByInitPathAndBindingNameId;
  std::unordered_map<uint64_t, std::size_t> queryFactIndicesByExpr;
  std::unordered_map<uint64_t, std::size_t> queryFactIndicesByResolvedPathAndCallNameId;
  std::unordered_map<uint64_t, std::size_t> tryFactIndicesByExpr;
  std::unordered_map<uint64_t, std::size_t> tryFactIndicesByOperandPathAndSource;
};

struct SemanticProgramPublishedLowererPreflightFacts {
  SymbolId firstSoftwareNumericTypeId = InvalidSymbolId;
  SymbolId firstRuntimeReflectionPathId = InvalidSymbolId;
  bool firstRuntimeReflectionPathIsObjectTable = false;
};

struct SemanticProgram {
  uint32_t contractVersion = SemanticProductContractVersionCurrent;
  bool publishedStorageFrozen = false;
  std::string entryPath;
  std::vector<std::string> sourceImports;
  std::vector<std::string> imports;
  std::vector<std::string> callTargetStringTable;
  std::unordered_map<std::string, SymbolId, SemanticProgramStringHash, SemanticProgramStringEqual>
      callTargetStringIdsByText;
  std::vector<SemanticProgramModuleResolvedArtifacts> moduleResolvedArtifacts;
  SemanticProgramPublishedRoutingLookups publishedRoutingLookups;
  SemanticProgramPublishedLowererPreflightFacts publishedLowererPreflightFacts;
  std::vector<SemanticProgramDefinition> definitions;
  std::vector<SemanticProgramExecution> executions;
  std::vector<SemanticProgramDirectCallTarget> directCallTargets;
  std::vector<SemanticProgramMethodCallTarget> methodCallTargets;
  std::vector<SemanticProgramBridgePathChoice> bridgePathChoices;
  std::vector<SemanticProgramCallableSummary> callableSummaries;
  std::vector<SemanticProgramTypeMetadata> typeMetadata;
  std::vector<SemanticProgramStructFieldMetadata> structFieldMetadata;
  std::vector<SemanticProgramSumTypeMetadata> sumTypeMetadata;
  std::vector<SemanticProgramSumVariantMetadata> sumVariantMetadata;
  std::vector<SemanticProgramCollectionSpecialization> collectionSpecializations;
  std::vector<SemanticProgramBindingFact> bindingFacts;
  std::vector<SemanticProgramReturnFact> returnFacts;
  std::vector<SemanticProgramLocalAutoFact> localAutoFacts;
  std::vector<SemanticProgramQueryFact> queryFacts;
  std::vector<SemanticProgramTryFact> tryFacts;
  std::vector<SemanticProgramOnErrorFact> onErrorFacts;
};

const std::vector<SemanticProgramFactFamilyInfo> &semanticProgramFactFamilyInfos();
std::optional<SemanticProgramFactOwnership>
semanticProgramFactFamilyOwnership(std::string_view familyName);
bool semanticProgramFactFamilyIsSemanticProductOwned(std::string_view familyName);
bool semanticProgramFactFamilyIsAstProvenanceOwned(std::string_view familyName);

std::vector<const SemanticProgramBridgePathChoice *>
semanticProgramBridgePathChoiceView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramCallableSummary *>
semanticProgramCallableSummaryView(const SemanticProgram &semanticProgram);
const SemanticProgramTypeMetadata *semanticProgramLookupTypeMetadata(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath);
std::vector<const SemanticProgramTypeMetadata *>
semanticProgramStructTypeMetadataView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramStructFieldMetadata *>
semanticProgramStructFieldMetadataView(const SemanticProgram &semanticProgram,
                                       std::string_view structPath);
std::vector<const SemanticProgramCollectionSpecialization *>
semanticProgramCollectionSpecializationView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramBindingFact *>
semanticProgramBindingFactView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramReturnFact *>
semanticProgramReturnFactView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramLocalAutoFact *>
semanticProgramLocalAutoFactView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramQueryFact *>
semanticProgramQueryFactView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramTryFact *>
semanticProgramTryFactView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramOnErrorFact *>
semanticProgramOnErrorFactView(const SemanticProgram &semanticProgram);

void freezeSemanticProgramPublishedStorage(SemanticProgram &semanticProgram);
bool semanticProgramPublishedStorageFrozen(const SemanticProgram &semanticProgram);
SymbolId semanticProgramInternCallTargetString(SemanticProgram &semanticProgram, std::string_view text);
std::optional<SymbolId> semanticProgramLookupCallTargetStringId(const SemanticProgram &semanticProgram,
                                                                std::string_view text);
void releaseSemanticProgramLookupMap(SemanticProgram &semanticProgram);
std::string_view semanticProgramResolveCallTargetString(const SemanticProgram &semanticProgram, SymbolId id);
const SemanticProgramDefinition *semanticProgramLookupPublishedDefinitionByPathId(
    const SemanticProgram &semanticProgram,
    SymbolId fullPathId);
const SemanticProgramDefinition *semanticProgramLookupPublishedDefinition(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath);
std::optional<SymbolId> semanticProgramLookupPublishedImportAliasTargetPathId(
    const SemanticProgram &semanticProgram,
    SymbolId aliasNameId);
std::optional<SymbolId> semanticProgramLookupPublishedImportAliasTargetPathId(
    const SemanticProgram &semanticProgram,
    std::string_view aliasName);
std::optional<SymbolId> semanticProgramLookupPublishedBridgePathChoiceId(const SemanticProgram &semanticProgram,
                                                                         uint64_t semanticNodeId);
std::optional<StdlibSurfaceId> semanticProgramBridgePathChoiceStdlibSurfaceId(
    const SemanticProgramBridgePathChoice &entry);
std::optional<StdlibSurfaceId> semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
std::string_view semanticProgramLookupPublishedLowererSoftwareNumericType(
    const SemanticProgram &semanticProgram);
std::string_view semanticProgramLookupPublishedLowererRuntimeReflectionPath(
    const SemanticProgram &semanticProgram);
bool semanticProgramLookupPublishedLowererRuntimeReflectionUsesObjectTable(
    const SemanticProgram &semanticProgram);
const SemanticProgramCallableSummary *semanticProgramLookupPublishedCallableSummaryByPathId(
    const SemanticProgram &semanticProgram,
    SymbolId fullPathId);
const SemanticProgramCallableSummary *semanticProgramLookupPublishedCallableSummary(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath);
const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(
    const SemanticProgram &semanticProgram,
    SymbolId definitionPathId);
const SemanticProgramCollectionSpecialization *
semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
    const SemanticProgram &semanticProgram,
    SymbolId initializerPathId,
    SymbolId bindingNameId);
const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
    const SemanticProgram &semanticProgram,
    SymbolId resolvedPathId,
    SymbolId callNameId);
const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactByOperandPathAndSource(
    const SemanticProgram &semanticProgram,
    SymbolId operandPathId,
    int sourceLine,
    int sourceColumn);
std::string_view semanticProgramBridgePathChoiceHelperName(
    const SemanticProgram &semanticProgram,
    const SemanticProgramBridgePathChoice &entry);
std::string_view semanticProgramCallableSummaryFullPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramCallableSummary &entry);
std::string_view semanticProgramBindingFactResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramBindingFact &entry);
std::string_view semanticProgramReturnFactDefinitionPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramReturnFact &entry);
std::string_view semanticProgramLocalAutoFactInitializerResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramLocalAutoFact &entry);
std::string_view semanticProgramQueryFactResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramQueryFact &entry);
std::string_view semanticProgramTryFactOperandResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramTryFact &entry);
std::string_view semanticProgramOnErrorFactDefinitionPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramOnErrorFact &entry);
std::string_view semanticProgramOnErrorFactHandlerPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramOnErrorFact &entry);

std::string formatSemanticProgram(const SemanticProgram &semanticProgram);

} // namespace primec

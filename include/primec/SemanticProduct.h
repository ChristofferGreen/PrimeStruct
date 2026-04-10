#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/SymbolInterner.h"

namespace primec {

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

struct SemanticProgramDirectCallTarget {
  std::string scopePath;
  std::string callName;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId callNameId = InvalidSymbolId;
  SymbolId resolvedPathId = InvalidSymbolId;
};

struct SemanticProgramMethodCallTarget {
  std::string scopePath;
  std::string methodName;
  std::string receiverTypeText;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId methodNameId = InvalidSymbolId;
  SymbolId receiverTypeTextId = InvalidSymbolId;
  SymbolId resolvedPathId = InvalidSymbolId;
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
  std::string handlerPath;
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
  std::vector<SemanticProgramBridgePathChoice> bridgePathChoices;
  std::vector<SemanticProgramCallableSummary> callableSummaries;
  std::vector<SemanticProgramBindingFact> bindingFacts;
  std::vector<SemanticProgramReturnFact> returnFacts;
  std::vector<SemanticProgramLocalAutoFact> localAutoFacts;
  std::vector<SemanticProgramQueryFact> queryFacts;
  std::vector<SemanticProgramTryFact> tryFacts;
  std::vector<SemanticProgramOnErrorFact> onErrorFacts;
};

struct SemanticProgram {
  std::string entryPath;
  std::vector<std::string> sourceImports;
  std::vector<std::string> imports;
  std::vector<std::string> callTargetStringTable;
  std::unordered_map<std::string, SymbolId> callTargetStringIdsByText;
  std::vector<SemanticProgramModuleResolvedArtifacts> moduleResolvedArtifacts;
  std::vector<SemanticProgramDefinition> definitions;
  std::vector<SemanticProgramExecution> executions;
  std::vector<SemanticProgramDirectCallTarget> directCallTargets;
  std::vector<SemanticProgramMethodCallTarget> methodCallTargets;
  std::vector<SemanticProgramBridgePathChoice> bridgePathChoices;
  std::vector<SemanticProgramCallableSummary> callableSummaries;
  std::vector<SemanticProgramTypeMetadata> typeMetadata;
  std::vector<SemanticProgramStructFieldMetadata> structFieldMetadata;
  std::vector<SemanticProgramBindingFact> bindingFacts;
  std::vector<SemanticProgramReturnFact> returnFacts;
  std::vector<SemanticProgramLocalAutoFact> localAutoFacts;
  std::vector<SemanticProgramQueryFact> queryFacts;
  std::vector<SemanticProgramTryFact> tryFacts;
  std::vector<SemanticProgramOnErrorFact> onErrorFacts;
};

std::vector<const SemanticProgramDirectCallTarget *>
semanticProgramDirectCallTargetView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramMethodCallTarget *>
semanticProgramMethodCallTargetView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramBridgePathChoice *>
semanticProgramBridgePathChoiceView(const SemanticProgram &semanticProgram);
std::vector<const SemanticProgramCallableSummary *>
semanticProgramCallableSummaryView(const SemanticProgram &semanticProgram);
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

SymbolId semanticProgramInternCallTargetString(SemanticProgram &semanticProgram, std::string_view text);
std::string_view semanticProgramResolveCallTargetString(const SemanticProgram &semanticProgram, SymbolId id);
std::string_view semanticProgramDirectCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramDirectCallTarget &entry);
std::string_view semanticProgramMethodCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramMethodCallTarget &entry);
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

std::string formatSemanticProgram(const SemanticProgram &semanticProgram);

} // namespace primec

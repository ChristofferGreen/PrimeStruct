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
  std::string resolvedPath;
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
  std::string resolvedPath;
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
  std::string helperName;
  std::string chosenPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramCallableSummary {
  std::string fullPath;
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
  std::string resolvedPath;
  std::string bindingTypeText;
  bool isMutable = false;
  bool isEntryArgString = false;
  bool isUnsafeReference = false;
  std::string referenceRoot;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
};

struct SemanticProgramReturnFact {
  std::string definitionPath;
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
};

struct SemanticProgramLocalAutoFact {
  std::string scopePath;
  std::string bindingName;
  std::string bindingTypeText;
  std::string initializerResolvedPath;
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
};

struct SemanticProgramQueryFact {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  std::string queryTypeText;
  std::string bindingTypeText;
  std::string receiverBindingTypeText;
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
  std::string operandResolvedPath;
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

std::string formatSemanticProgram(const SemanticProgram &semanticProgram);

} // namespace primec

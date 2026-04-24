#pragma once

#include "SemanticsHelpers.h"
#include "primec/Semantics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace primec::semantics {

struct LocalAutoTrySnapshotData {
  std::string operandResolvedPath;
  BindingInfo operandBinding;
  BindingInfo operandReceiverBinding;
  std::string operandQueryTypeText;
  std::string valueType;
  std::string errorType;
  ReturnKind contextReturnKind = ReturnKind::Unknown;
  std::string onErrorHandlerPath;
  std::string onErrorErrorType;
  size_t onErrorBoundArgCount = 0;
};

struct ResultTypeInfo {
  bool isResult = false;
  bool hasValue = false;
  std::string valueType;
  std::string errorType;
};

struct QuerySnapshotData {
  std::string resolvedPath;
  std::string typeText;
  BindingInfo binding;
  ResultTypeInfo resultInfo;
  BindingInfo receiverBinding;
};

struct CallSnapshotData {
  std::string resolvedPath;
  BindingInfo binding;
};

struct LocalAutoBindingSnapshotEntry {
  std::string scopePath;
  std::string bindingName;
  int sourceLine = 0;
  int sourceColumn = 0;
  BindingInfo binding;
  std::string initializerResolvedPath;
  BindingInfo initializerBinding;
  BindingInfo initializerReceiverBinding;
  std::string initializerQueryTypeText;
  bool initializerResultHasValue = false;
  std::string initializerResultValueType;
  std::string initializerResultErrorType;
  bool initializerHasTry = false;
  std::string initializerTryOperandResolvedPath;
  BindingInfo initializerTryOperandBinding;
  BindingInfo initializerTryOperandReceiverBinding;
  std::string initializerTryOperandQueryTypeText;
  std::string initializerTryValueType;
  std::string initializerTryErrorType;
  ReturnKind initializerTryContextReturnKind = ReturnKind::Unknown;
  std::string initializerTryOnErrorHandlerPath;
  std::string initializerTryOnErrorErrorType;
  size_t initializerTryOnErrorBoundArgCount = 0;
  uint64_t semanticNodeId = 0;
  std::string initializerDirectCallResolvedPath;
  ReturnKind initializerDirectCallReturnKind = ReturnKind::Unknown;
  std::string initializerMethodCallResolvedPath;
  ReturnKind initializerMethodCallReturnKind = ReturnKind::Unknown;
};

struct TryValueSnapshotEntry {
  std::string scopePath;
  std::string operandResolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  BindingInfo operandBinding;
  BindingInfo operandReceiverBinding;
  std::string operandQueryTypeText;
  std::string valueType;
  std::string errorType;
  ReturnKind contextReturnKind = ReturnKind::Unknown;
  std::string onErrorHandlerPath;
  std::string onErrorErrorType;
  size_t onErrorBoundArgCount = 0;
  uint64_t semanticNodeId = 0;
};

struct QueryFactSnapshotEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string typeText;
  BindingInfo binding;
  BindingInfo receiverBinding;
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueType;
  std::string resultErrorType;
  uint64_t semanticNodeId = 0;
};

struct OnErrorSnapshotEntry {
  std::string definitionPath;
  ReturnKind returnKind = ReturnKind::Unknown;
  std::string handlerPath;
  std::string errorType;
  size_t boundArgCount = 0;
  std::vector<std::string> boundArgTexts;
  bool returnResultHasValue = false;
  std::string returnResultValueType;
  std::string returnResultErrorType;
  uint64_t semanticNodeId = 0;
};

struct CollectedDirectCallTargetEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct CollectedMethodCallTargetEntry {
  std::string scopePath;
  std::string methodName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  BindingInfo receiverBinding;
  uint64_t semanticNodeId = 0;
};

struct CollectedBridgePathChoiceEntry {
  std::string scopePath;
  std::string collectionFamily;
  std::string helperName;
  std::string chosenPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct CollectedCallableSummaryEntry {
  std::string fullPath;
  bool isExecution = false;
  ReturnKind returnKind = ReturnKind::Unknown;
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
  size_t onErrorBoundArgCount = 0;
  uint64_t semanticNodeId = 0;
};

struct TypeMetadataSnapshotEntry {
  std::string fullPath;
  std::string category;
  bool isPublic = false;
  bool hasNoPadding = false;
  bool hasPlatformIndependentPadding = false;
  bool hasExplicitAlignment = false;
  uint32_t explicitAlignmentBytes = 0;
  size_t fieldCount = 0;
  size_t enumValueCount = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct StructFieldMetadataSnapshotEntry {
  std::string structPath;
  std::string fieldName;
  size_t fieldIndex = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  BindingInfo binding;
  uint64_t semanticNodeId = 0;
};

struct BindingFactSnapshotEntry {
  std::string scopePath;
  std::string siteKind;
  std::string name;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  BindingInfo binding;
  uint64_t semanticNodeId = 0;
};

struct ReturnFactSnapshotEntry {
  std::string definitionPath;
  ReturnKind kind = ReturnKind::Unknown;
  std::string structPath;
  BindingInfo binding;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct SemanticPublicationSurface {
  std::vector<std::string> callTargetSeedStrings;
  std::vector<CollectedDirectCallTargetEntry> directCallTargets;
  std::vector<CollectedMethodCallTargetEntry> methodCallTargets;
  std::vector<CollectedBridgePathChoiceEntry> bridgePathChoices;
  std::vector<CollectedCallableSummaryEntry> callableSummaries;
  std::vector<TypeMetadataSnapshotEntry> typeMetadata;
  std::vector<StructFieldMetadataSnapshotEntry> structFieldMetadata;
  std::vector<BindingFactSnapshotEntry> bindingFacts;
  std::vector<ReturnFactSnapshotEntry> returnFacts;
  std::vector<LocalAutoBindingSnapshotEntry> localAutoFacts;
  std::vector<QueryFactSnapshotEntry> queryFacts;
  std::vector<TryValueSnapshotEntry> tryFacts;
  std::vector<OnErrorSnapshotEntry> onErrorFacts;
};

} // namespace primec::semantics

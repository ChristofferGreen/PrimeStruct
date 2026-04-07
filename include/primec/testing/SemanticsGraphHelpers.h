#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

struct StronglyConnectedComponentsTestEdge {
  uint32_t sourceId = 0;
  uint32_t targetId = 0;
};

struct StronglyConnectedComponentSnapshot {
  std::vector<uint32_t> nodeIds;
};

struct StronglyConnectedComponentsSnapshot {
  std::vector<StronglyConnectedComponentSnapshot> components;
  std::vector<uint32_t> componentIdByNodeId;
};

struct CondensationDagNodeSnapshot {
  uint32_t componentId = 0;
  std::vector<uint32_t> memberNodeIds;
  std::vector<uint32_t> incomingComponentIds;
  std::vector<uint32_t> outgoingComponentIds;
};

struct CondensationDagEdgeSnapshot {
  uint32_t sourceComponentId = 0;
  uint32_t targetComponentId = 0;
};

struct CondensationDagSnapshot {
  std::vector<CondensationDagNodeSnapshot> nodes;
  std::vector<CondensationDagEdgeSnapshot> edges;
  std::vector<uint32_t> topologicalComponentIds;
  std::vector<uint32_t> componentIdByNodeId;
};

struct TypeResolutionGraphSnapshotNode {
  uint32_t id = 0;
  std::string kind;
  std::string label;
  std::string scopePath;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct TypeResolutionGraphSnapshotEdge {
  uint32_t sourceId = 0;
  uint32_t targetId = 0;
  std::string kind;
};

struct TypeResolutionGraphSnapshot {
  std::vector<TypeResolutionGraphSnapshotNode> nodes;
  std::vector<TypeResolutionGraphSnapshotEdge> edges;
  size_t nodeCount = 0;
  size_t edgeCount = 0;
  size_t definitionReturnCount = 0;
  size_t callConstraintCount = 0;
  size_t localAutoCount = 0;
  size_t dependencyEdgeCount = 0;
  size_t requirementEdgeCount = 0;
  size_t sccCount = 0;
  size_t sccMaxSize = 0;
  uint64_t prepareMillis = 0;
  uint64_t buildMillis = 0;
  uint64_t prepareMaxMillis = 0;
  uint64_t buildMaxMillis = 0;
  bool prepareOverBudget = false;
  bool buildOverBudget = false;
  uint64_t invalidationLocalBindingCount = 0;
  uint64_t invalidationControlFlowCount = 0;
  uint64_t invalidationInitializerShapeCount = 0;
  uint64_t invalidationDefinitionSignatureCount = 0;
  uint64_t invalidationImportAliasCount = 0;
  uint64_t invalidationReceiverTypeCount = 0;
};

struct TypeResolutionReturnSnapshotEntry {
  std::string definitionPath;
  std::string returnKind;
  std::string structPath;
  std::string bindingTypeText;
};

struct TypeResolutionReturnSnapshot {
  std::vector<TypeResolutionReturnSnapshotEntry> entries;
};

struct TypeResolutionLocalBindingSnapshotEntry {
  std::string scopePath;
  std::string bindingName;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string bindingTypeText;
  std::string initializerResolvedPath;
  std::string initializerBindingTypeText;
  std::string initializerReceiverBindingTypeText;
  std::string initializerQueryTypeText;
  bool initializerResultHasValue = false;
  std::string initializerResultValueTypeText;
  std::string initializerResultErrorTypeText;
  bool initializerHasTry = false;
  std::string initializerTryOperandResolvedPath;
  std::string initializerTryOperandBindingTypeText;
  std::string initializerTryOperandReceiverBindingTypeText;
  std::string initializerTryOperandQueryTypeText;
  std::string initializerTryValueTypeText;
  std::string initializerTryErrorTypeText;
  std::string initializerTryContextReturnKindText;
  std::string initializerTryOnErrorHandlerPath;
  std::string initializerTryOnErrorErrorTypeText;
  size_t initializerTryOnErrorBoundArgCount = 0;
  std::string initializerDirectCallResolvedPath;
  std::string initializerDirectCallReturnKindText;
};

struct TypeResolutionLocalBindingSnapshot {
  std::vector<TypeResolutionLocalBindingSnapshotEntry> entries;
};

struct TypeResolutionQueryCallSnapshotEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string typeText;
};

struct TypeResolutionQueryCallSnapshot {
  std::vector<TypeResolutionQueryCallSnapshotEntry> entries;
};

struct TypeResolutionQueryBindingSnapshotEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string bindingTypeText;
};

struct TypeResolutionQueryBindingSnapshot {
  std::vector<TypeResolutionQueryBindingSnapshotEntry> entries;
};

struct TypeResolutionQueryResultTypeSnapshotEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  bool hasValue = false;
  std::string valueTypeText;
  std::string errorTypeText;
};

struct TypeResolutionQueryResultTypeSnapshot {
  std::vector<TypeResolutionQueryResultTypeSnapshotEntry> entries;
};

struct TypeResolutionTryValueSnapshotEntry {
  std::string scopePath;
  std::string operandResolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string operandBindingTypeText;
  std::string operandReceiverBindingTypeText;
  std::string operandQueryTypeText;
  std::string valueTypeText;
  std::string errorTypeText;
  std::string contextReturnKindText;
  std::string onErrorHandlerPath;
  std::string onErrorErrorTypeText;
  size_t onErrorBoundArgCount = 0;
};

struct TypeResolutionTryValueSnapshot {
  std::vector<TypeResolutionTryValueSnapshotEntry> entries;
};

struct TypeResolutionCallBindingSnapshotEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string bindingTypeText;
};

struct TypeResolutionCallBindingSnapshot {
  std::vector<TypeResolutionCallBindingSnapshotEntry> entries;
};

struct TypeResolutionQueryReceiverBindingSnapshotEntry {
  std::string scopePath;
  std::string callName;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
  std::string receiverBindingTypeText;
};

struct TypeResolutionQueryReceiverBindingSnapshot {
  std::vector<TypeResolutionQueryReceiverBindingSnapshotEntry> entries;
};

struct TypeResolutionOnErrorSnapshotEntry {
  std::string definitionPath;
  std::string returnKindText;
  std::string handlerPath;
  std::string errorTypeText;
  size_t boundArgCount = 0;
  bool returnResultHasValue = false;
  std::string returnResultValueTypeText;
  std::string returnResultErrorTypeText;
};

struct TypeResolutionOnErrorSnapshot {
  std::vector<TypeResolutionOnErrorSnapshotEntry> entries;
};

struct TypeResolutionValidationContextSnapshotEntry {
  std::string definitionPath;
  std::string returnKindText;
  bool definitionIsCompute = false;
  bool definitionIsUnsafe = false;
  std::vector<std::string> activeEffects;
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueTypeText;
  std::string resultErrorTypeText;
  bool hasOnError = false;
  std::string onErrorHandlerPath;
  std::string onErrorErrorTypeText;
  size_t onErrorBoundArgCount = 0;
};

struct TypeResolutionValidationContextSnapshot {
  std::vector<TypeResolutionValidationContextSnapshotEntry> entries;
};

StronglyConnectedComponentsSnapshot computeStronglyConnectedComponentsForTesting(
    uint32_t nodeCount,
    const std::vector<StronglyConnectedComponentsTestEdge> &edges);
CondensationDagSnapshot computeCondensationDagForTesting(
    uint32_t nodeCount,
    const std::vector<StronglyConnectedComponentsTestEdge> &edges);
bool buildTypeResolutionGraphForTesting(Program program,
                                        const std::string &entryPath,
                                        std::string &error,
                                        TypeResolutionGraphSnapshot &out,
                                        const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionDependencyDagForTesting(Program program,
                                                  const std::string &entryPath,
                                                  std::string &error,
                                                  CondensationDagSnapshot &out,
                                                  const std::vector<std::string> &semanticTransforms = {});
bool dumpTypeResolutionGraphForTesting(Program program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       std::string &out,
                                       const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionReturnSnapshotForTesting(Program program,
                                                   const std::string &entryPath,
                                                   std::string &error,
                                                   TypeResolutionReturnSnapshot &out,
                                                   const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionLocalBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionLocalBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionQueryCallSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryCallSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionQueryBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionQueryResultTypeSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryResultTypeSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionTryValueSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionTryValueSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionCallBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionCallBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionQueryReceiverBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryReceiverBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionOnErrorSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionOnErrorSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});
bool computeTypeResolutionValidationContextSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionValidationContextSnapshot &out,
    const std::vector<std::string> &semanticTransforms = {});

} // namespace primec::semantics

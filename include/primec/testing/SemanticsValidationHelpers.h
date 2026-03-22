#pragma once

#include <cstdint>
#include <optional>
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
  std::string initializerQueryTypeText;
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

std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep(const std::string &text);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                              bool allowBoolCount);
bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount);
bool runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(const Expr &expr);
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

} // namespace primec::semantics

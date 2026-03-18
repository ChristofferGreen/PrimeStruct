#pragma once

#include "StronglyConnectedComponents.h"

#include <cstdint>
#include <vector>

namespace primec::semantics {

struct CondensationDagNode {
  uint32_t componentId = 0;
  std::vector<uint32_t> memberNodeIds;
  std::vector<uint32_t> incomingComponentIds;
  std::vector<uint32_t> outgoingComponentIds;
};

struct CondensationDagEdge {
  uint32_t sourceComponentId = 0;
  uint32_t targetComponentId = 0;
};

struct CondensationDag {
  std::vector<CondensationDagNode> nodes;
  std::vector<CondensationDagEdge> edges;
  std::vector<uint32_t> topologicalComponentIds;
  std::vector<uint32_t> componentIdByNodeId;
};

CondensationDag buildCondensationDag(const StronglyConnectedComponents &components,
                                     const std::vector<DirectedGraphEdge> &edges);
CondensationDag computeCondensationDag(uint32_t nodeCount,
                                       const std::vector<DirectedGraphEdge> &edges);

} // namespace primec::semantics

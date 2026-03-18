#pragma once

#include <cstdint>
#include <vector>

namespace primec::semantics {

struct DirectedGraphEdge {
  uint32_t sourceId = 0;
  uint32_t targetId = 0;
};

struct StronglyConnectedComponent {
  std::vector<uint32_t> nodeIds;
};

struct StronglyConnectedComponents {
  std::vector<StronglyConnectedComponent> components;
  std::vector<uint32_t> componentIdByNodeId;
};

StronglyConnectedComponents computeStronglyConnectedComponents(uint32_t nodeCount,
                                                              const std::vector<DirectedGraphEdge> &edges);

} // namespace primec::semantics

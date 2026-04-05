#include "StronglyConnectedComponents.h"

#include "primec/testing/SemanticsGraphHelpers.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

class TarjanSccBuilder {
public:
  TarjanSccBuilder(uint32_t nodeCount, const std::vector<DirectedGraphEdge> &edges)
      : adjacency_(nodeCount),
        indexByNode_(nodeCount, -1),
        lowLinkByNode_(nodeCount, -1),
        onStack_(nodeCount, false) {
    for (const DirectedGraphEdge &edge : edges) {
      if (edge.sourceId >= nodeCount || edge.targetId >= nodeCount) {
        continue;
      }
      adjacency_[edge.sourceId].push_back(edge.targetId);
    }
  }

  StronglyConnectedComponents build() {
    for (uint32_t nodeId = 0; nodeId < adjacency_.size(); ++nodeId) {
      if (indexByNode_[nodeId] >= 0) {
        continue;
      }
      visit(nodeId);
    }

    StronglyConnectedComponents result;
    result.components.reserve(rawComponents_.size());
    result.componentIdByNodeId.resize(adjacency_.size());
    for (auto &component : rawComponents_) {
      std::sort(component.begin(), component.end());
      result.components.push_back(StronglyConnectedComponent{std::move(component)});
    }
    normalizeComponentOrder(result);
    return result;
  }

private:
  std::vector<std::vector<uint32_t>> adjacency_;
  std::vector<int32_t> indexByNode_;
  std::vector<int32_t> lowLinkByNode_;
  std::vector<uint32_t> stack_;
  std::vector<bool> onStack_;
  std::vector<std::vector<uint32_t>> rawComponents_;
  int32_t nextIndex_ = 0;

  void visit(uint32_t nodeId) {
    indexByNode_[nodeId] = nextIndex_;
    lowLinkByNode_[nodeId] = nextIndex_;
    ++nextIndex_;
    stack_.push_back(nodeId);
    onStack_[nodeId] = true;

    for (uint32_t targetId : adjacency_[nodeId]) {
      if (indexByNode_[targetId] < 0) {
        visit(targetId);
        lowLinkByNode_[nodeId] = std::min(lowLinkByNode_[nodeId], lowLinkByNode_[targetId]);
        continue;
      }
      if (onStack_[targetId]) {
        lowLinkByNode_[nodeId] = std::min(lowLinkByNode_[nodeId], indexByNode_[targetId]);
      }
    }

    if (lowLinkByNode_[nodeId] != indexByNode_[nodeId]) {
      return;
    }

    std::vector<uint32_t> component;
    while (!stack_.empty()) {
      const uint32_t stackedNodeId = stack_.back();
      stack_.pop_back();
      onStack_[stackedNodeId] = false;
      component.push_back(stackedNodeId);
      if (stackedNodeId == nodeId) {
        break;
      }
    }
    rawComponents_.push_back(std::move(component));
  }

  static void normalizeComponentOrder(StronglyConnectedComponents &components) {
    std::vector<size_t> order(components.components.size());
    std::iota(order.begin(), order.end(), static_cast<size_t>(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t leftIndex, size_t rightIndex) {
      const uint32_t leftNodeId = components.components[leftIndex].nodeIds.front();
      const uint32_t rightNodeId = components.components[rightIndex].nodeIds.front();
      return leftNodeId < rightNodeId;
    });

    StronglyConnectedComponents normalized;
    normalized.components.reserve(components.components.size());
    normalized.componentIdByNodeId.resize(components.componentIdByNodeId.size());
    for (size_t componentId = 0; componentId < order.size(); ++componentId) {
      StronglyConnectedComponent component = std::move(components.components[order[componentId]]);
      for (uint32_t nodeId : component.nodeIds) {
        normalized.componentIdByNodeId[nodeId] = static_cast<uint32_t>(componentId);
      }
      normalized.components.push_back(std::move(component));
    }
    components = std::move(normalized);
  }
};

} // namespace

StronglyConnectedComponents computeStronglyConnectedComponents(uint32_t nodeCount,
                                                              const std::vector<DirectedGraphEdge> &edges) {
  TarjanSccBuilder builder(nodeCount, edges);
  return builder.build();
}

StronglyConnectedComponentsSnapshot computeStronglyConnectedComponentsForTesting(
    uint32_t nodeCount,
    const std::vector<StronglyConnectedComponentsTestEdge> &edges) {
  std::vector<DirectedGraphEdge> graphEdges;
  graphEdges.reserve(edges.size());
  for (const StronglyConnectedComponentsTestEdge &edge : edges) {
    graphEdges.push_back(DirectedGraphEdge{edge.sourceId, edge.targetId});
  }

  const StronglyConnectedComponents components = computeStronglyConnectedComponents(nodeCount, graphEdges);
  StronglyConnectedComponentsSnapshot snapshot;
  snapshot.componentIdByNodeId = components.componentIdByNodeId;
  snapshot.components.reserve(components.components.size());
  for (const StronglyConnectedComponent &component : components.components) {
    snapshot.components.push_back(StronglyConnectedComponentSnapshot{component.nodeIds});
  }
  return snapshot;
}

} // namespace primec::semantics

#include "CondensationDag.h"

#include "primec/testing/SemanticsValidationHelpers.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

std::vector<std::pair<uint32_t, uint32_t>> collectCondensationEdgePairs(
    const StronglyConnectedComponents &components,
    const std::vector<DirectedGraphEdge> &edges) {
  std::vector<std::pair<uint32_t, uint32_t>> pairs;
  pairs.reserve(edges.size());
  const uint32_t nodeCount = static_cast<uint32_t>(components.componentIdByNodeId.size());
  for (const DirectedGraphEdge &edge : edges) {
    if (edge.sourceId >= nodeCount || edge.targetId >= nodeCount) {
      continue;
    }
    const uint32_t sourceComponentId = components.componentIdByNodeId[edge.sourceId];
    const uint32_t targetComponentId = components.componentIdByNodeId[edge.targetId];
    if (sourceComponentId == targetComponentId) {
      continue;
    }
    pairs.emplace_back(sourceComponentId, targetComponentId);
  }
  std::sort(pairs.begin(), pairs.end());
  pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
  return pairs;
}

std::vector<uint32_t> buildTopologicalComponentOrder(const std::vector<CondensationDagNode> &nodes) {
  std::vector<uint32_t> indegreeByComponentId(nodes.size(), 0);
  for (const CondensationDagNode &node : nodes) {
    indegreeByComponentId[node.componentId] = static_cast<uint32_t>(node.incomingComponentIds.size());
  }

  std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> ready;
  for (uint32_t componentId = 0; componentId < indegreeByComponentId.size(); ++componentId) {
    if (indegreeByComponentId[componentId] == 0) {
      ready.push(componentId);
    }
  }

  std::vector<uint32_t> order;
  order.reserve(nodes.size());
  while (!ready.empty()) {
    const uint32_t componentId = ready.top();
    ready.pop();
    order.push_back(componentId);
    for (uint32_t targetComponentId : nodes[componentId].outgoingComponentIds) {
      uint32_t &indegree = indegreeByComponentId[targetComponentId];
      if (indegree == 0) {
        continue;
      }
      --indegree;
      if (indegree == 0) {
        ready.push(targetComponentId);
      }
    }
  }
  return order;
}

} // namespace

CondensationDag buildCondensationDag(const StronglyConnectedComponents &components,
                                     const std::vector<DirectedGraphEdge> &edges) {
  CondensationDag dag;
  dag.componentIdByNodeId = components.componentIdByNodeId;
  dag.nodes.reserve(components.components.size());
  for (uint32_t componentId = 0; componentId < components.components.size(); ++componentId) {
    dag.nodes.push_back(CondensationDagNode{
        componentId,
        components.components[componentId].nodeIds,
        {},
        {},
    });
  }

  const std::vector<std::pair<uint32_t, uint32_t>> edgePairs = collectCondensationEdgePairs(components, edges);
  dag.edges.reserve(edgePairs.size());
  for (const auto &[sourceComponentId, targetComponentId] : edgePairs) {
    dag.edges.push_back(CondensationDagEdge{sourceComponentId, targetComponentId});
    dag.nodes[sourceComponentId].outgoingComponentIds.push_back(targetComponentId);
    dag.nodes[targetComponentId].incomingComponentIds.push_back(sourceComponentId);
  }
  dag.topologicalComponentIds = buildTopologicalComponentOrder(dag.nodes);
  return dag;
}

CondensationDag computeCondensationDag(uint32_t nodeCount,
                                       const std::vector<DirectedGraphEdge> &edges) {
  const StronglyConnectedComponents components = computeStronglyConnectedComponents(nodeCount, edges);
  return buildCondensationDag(components, edges);
}

CondensationDagSnapshot computeCondensationDagForTesting(uint32_t nodeCount,
                                                         const std::vector<StronglyConnectedComponentsTestEdge> &edges) {
  std::vector<DirectedGraphEdge> graphEdges;
  graphEdges.reserve(edges.size());
  for (const StronglyConnectedComponentsTestEdge &edge : edges) {
    graphEdges.push_back(DirectedGraphEdge{edge.sourceId, edge.targetId});
  }

  const CondensationDag dag = computeCondensationDag(nodeCount, graphEdges);
  CondensationDagSnapshot snapshot;
  snapshot.componentIdByNodeId = dag.componentIdByNodeId;
  snapshot.topologicalComponentIds = dag.topologicalComponentIds;
  snapshot.nodes.reserve(dag.nodes.size());
  for (const CondensationDagNode &node : dag.nodes) {
    snapshot.nodes.push_back(CondensationDagNodeSnapshot{
        node.componentId,
        node.memberNodeIds,
        node.incomingComponentIds,
        node.outgoingComponentIds,
    });
  }
  snapshot.edges.reserve(dag.edges.size());
  for (const CondensationDagEdge &edge : dag.edges) {
    snapshot.edges.push_back(CondensationDagEdgeSnapshot{
        edge.sourceComponentId,
        edge.targetComponentId,
    });
  }
  return snapshot;
}

} // namespace primec::semantics

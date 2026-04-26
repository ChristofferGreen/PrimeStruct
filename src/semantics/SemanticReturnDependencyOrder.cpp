#include "primec/SemanticReturnDependencyOrder.h"

#include "CondensationDag.h"
#include "TypeResolutionGraph.h"

#include <cstdint>
#include <utility>

namespace primec::semantics {

std::vector<ReturnDependencyComponent> buildReturnDependencyOrder(const Program &program) {
  const TypeResolutionGraph graph = buildTypeResolutionGraph(program);
  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);

  std::vector<ReturnDependencyComponent> order;
  order.reserve(dag.nodes.size());
  for (auto componentIt = dag.topologicalComponentIds.rbegin();
       componentIt != dag.topologicalComponentIds.rend();
       ++componentIt) {
    const CondensationDagNode &componentNode = dag.nodes[*componentIt];
    ReturnDependencyComponent component;
    component.hasCycle = componentNode.memberNodeIds.size() > 1;
    component.definitionPaths.reserve(componentNode.memberNodeIds.size());
    for (uint32_t nodeId : componentNode.memberNodeIds) {
      if (nodeId >= graph.nodes.size()) {
        continue;
      }
      const TypeResolutionGraphNode &node = graph.nodes[nodeId];
      if (node.kind != TypeResolutionNodeKind::DefinitionReturn || node.resolvedPath.empty()) {
        continue;
      }
      component.definitionPaths.push_back(node.resolvedPath);
    }
    if (!component.definitionPaths.empty()) {
      order.push_back(std::move(component));
    }
  }
  return order;
}

} // namespace primec::semantics

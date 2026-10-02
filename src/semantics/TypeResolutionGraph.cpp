#include "TypeResolutionGraph.h"

#include "CondensationDag.h"
#include "SemanticsHelpers.h"
#include "TypeResolutionGraphPreparation.h"
#include "primec/support/CompileArena.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/testing/SemanticsGraphHelpers.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "TypeResolutionGraphBuilder.h"

namespace primec::semantics {
using namespace typeResolutionGraphBuilder;

std::string_view typeResolutionNodeKindName(TypeResolutionNodeKind kind) {
  switch (kind) {
  case TypeResolutionNodeKind::DefinitionReturn:
    return "definition_return";
  case TypeResolutionNodeKind::CallConstraint:
    return "call_constraint";
  case TypeResolutionNodeKind::LocalAuto:
    return "local_auto";
  }
  return "definition_return";
}

std::string_view typeResolutionEdgeKindName(TypeResolutionEdgeKind kind) {
  switch (kind) {
  case TypeResolutionEdgeKind::Dependency:
    return "dependency";
  case TypeResolutionEdgeKind::Requirement:
    return "requirement";
  }
  return "dependency";
}

std::string_view typeResolutionGraphInvalidationEditFamilyName(
    TypeResolutionGraphInvalidationEditFamily editFamily) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    return "local_binding";
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    return "control_flow";
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    return "initializer_shape";
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    return "definition_signature";
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    return "import_alias";
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    return "receiver_type";
  }
  return "local_binding";
}

std::string_view typeResolutionGraphInvalidationPropagationName(
    TypeResolutionGraphInvalidationPropagation propagation) {
  switch (propagation) {
  case TypeResolutionGraphInvalidationPropagation::DefinitionLocal:
    return "definition_local";
  case TypeResolutionGraphInvalidationPropagation::CrossDefinition:
    return "cross_definition";
  }
  return "definition_local";
}

const std::vector<TypeResolutionGraphInvalidationContract> &
typeResolutionGraphInvalidationContracts() {
  // TODO-5235: built via systemHeapValue() so this magic static's backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::vector<TypeResolutionGraphInvalidationContract> Contracts =
      primec::systemHeapValue([]() -> std::vector<TypeResolutionGraphInvalidationContract> {
      return {
      {TypeResolutionGraphInvalidationEditFamily::LocalBinding,
       "local_binding",
       TypeResolutionGraphInvalidationPropagation::DefinitionLocal,
       "local_auto, binding, query, try, on_error nodes in the edited definition",
       "binding/result consumers reached from the edited definition's dependency edges",
       "diagnostics attached to the edited binding and its dependent local facts"},
      {TypeResolutionGraphInvalidationEditFamily::ControlFlow,
       "control_flow",
       TypeResolutionGraphInvalidationPropagation::DefinitionLocal,
       "definition-return, branch-local binding, and control-dependent query nodes",
       "return/result consumers whose dependency edges touch the edited control-flow region",
       "branch reachability and return-consistency diagnostics in the edited definition"},
      {TypeResolutionGraphInvalidationEditFamily::InitializerShape,
       "initializer_shape",
       TypeResolutionGraphInvalidationPropagation::DefinitionLocal,
       "local_auto and call_constraint nodes owned by the edited initializer",
       "initializer binding/result queries and downstream local facts in dependency order",
       "initializer type, result-shape, and helper-resolution diagnostics for the edited site"},
      {TypeResolutionGraphInvalidationEditFamily::DefinitionSignature,
       "definition_signature",
       TypeResolutionGraphInvalidationPropagation::CrossDefinition,
       "definition_return nodes for the edited definition and directly dependent call sites",
       "dependent call_constraint, binding, result, and return nodes across import boundaries",
       "call-compatibility, return-contract, and template-argument diagnostics at dependent sites"},
      {TypeResolutionGraphInvalidationEditFamily::ImportAlias,
       "import_alias",
       TypeResolutionGraphInvalidationPropagation::CrossDefinition,
       "call_constraint nodes whose canonical path or helper-shadow choice used the alias",
       "dependent helper-routing, binding, and result queries in deterministic path order",
       "unresolved import, ambiguous helper, and alias-derived call diagnostics"},
      {TypeResolutionGraphInvalidationEditFamily::ReceiverType,
       "receiver_type",
       TypeResolutionGraphInvalidationPropagation::CrossDefinition,
       "method call_constraint nodes and receiver-derived helper-family selections",
       "dependent method targets, binding/result facts, and helper-routing queries",
       "method-target, receiver-binding, and helper-family diagnostics at dependent sites"},
      };
      });
  return Contracts;
}

const TypeResolutionGraphInvalidationContract *
typeResolutionGraphInvalidationContract(
    TypeResolutionGraphInvalidationEditFamily editFamily) {
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    if (contract.editFamily == editFamily) {
      return &contract;
    }
  }
  return nullptr;
}

uint64_t typeResolutionGraphInvalidationCount(
    const TypeResolutionGraph &graph,
    TypeResolutionGraphInvalidationEditFamily editFamily) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    return graph.invalidationLocalBindingCount;
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    return graph.invalidationControlFlowCount;
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    return graph.invalidationInitializerShapeCount;
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    return graph.invalidationDefinitionSignatureCount;
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    return graph.invalidationImportAliasCount;
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    return graph.invalidationReceiverTypeCount;
  }
  return 0;
}

TypeResolutionGraph buildTypeResolutionGraph(const Program &program) {
  const auto start = std::chrono::steady_clock::now();
  TypeResolutionGraphBuilder builder(program);
  TypeResolutionGraph graph = builder.build();
  const auto end = std::chrono::steady_clock::now();
  graph.buildMillis =
      static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  return graph;
}

bool buildTypeResolutionGraphForProgram(Program program,
                                        const std::string &entryPath,
                                        const std::vector<std::string> &semanticTransforms,
                                        std::string &error,
                                        TypeResolutionGraph &out) {
  error.clear();
  out = {};
  const auto prepStart = std::chrono::steady_clock::now();
  uint64_t explicitTemplateArgFactHitCount = 0;
  uint64_t implicitTemplateArgFactHitCount = 0;
  // Keep this baseline call-shape documented for architecture guard tests:
  // prepareProgramForTypeResolutionAnalysis(program, entryPath, semanticTransforms, error)
  if (!prepareProgramForTypeResolutionAnalysis(program,
                                               entryPath,
                                               semanticTransforms,
                                               error,
                                               &explicitTemplateArgFactHitCount,
                                               &implicitTemplateArgFactHitCount)) {
    return false;
  }
  const auto prepEnd = std::chrono::steady_clock::now();
  const InvalidationCounts invalidationCounts = computeInvalidationCounts(program);
  out = buildTypeResolutionGraph(program);
  out.prepareMillis =
      static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(prepEnd - prepStart).count());
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    setTypeResolutionGraphInvalidationCount(
        out,
        contract.editFamily,
        invalidationCountForFamily(invalidationCounts, contract.editFamily));
  }
  out.invalidationFanouts = buildInvalidationFanouts(out, invalidationCounts);
  out.explicitTemplateArgInferenceFactHitCount = explicitTemplateArgFactHitCount;
  out.implicitTemplateArgInferenceFactHitCount = implicitTemplateArgFactHitCount;
  if (const auto maxPrepare = readGraphMetricBudget("PRIMESTRUCT_GRAPH_PREPARE_MS_MAX");
      maxPrepare.has_value()) {
    out.prepareMaxMillis = *maxPrepare;
    out.prepareOverBudget = out.prepareMillis > out.prepareMaxMillis;
  }
  if (const auto maxBuild = readGraphMetricBudget("PRIMESTRUCT_GRAPH_BUILD_MS_MAX");
      maxBuild.has_value()) {
    out.buildMaxMillis = *maxBuild;
    out.buildOverBudget = out.buildMillis > out.buildMaxMillis;
  }
  return true;
}

CondensationDag computeTypeResolutionDependencyDag(const TypeResolutionGraph &graph) {
  std::vector<DirectedGraphEdge> dependencyEdges;
  dependencyEdges.reserve(graph.edges.size());
  for (const TypeResolutionGraphEdge &edge : graph.edges) {
    if (edge.kind != TypeResolutionEdgeKind::Dependency) {
      continue;
    }
    dependencyEdges.push_back(DirectedGraphEdge{edge.sourceId, edge.targetId});
  }
  return computeCondensationDag(static_cast<uint32_t>(graph.nodes.size()), dependencyEdges);
}

std::string formatTypeResolutionGraph(const TypeResolutionGraph &graph) {
  std::ostringstream out;
  out << "type_graph {\n";
  size_t definitionReturnCount = 0;
  size_t callConstraintCount = 0;
  size_t localAutoCount = 0;
  for (const auto &node : graph.nodes) {
    switch (node.kind) {
      case TypeResolutionNodeKind::DefinitionReturn:
        ++definitionReturnCount;
        break;
      case TypeResolutionNodeKind::CallConstraint:
        ++callConstraintCount;
        break;
      case TypeResolutionNodeKind::LocalAuto:
        ++localAutoCount;
        break;
    }
  }
  size_t dependencyEdgeCount = 0;
  size_t requirementEdgeCount = 0;
  for (const auto &edge : graph.edges) {
    switch (edge.kind) {
      case TypeResolutionEdgeKind::Dependency:
        ++dependencyEdgeCount;
        break;
      case TypeResolutionEdgeKind::Requirement:
        ++requirementEdgeCount;
        break;
    }
  }
  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);
  size_t maxSccSize = 0;
  for (const auto &component : dag.nodes) {
    maxSccSize = std::max(maxSccSize, component.memberNodeIds.size());
  }
  out << "  metrics prepare_ms=" << graph.prepareMillis
      << " build_ms=" << graph.buildMillis
      << " prepare_ms_max=" << graph.prepareMaxMillis
      << " build_ms_max=" << graph.buildMaxMillis
      << " prepare_over=" << (graph.prepareOverBudget ? "true" : "false")
      << " build_over=" << (graph.buildOverBudget ? "true" : "false")
      << " nodes=" << graph.nodes.size()
      << " edges=" << graph.edges.size()
      << " nodes_definition_return=" << definitionReturnCount
      << " nodes_call_constraint=" << callConstraintCount
      << " nodes_local_auto=" << localAutoCount
      << " edges_dependency=" << dependencyEdgeCount
      << " edges_requirement=" << requirementEdgeCount
      << " scc_count=" << dag.nodes.size()
      << " scc_max_size=" << maxSccSize
      << " invalidation_local_binding=" << graph.invalidationLocalBindingCount
      << " invalidation_control_flow=" << graph.invalidationControlFlowCount
      << " invalidation_initializer_shape=" << graph.invalidationInitializerShapeCount
      << " invalidation_definition_signature=" << graph.invalidationDefinitionSignatureCount
      << " invalidation_import_alias=" << graph.invalidationImportAliasCount
      << " invalidation_receiver_type=" << graph.invalidationReceiverTypeCount << "\n";
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    out << "  invalidation_contract name=" << contract.name
        << " propagation=" << typeResolutionGraphInvalidationPropagationName(contract.propagation)
        << " observed=" << typeResolutionGraphInvalidationCount(graph, contract.editFamily)
        << " immediate=\"" << contract.immediateInvalidations << "\""
        << " lazy=\"" << contract.lazyRevisits << "\""
        << " diagnostics=\"" << contract.diagnosticDiscards << "\"\n";
  }
  for (const auto &fanout : graph.invalidationFanouts) {
    out << "  invalidation_fanout family="
        << typeResolutionGraphInvalidationEditFamilyName(fanout.editFamily)
        << " trigger=" << fanout.triggerNodeId
        << " label=\"" << fanout.triggerLabel << "\""
        << " immediate=" << formatNodeIdList(fanout.immediateNodeIds)
        << " lazy=" << formatNodeIdList(fanout.lazyRevisitNodeIds)
        << " diagnostics=" << formatNodeIdList(fanout.diagnosticNodeIds) << "\n";
  }
  for (const auto &node : graph.nodes) {
    out << "  node " << node.id << " kind=" << typeResolutionNodeKindName(node.kind)
        << " label=\"" << node.label << "\""
        << " scope=\"" << node.scopePath << "\""
        << " path=\"" << node.resolvedPath << "\""
        << " line=" << node.sourceLine
        << " column=" << node.sourceColumn << "\n";
  }
  for (size_t edgeIndex = 0; edgeIndex < graph.edges.size(); ++edgeIndex) {
    const auto &edge = graph.edges[edgeIndex];
    out << "  edge " << edgeIndex << " kind=" << typeResolutionEdgeKindName(edge.kind)
        << " source=" << edge.sourceId
        << " target=" << edge.targetId << "\n";
  }
  out << "}\n";
  return out.str();
}

bool buildTypeResolutionGraphForTesting(Program program,
                                        const std::string &entryPath,
                                        std::string &error,
                                        TypeResolutionGraphSnapshot &out,
                                        const std::vector<std::string> &semanticTransforms) {
  TypeResolutionGraph graph;
  if (!buildTypeResolutionGraphForProgram(std::move(program), entryPath, semanticTransforms, error, graph)) {
    out = {};
    return false;
  }
  out = {};
  out.nodeCount = graph.nodes.size();
  out.edgeCount = graph.edges.size();
  for (const auto &node : graph.nodes) {
    switch (node.kind) {
      case TypeResolutionNodeKind::DefinitionReturn:
        ++out.definitionReturnCount;
        break;
      case TypeResolutionNodeKind::CallConstraint:
        ++out.callConstraintCount;
        break;
      case TypeResolutionNodeKind::LocalAuto:
        ++out.localAutoCount;
        break;
    }
  }
  for (const auto &edge : graph.edges) {
    switch (edge.kind) {
      case TypeResolutionEdgeKind::Dependency:
        ++out.dependencyEdgeCount;
        break;
      case TypeResolutionEdgeKind::Requirement:
        ++out.requirementEdgeCount;
        break;
    }
  }
  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);
  size_t maxSccSize = 0;
  for (const auto &component : dag.nodes) {
    maxSccSize = std::max(maxSccSize, component.memberNodeIds.size());
  }
  out.sccCount = dag.nodes.size();
  out.sccMaxSize = maxSccSize;
  out.prepareMillis = graph.prepareMillis;
  out.buildMillis = graph.buildMillis;
  out.prepareMaxMillis = graph.prepareMaxMillis;
  out.buildMaxMillis = graph.buildMaxMillis;
  out.prepareOverBudget = graph.prepareOverBudget;
  out.buildOverBudget = graph.buildOverBudget;
  out.invalidationLocalBindingCount = graph.invalidationLocalBindingCount;
  out.invalidationControlFlowCount = graph.invalidationControlFlowCount;
  out.invalidationInitializerShapeCount = graph.invalidationInitializerShapeCount;
  out.invalidationDefinitionSignatureCount = graph.invalidationDefinitionSignatureCount;
  out.invalidationImportAliasCount = graph.invalidationImportAliasCount;
  out.invalidationReceiverTypeCount = graph.invalidationReceiverTypeCount;
  out.explicitTemplateArgInferenceFactHitCount = graph.explicitTemplateArgInferenceFactHitCount;
  out.implicitTemplateArgInferenceFactHitCount = graph.implicitTemplateArgInferenceFactHitCount;
  out.nodes.reserve(graph.nodes.size());
  for (const auto &node : graph.nodes) {
    out.nodes.push_back(TypeResolutionGraphSnapshotNode{
        node.id,
        std::string(typeResolutionNodeKindName(node.kind)),
        node.label,
        node.scopePath,
        node.resolvedPath,
        node.sourceLine,
        node.sourceColumn,
    });
  }
  out.edges.reserve(graph.edges.size());
  for (const auto &edge : graph.edges) {
    out.edges.push_back(TypeResolutionGraphSnapshotEdge{
        edge.sourceId,
        edge.targetId,
        std::string(typeResolutionEdgeKindName(edge.kind)),
    });
  }
  out.invalidationContracts.reserve(typeResolutionGraphInvalidationContracts().size());
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    out.invalidationContracts.push_back(TypeResolutionGraphInvalidationContractSnapshot{
        std::string(contract.name),
        std::string(typeResolutionGraphInvalidationPropagationName(contract.propagation)),
        std::string(contract.immediateInvalidations),
        std::string(contract.lazyRevisits),
        std::string(contract.diagnosticDiscards),
        typeResolutionGraphInvalidationCount(graph, contract.editFamily),
    });
  }
  out.invalidationFanouts.reserve(graph.invalidationFanouts.size());
  for (const auto &fanout : graph.invalidationFanouts) {
    out.invalidationFanouts.push_back(TypeResolutionGraphInvalidationFanoutSnapshot{
        std::string(typeResolutionGraphInvalidationEditFamilyName(fanout.editFamily)),
        fanout.triggerNodeId,
        fanout.triggerLabel,
        fanout.immediateNodeIds,
        fanout.lazyRevisitNodeIds,
        fanout.diagnosticNodeIds,
    });
  }
  return true;
}

bool computeTypeResolutionDependencyDagForTesting(Program program,
                                                  const std::string &entryPath,
                                                  std::string &error,
                                                  CondensationDagSnapshot &out,
                                                  const std::vector<std::string> &semanticTransforms) {
  TypeResolutionGraph graph;
  if (!buildTypeResolutionGraphForProgram(std::move(program), entryPath, semanticTransforms, error, graph)) {
    out = {};
    return false;
  }

  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);
  out = {};
  out.componentIdByNodeId = dag.componentIdByNodeId;
  out.topologicalComponentIds = dag.topologicalComponentIds;
  out.nodes.reserve(dag.nodes.size());
  for (const CondensationDagNode &node : dag.nodes) {
    out.nodes.push_back(CondensationDagNodeSnapshot{
        node.componentId,
        node.memberNodeIds,
        node.incomingComponentIds,
        node.outgoingComponentIds,
    });
  }
  out.edges.reserve(dag.edges.size());
  for (const CondensationDagEdge &edge : dag.edges) {
    out.edges.push_back(CondensationDagEdgeSnapshot{
        edge.sourceComponentId,
        edge.targetComponentId,
    });
  }
  return true;
}

bool dumpTypeResolutionGraphForTesting(Program program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       std::string &out,
                                       const std::vector<std::string> &semanticTransforms) {
  TypeResolutionGraph graph;
  if (!buildTypeResolutionGraphForProgram(std::move(program), entryPath, semanticTransforms, error, graph)) {
    out.clear();
    return false;
  }
  out = formatTypeResolutionGraph(graph);
  return true;
}

} // namespace primec::semantics

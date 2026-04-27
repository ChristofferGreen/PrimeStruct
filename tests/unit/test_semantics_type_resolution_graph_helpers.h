#include "primec/testing/SemanticsGraphHelpers.h"

namespace {

[[maybe_unused]] const primec::semantics::TypeResolutionGraphSnapshotNode &requireGraphNode(
    const primec::semantics::TypeResolutionGraphSnapshot &graph,
    uint32_t id) {
  return graph.nodes.at(id);
}

[[maybe_unused]] const primec::semantics::TypeResolutionGraphSnapshotEdge &requireGraphEdge(
    const primec::semantics::TypeResolutionGraphSnapshot &graph,
    uint32_t index) {
  return graph.edges.at(index);
}

[[maybe_unused]] primec::semantics::TypeResolutionGraphInvalidationContractSnapshot
requireInvalidationContract(const primec::semantics::TypeResolutionGraphSnapshot &graph,
                            const std::string &editFamily) {
  const auto it = std::find_if(
      graph.invalidationContracts.begin(),
      graph.invalidationContracts.end(),
      [&](const auto &contract) { return contract.editFamily == editFamily; });
  REQUIRE(it != graph.invalidationContracts.end());
  return *it;
}

[[maybe_unused]] std::string requireTypeResolutionGraphDump(const std::string &source, const std::string &entryPath) {
  std::string error;
  std::string dump;
  const bool ok = primec::semantics::dumpTypeResolutionGraphForTesting(parseProgram(source), entryPath, error, dump);
  CHECK(ok);
  if (!ok) {
    return {};
  }
  CHECK(error.empty());
  return dump;
}

[[maybe_unused]] std::string stripTypeResolutionGraphVariableHeader(const std::string &dump) {
  const std::string header = "type_graph {\n";
  REQUIRE(dump.rfind(header, 0) == 0);
  size_t lineStart = header.size();
  while (lineStart < dump.size()) {
    const size_t lineEnd = dump.find('\n', lineStart);
    REQUIRE(lineEnd != std::string::npos);
    const std::string line = dump.substr(lineStart, lineEnd - lineStart);
    if (line.rfind("  metrics ", 0) != 0 &&
        line.rfind("  invalidation_contract ", 0) != 0) {
      break;
    }
    lineStart = lineEnd + 1;
  }
  return header + dump.substr(lineStart);
}

[[maybe_unused]] primec::semantics::TypeResolutionLocalBindingSnapshotEntry requireLocalBindingSnapshotEntry(
    const primec::semantics::TypeResolutionLocalBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &bindingName) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.bindingName == bindingName;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

[[maybe_unused]] primec::semantics::TypeResolutionTryValueSnapshotEntry requireTryValueSnapshotEntry(
    const primec::semantics::TypeResolutionTryValueSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &operandResolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.operandResolvedPath == operandResolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

[[maybe_unused]] primec::semantics::TypeResolutionQueryCallSnapshotEntry requireQueryCallSnapshotEntry(
    const primec::semantics::TypeResolutionQueryCallSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

[[maybe_unused]] primec::semantics::TypeResolutionQueryBindingSnapshotEntry requireQueryBindingSnapshotEntry(
    const primec::semantics::TypeResolutionQueryBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

[[maybe_unused]] primec::semantics::TypeResolutionQueryResultTypeSnapshotEntry requireQueryResultTypeSnapshotEntry(
    const primec::semantics::TypeResolutionQueryResultTypeSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

[[maybe_unused]] primec::semantics::TypeResolutionCallBindingSnapshotEntry requireCallBindingSnapshotEntry(
    const primec::semantics::TypeResolutionCallBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

[[maybe_unused]] size_t requireTopologicalComponentIndex(const primec::semantics::CondensationDagSnapshot &dag, uint32_t componentId) {
  const auto it = std::find(dag.topologicalComponentIds.begin(), dag.topologicalComponentIds.end(), componentId);
  REQUIRE(it != dag.topologicalComponentIds.end());
  return static_cast<size_t>(std::distance(dag.topologicalComponentIds.begin(), it));
}

[[maybe_unused]] bool hasCondensationEdge(const primec::semantics::CondensationDagSnapshot &dag,
                                          uint32_t sourceComponentId,
                                          uint32_t targetComponentId) {
  return std::find_if(dag.edges.begin(), dag.edges.end(), [&](const auto &edge) {
           return edge.sourceComponentId == sourceComponentId &&
                  edge.targetComponentId == targetComponentId;
         }) != dag.edges.end();
}

} // namespace

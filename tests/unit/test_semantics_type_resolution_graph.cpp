#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

#include "primec/testing/SemanticsGraphHelpers.h"

#include "third_party/doctest.h"
#include "test_semantics_helpers.h"

namespace {

struct ScopedEnvVar {
  explicit ScopedEnvVar(const char *name, const std::string &value)
      : name_(name) {
    const char *existing = std::getenv(name_);
    if (existing != nullptr) {
      previous_ = existing;
    }
    setenv(name_, value.c_str(), 1);
  }

  ~ScopedEnvVar() {
    if (previous_.has_value()) {
      setenv(name_, previous_->c_str(), 1);
    } else {
      unsetenv(name_);
    }
  }

  ScopedEnvVar(const ScopedEnvVar &) = delete;
  ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

private:
  const char *name_;
  std::optional<std::string> previous_;
};

} // namespace

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

#include "test_semantics_type_resolution_graph_helpers.h"

TEST_CASE("type resolution graph builder keeps stable node and edge order for return and local auto dependencies") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
mid() {
  return(leaf())
}

[return<auto>]
main() {
  [auto] value{mid()}
  return(value)
}
)";
  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot graph;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(parseProgram(source), "/main", error, graph));
  CHECK(error.empty());

  REQUIRE(graph.nodes.size() == 6);
  CHECK(requireGraphNode(graph, 0).kind == "definition_return");
  CHECK(requireGraphNode(graph, 0).label == "/leaf");
  CHECK(requireGraphNode(graph, 1).kind == "definition_return");
  CHECK(requireGraphNode(graph, 1).label == "/mid");
  CHECK(requireGraphNode(graph, 2).kind == "definition_return");
  CHECK(requireGraphNode(graph, 2).label == "/main");
  CHECK(requireGraphNode(graph, 3).kind == "call_constraint");
  CHECK(requireGraphNode(graph, 3).label == "/mid::call#0");
  CHECK(requireGraphNode(graph, 3).resolvedPath == "/leaf");
  CHECK(requireGraphNode(graph, 4).kind == "call_constraint");
  CHECK(requireGraphNode(graph, 4).label == "/main::call#0");
  CHECK(requireGraphNode(graph, 4).resolvedPath == "/mid");
  CHECK(requireGraphNode(graph, 5).kind == "local_auto");
  CHECK(requireGraphNode(graph, 5).label == "/main::auto:value#0");

  REQUIRE(graph.edges.size() == 4);
  CHECK(requireGraphEdge(graph, 0).kind == "dependency");
  CHECK(requireGraphEdge(graph, 0).sourceId == 3);
  CHECK(requireGraphEdge(graph, 0).targetId == 0);
  CHECK(requireGraphEdge(graph, 1).kind == "dependency");
  CHECK(requireGraphEdge(graph, 1).sourceId == 1);
  CHECK(requireGraphEdge(graph, 1).targetId == 3);
  CHECK(requireGraphEdge(graph, 2).kind == "dependency");
  CHECK(requireGraphEdge(graph, 2).sourceId == 4);
  CHECK(requireGraphEdge(graph, 2).targetId == 1);
  CHECK(requireGraphEdge(graph, 3).kind == "dependency");
  CHECK(requireGraphEdge(graph, 3).sourceId == 5);
  CHECK(requireGraphEdge(graph, 3).targetId == 4);
}

TEST_CASE("type resolution graph builder records requirement edges for explicit return contracts") {
  const std::string source = R"(
[return<i32>]
leaf() {
  return(1i32)
}

[return<i32>]
main() {
  return(leaf())
}
)";
  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot graph;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(parseProgram(source), "/main", error, graph));
  CHECK(error.empty());

  REQUIRE(graph.nodes.size() == 3);
  CHECK(requireGraphNode(graph, 0).label == "/leaf");
  CHECK(requireGraphNode(graph, 1).label == "/main");
  CHECK(requireGraphNode(graph, 2).kind == "call_constraint");
  CHECK(requireGraphNode(graph, 2).resolvedPath == "/leaf");

  REQUIRE(graph.edges.size() == 2);
  CHECK(requireGraphEdge(graph, 0).kind == "dependency");
  CHECK(requireGraphEdge(graph, 0).sourceId == 2);
  CHECK(requireGraphEdge(graph, 0).targetId == 0);
  CHECK(requireGraphEdge(graph, 1).kind == "requirement");
  CHECK(requireGraphEdge(graph, 1).sourceId == 1);
  CHECK(requireGraphEdge(graph, 1).targetId == 2);
}

TEST_CASE("type resolution dependency dag helper groups mutual-recursion graph components") {
  const std::string source = R"(
[return<auto>]
alpha() {
  return(beta())
}

TEST_CASE("type resolution graph snapshot honors budget env vars") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

TEST_CASE("type resolution graph perf soak (disabled by default)") {
  if (std::getenv("PRIMESTRUCT_GRAPH_SOAK") == nullptr) {
    return;
  }

  std::string source = R"(
[return<i32>]
leaf([i32] value) {
  return(value)
}

TEST_CASE("type resolution graph layer ordering stays consistent") {
  const std::string source = R"(
[return<auto>]
leaf([i32] value) {
  return(value)
}

TEST_CASE("type resolution graph local binding invalidation follows dependency chain") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
main() {
  [auto] value{leaf()}
  return(value)
}
)";

  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot graph;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(parseProgram(source), "/main", error, graph));
  CHECK(error.empty());

  auto findNodeId = [&](const std::string &label) -> uint32_t {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const auto &node) {
      return node.label == label;
    });
    REQUIRE(it != graph.nodes.end());
    return it->id;
  };

  const uint32_t localAutoId = findNodeId("/main::auto:value#0");
  std::vector<uint32_t> stack{localAutoId};
  std::vector<bool> visited(graph.nodes.size(), false);
  visited[localAutoId] = true;
  while (!stack.empty()) {
    const uint32_t current = stack.back();
    stack.pop_back();
    for (const auto &edge : graph.edges) {
      if (edge.kind != "dependency" || edge.sourceId != current) {
        continue;
      }
      if (edge.targetId >= visited.size() || visited[edge.targetId]) {
        continue;
      }
      visited[edge.targetId] = true;
      stack.push_back(edge.targetId);
    }
  }

  auto hasLabel = [&](const std::string &label) {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const auto &node) {
      return node.label == label;
    });
    REQUIRE(it != graph.nodes.end());
    return visited[it->id];
  };

  CHECK(hasLabel("/main::auto:value#0"));
  CHECK(hasLabel("/main::call#0"));
  CHECK(hasLabel("/leaf"));
}

[return<auto>]
main() {
  return(leaf(1i32))
}
)";

  std::string error;
  primec::semantics::TypeResolutionReturnSnapshot baseline;
  REQUIRE(primec::semantics::computeTypeResolutionReturnSnapshotForTesting(
      parseProgram(source), "/main", error, baseline));
  CHECK(error.empty());

  primec::semantics::TypeResolutionReturnSnapshot forward;
  {
    const ScopedEnvVar forwardOrder("PRIMESTRUCT_GRAPH_LAYER_ORDER", "forward");
    REQUIRE(primec::semantics::computeTypeResolutionReturnSnapshotForTesting(
        parseProgram(source), "/main", error, forward));
  }
  CHECK(error.empty());
  REQUIRE(baseline.entries.size() == forward.entries.size());
  for (size_t index = 0; index < baseline.entries.size(); ++index) {
    CHECK(baseline.entries[index].definitionPath == forward.entries[index].definitionPath);
    CHECK(baseline.entries[index].returnKind == forward.entries[index].returnKind);
    CHECK(baseline.entries[index].structPath == forward.entries[index].structPath);
    CHECK(baseline.entries[index].bindingTypeText == forward.entries[index].bindingTypeText);
  }
}

[return<i32>]
main() {
  [auto] value{0i32}
  [auto] a{leaf(value)}
  [auto] b{leaf(a)}
  [auto] c{leaf(b)}
  [auto] d{leaf(c)}
  [auto] e{leaf(d)}
  [auto] f{leaf(e)}
  [auto] g{leaf(f)}
  return(g)
}
)";

  for (int index = 0; index < 200; ++index) {
    std::string error;
    primec::semantics::TypeResolutionGraphSnapshot snapshot;
    REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(
        parseProgram(source), "/main", error, snapshot));
    CHECK(error.empty());
  }
}

[return<auto>]
main() {
  return(leaf())
}
)";

  uint64_t prepareMillis = 0;
  uint64_t buildMillis = 0;
  {
    const ScopedEnvVar prepareBudget("PRIMESTRUCT_GRAPH_PREPARE_MS_MAX", "0");
    const ScopedEnvVar buildBudget("PRIMESTRUCT_GRAPH_BUILD_MS_MAX", "0");

    std::string error;
    primec::semantics::TypeResolutionGraphSnapshot snapshot;
    REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(parseProgram(source), "/main", error, snapshot));
    CHECK(error.empty());

    CHECK(snapshot.prepareMaxMillis == 0u);
    CHECK(snapshot.buildMaxMillis == 0u);
    CHECK(snapshot.prepareOverBudget == (snapshot.prepareMillis > snapshot.prepareMaxMillis));
    CHECK(snapshot.buildOverBudget == (snapshot.buildMillis > snapshot.buildMaxMillis));
    prepareMillis = snapshot.prepareMillis;
    buildMillis = snapshot.buildMillis;
  }

  const ScopedEnvVar prepareBudget("PRIMESTRUCT_GRAPH_PREPARE_MS_MAX", std::to_string(prepareMillis + 1u));
  const ScopedEnvVar buildBudget("PRIMESTRUCT_GRAPH_BUILD_MS_MAX", std::to_string(buildMillis + 1u));

  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot snapshot;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  CHECK(snapshot.prepareOverBudget == false);
  CHECK(snapshot.buildOverBudget == false);
}

[return<auto>]
beta() {
  return(alpha())
}
)";
  std::string error;
  primec::semantics::CondensationDagSnapshot dag;
  REQUIRE(primec::semantics::computeTypeResolutionDependencyDagForTesting(
      parseProgram(source), "/alpha", error, dag));
  CHECK(error.empty());

  REQUIRE(dag.nodes.size() == 1);
  CHECK(dag.nodes[0].memberNodeIds == std::vector<uint32_t>({0, 1, 2, 3}));
  CHECK(dag.topologicalComponentIds == std::vector<uint32_t>({0}));
  CHECK(dag.componentIdByNodeId == std::vector<uint32_t>({0, 0, 0, 0}));
}

TEST_CASE("type resolution dependency dag helper preserves acyclic graph dependency order") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
mid() {
  return(leaf())
}

[return<auto>]
main() {
  [auto] value{mid()}
  return(value)
}
)";
  std::string error;
  primec::semantics::CondensationDagSnapshot dag;
  REQUIRE(primec::semantics::computeTypeResolutionDependencyDagForTesting(
      parseProgram(source), "/main", error, dag));
  CHECK(error.empty());

  REQUIRE(dag.nodes.size() == 6);
  REQUIRE(dag.edges.size() == 4);
  CHECK(hasCondensationEdge(dag, dag.componentIdByNodeId[1], dag.componentIdByNodeId[3]));
  CHECK(hasCondensationEdge(dag, dag.componentIdByNodeId[3], dag.componentIdByNodeId[0]));
  CHECK(hasCondensationEdge(dag, dag.componentIdByNodeId[4], dag.componentIdByNodeId[1]));
  CHECK(hasCondensationEdge(dag, dag.componentIdByNodeId[5], dag.componentIdByNodeId[4]));

  CHECK(requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[5]) <
        requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[4]));
  CHECK(requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[4]) <
        requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[1]));
  CHECK(requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[1]) <
        requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[3]));
  CHECK(requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[3]) <
        requireTopologicalComponentIndex(dag, dag.componentIdByNodeId[0]));
}

TEST_CASE("type resolution graph builder resolves imported public call targets") {
  const std::string source = R"(
import /pkg/leaf

[public return<auto>]
/pkg/leaf() {
  return(1i32)
}

[return<auto>]
main() {
  return(leaf())
}
)";
  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot graph;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(parseProgram(source), "/main", error, graph));
  CHECK(error.empty());

  REQUIRE(graph.nodes.size() == 3);
  CHECK(requireGraphNode(graph, 0).label == "/pkg/leaf");
  CHECK(requireGraphNode(graph, 1).label == "/main");
  CHECK(requireGraphNode(graph, 2).kind == "call_constraint");
  CHECK(requireGraphNode(graph, 2).resolvedPath == "/pkg/leaf");

  REQUIRE(graph.edges.size() == 2);
  CHECK(requireGraphEdge(graph, 0).kind == "dependency");
  CHECK(requireGraphEdge(graph, 0).sourceId == 2);
  CHECK(requireGraphEdge(graph, 0).targetId == 0);
  CHECK(requireGraphEdge(graph, 1).kind == "dependency");
  CHECK(requireGraphEdge(graph, 1).sourceId == 1);
  CHECK(requireGraphEdge(graph, 1).targetId == 2);
}

TEST_CASE("type resolution graph dump stays stable for a simple call chain") {
  const std::string source =
      "[return<auto>]\n"
      "leaf() {\n"
      "  return(1i32)\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "main() {\n"
      "  return(leaf())\n"
      "}\n";

  const std::string expected =
      "type_graph {\n"
      "  node 0 kind=definition_return label=\"/leaf\" scope=\"/leaf\" path=\"/leaf\" line=2 column=1\n"
      "  node 1 kind=definition_return label=\"/main\" scope=\"/main\" path=\"/main\" line=7 column=1\n"
      "  node 2 kind=call_constraint label=\"/main::call#0\" scope=\"/main\" path=\"/leaf\" line=8 column=10\n"
      "  edge 0 kind=dependency source=2 target=0\n"
      "  edge 1 kind=dependency source=1 target=2\n"
      "}\n";

  const std::string dump = requireTypeResolutionGraphDump(source, "/main");
  const std::string header = "type_graph {\n";
  REQUIRE(dump.rfind(header, 0) == 0);
  const size_t metricsStart = header.size();
  const size_t metricsEnd = dump.find('\n', metricsStart);
  REQUIRE(metricsEnd != std::string::npos);
  const std::string metricsLine = dump.substr(metricsStart, metricsEnd - metricsStart);
  CHECK(metricsLine.rfind("  metrics ", 0) == 0);
  CHECK(metricsLine.find("nodes=3") != std::string::npos);
  CHECK(metricsLine.find("edges=2") != std::string::npos);
  CHECK(metricsLine.find("prepare_over=") != std::string::npos);
  CHECK(metricsLine.find("build_over=") != std::string::npos);
  CHECK(metricsLine.find("nodes_definition_return=2") != std::string::npos);
  CHECK(metricsLine.find("nodes_call_constraint=1") != std::string::npos);
  CHECK(metricsLine.find("nodes_local_auto=0") != std::string::npos);
  CHECK(metricsLine.find("edges_dependency=2") != std::string::npos);
  CHECK(metricsLine.find("edges_requirement=0") != std::string::npos);
  CHECK(metricsLine.find("scc_count=3") != std::string::npos);
  CHECK(metricsLine.find("scc_max_size=1") != std::string::npos);
  CHECK(metricsLine.find("invalidation_local_binding=") != std::string::npos);
  CHECK(metricsLine.find("invalidation_control_flow=") != std::string::npos);
  CHECK(metricsLine.find("invalidation_initializer_shape=") != std::string::npos);
  CHECK(metricsLine.find("invalidation_definition_signature=") != std::string::npos);
  CHECK(metricsLine.find("invalidation_import_alias=") != std::string::npos);
  CHECK(metricsLine.find("invalidation_receiver_type=") != std::string::npos);
  const std::string stripped = header + dump.substr(metricsEnd + 1);
  CHECK(stripped == expected);
}

TEST_CASE("type resolution graph dump stays stable for mutual recursion") {
  const std::string source =
      "[return<auto>]\n"
      "alpha() {\n"
      "  return(beta())\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "beta() {\n"
      "  return(alpha())\n"
      "}\n";

  const std::string expected =
      "type_graph {\n"
      "  node 0 kind=definition_return label=\"/alpha\" scope=\"/alpha\" path=\"/alpha\" line=2 column=1\n"
      "  node 1 kind=definition_return label=\"/beta\" scope=\"/beta\" path=\"/beta\" line=7 column=1\n"
      "  node 2 kind=call_constraint label=\"/alpha::call#0\" scope=\"/alpha\" path=\"/beta\" line=3 column=10\n"
      "  node 3 kind=call_constraint label=\"/beta::call#0\" scope=\"/beta\" path=\"/alpha\" line=8 column=10\n"
      "  edge 0 kind=dependency source=2 target=1\n"
      "  edge 1 kind=dependency source=0 target=2\n"
      "  edge 2 kind=dependency source=3 target=0\n"
      "  edge 3 kind=dependency source=1 target=3\n"
      "}\n";

  CHECK(requireTypeResolutionGraphDump(source, "/alpha") == expected);
}

TEST_CASE("type resolution graph dump stays stable for namespace import resolution") {
  const std::string source =
      "import /pkg/leaf\n"
      "\n"
      "[public return<auto>]\n"
      "/pkg/leaf() {\n"
      "  return(1i32)\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "main() {\n"
      "  return(leaf())\n"
      "}\n";

  const std::string expected =
      "type_graph {\n"
      "  node 0 kind=definition_return label=\"/pkg/leaf\" scope=\"/pkg/leaf\" path=\"/pkg/leaf\" line=4 column=1\n"
      "  node 1 kind=definition_return label=\"/main\" scope=\"/main\" path=\"/main\" line=9 column=1\n"
      "  node 2 kind=call_constraint label=\"/main::call#0\" scope=\"/main\" path=\"/pkg/leaf\" line=10 column=10\n"
      "  edge 0 kind=dependency source=2 target=0\n"
      "  edge 1 kind=dependency source=1 target=2\n"
      "}\n";

  CHECK(requireTypeResolutionGraphDump(source, "/main") == expected);
}

TEST_CASE("type resolution graph dump stays stable for template specialization expansion") {
  const std::string source =
      "[return<auto>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "main() {\n"
      "  [auto] value{id(1i32)}\n"
      "  return(value)\n"
      "}\n";

  const std::string expected =
      "type_graph {\n"
      "  node 0 kind=definition_return label=\"/id__t25a78a513414c3bf\" scope=\"/id__t25a78a513414c3bf\" "
      "path=\"/id__t25a78a513414c3bf\" line=2 column=1\n"
      "  node 1 kind=definition_return label=\"/main\" scope=\"/main\" path=\"/main\" line=7 column=1\n"
      "  node 2 kind=call_constraint label=\"/main::call#0\" scope=\"/main\" path=\"/id__t25a78a513414c3bf\" "
      "line=8 column=16\n"
      "  node 3 kind=local_auto label=\"/main::auto:value#0\" scope=\"/main\" path=\"\" line=8 column=16\n"
      "  edge 0 kind=dependency source=2 target=0\n"
      "  edge 1 kind=dependency source=3 target=2\n"
      "}\n";

  CHECK(requireTypeResolutionGraphDump(source, "/main") == expected);
}

TEST_CASE("type resolution call binding snapshot shares template-specialization preparation") {
  const std::string source =
      "[return<auto>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "main() {\n"
      "  [i32] value{id(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionCallBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &entry = requireCallBindingSnapshotEntry(snapshot, "/main", "/id__t25a78a513414c3bf");
  CHECK(entry.bindingTypeText == "i32");
}

TEST_CASE("type resolution local binding snapshot keeps shared try metadata after local flow") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<int, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] branch{
    if(true,
      then(){ return(1i32) },
      else(){ return(2i32) })
  }
  [auto] selected{try(lookup())}
  return(Result.ok(plus(branch, selected)))
}
)";

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &selectedEntry = requireLocalBindingSnapshotEntry(snapshot, "/main", "selected");
  CHECK(selectedEntry.initializerResolvedPath == "/lookup");
  CHECK(selectedEntry.initializerHasTry);
  CHECK(selectedEntry.initializerTryOperandResolvedPath == "/lookup");
  CHECK(selectedEntry.initializerTryOnErrorHandlerPath == "/unexpectedError");
  CHECK(!selectedEntry.initializerTryOperandQueryTypeText.empty());
  CHECK(!selectedEntry.initializerTryValueTypeText.empty());
}

TEST_CASE("type resolution local try metadata stays aligned with try snapshot metadata") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<int, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)";

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot localSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, localSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionTryValueSnapshot trySnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionTryValueSnapshotForTesting(
      parseProgram(source), "/main", error, trySnapshot));
  CHECK(error.empty());

  const auto &localEntry = requireLocalBindingSnapshotEntry(localSnapshot, "/main", "selected");
  const auto &tryEntry = requireTryValueSnapshotEntry(trySnapshot, "/main", "/lookup");
  CHECK(localEntry.initializerHasTry);
  CHECK(localEntry.initializerTryOperandResolvedPath == tryEntry.operandResolvedPath);
  CHECK(localEntry.initializerTryOperandBindingTypeText == tryEntry.operandBindingTypeText);
  CHECK(localEntry.initializerTryOperandReceiverBindingTypeText == tryEntry.operandReceiverBindingTypeText);
  CHECK(localEntry.initializerTryOperandQueryTypeText == tryEntry.operandQueryTypeText);
  CHECK(localEntry.initializerTryValueTypeText == tryEntry.valueTypeText);
  CHECK(localEntry.initializerTryErrorTypeText == tryEntry.errorTypeText);
  CHECK(localEntry.initializerTryContextReturnKindText == tryEntry.contextReturnKindText);
  CHECK(localEntry.initializerTryOnErrorHandlerPath == tryEntry.onErrorHandlerPath);
  CHECK(localEntry.initializerTryOnErrorErrorTypeText == tryEntry.onErrorErrorTypeText);
  CHECK(localEntry.initializerTryOnErrorBoundArgCount == tryEntry.onErrorBoundArgCount);
}

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

namespace {

const primec::semantics::TypeResolutionGraphSnapshotNode &requireGraphNode(
    const primec::semantics::TypeResolutionGraphSnapshot &graph,
    uint32_t id) {
  return graph.nodes.at(id);
}

const primec::semantics::TypeResolutionGraphSnapshotEdge &requireGraphEdge(
    const primec::semantics::TypeResolutionGraphSnapshot &graph,
    uint32_t index) {
  return graph.edges.at(index);
}

} // namespace

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
  CHECK(requireGraphNode(graph, 4).kind == "local_auto");
  CHECK(requireGraphNode(graph, 4).label == "/main::auto:value#0");
  CHECK(requireGraphNode(graph, 5).kind == "call_constraint");
  CHECK(requireGraphNode(graph, 5).label == "/main::call#0");
  CHECK(requireGraphNode(graph, 5).resolvedPath == "/mid");

  REQUIRE(graph.edges.size() == 4);
  CHECK(requireGraphEdge(graph, 0).kind == "dependency");
  CHECK(requireGraphEdge(graph, 0).sourceId == 3);
  CHECK(requireGraphEdge(graph, 0).targetId == 0);
  CHECK(requireGraphEdge(graph, 1).kind == "dependency");
  CHECK(requireGraphEdge(graph, 1).sourceId == 1);
  CHECK(requireGraphEdge(graph, 1).targetId == 3);
  CHECK(requireGraphEdge(graph, 2).kind == "dependency");
  CHECK(requireGraphEdge(graph, 2).sourceId == 5);
  CHECK(requireGraphEdge(graph, 2).targetId == 1);
  CHECK(requireGraphEdge(graph, 3).kind == "dependency");
  CHECK(requireGraphEdge(graph, 3).sourceId == 4);
  CHECK(requireGraphEdge(graph, 3).targetId == 5);
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

TEST_SUITE_END();

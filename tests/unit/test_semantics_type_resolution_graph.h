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

std::string requireTypeResolutionGraphDump(const std::string &source, const std::string &entryPath) {
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

const primec::semantics::TypeResolutionLocalBindingSnapshotEntry &requireLocalBindingSnapshotEntry(
    const primec::semantics::TypeResolutionLocalBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &bindingName) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.bindingName == bindingName;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

const primec::semantics::TypeResolutionTryValueSnapshotEntry &requireTryValueSnapshotEntry(
    const primec::semantics::TypeResolutionTryValueSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &operandResolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.operandResolvedPath == operandResolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

const primec::semantics::TypeResolutionQueryCallSnapshotEntry &requireQueryCallSnapshotEntry(
    const primec::semantics::TypeResolutionQueryCallSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

const primec::semantics::TypeResolutionQueryBindingSnapshotEntry &requireQueryBindingSnapshotEntry(
    const primec::semantics::TypeResolutionQueryBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

const primec::semantics::TypeResolutionQueryResultTypeSnapshotEntry &requireQueryResultTypeSnapshotEntry(
    const primec::semantics::TypeResolutionQueryResultTypeSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

const primec::semantics::TypeResolutionQueryReceiverBindingSnapshotEntry &requireQueryReceiverBindingSnapshotEntry(
    const primec::semantics::TypeResolutionQueryReceiverBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

primec::semantics::TypeResolutionCallBindingSnapshotEntry requireCallBindingSnapshotEntry(
    const primec::semantics::TypeResolutionCallBindingSnapshot &snapshot,
    const std::string &scopePath,
    const std::string &resolvedPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.scopePath == scopePath && entry.resolvedPath == resolvedPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
}

size_t requireTopologicalComponentIndex(const primec::semantics::CondensationDagSnapshot &dag, uint32_t componentId) {
  const auto it = std::find(dag.topologicalComponentIds.begin(), dag.topologicalComponentIds.end(), componentId);
  REQUIRE(it != dag.topologicalComponentIds.end());
  return static_cast<size_t>(std::distance(dag.topologicalComponentIds.begin(), it));
}

bool hasCondensationEdge(const primec::semantics::CondensationDagSnapshot &dag,
                         uint32_t sourceComponentId,
                         uint32_t targetComponentId) {
  return std::find_if(dag.edges.begin(), dag.edges.end(), [&](const auto &edge) {
           return edge.sourceComponentId == sourceComponentId &&
                  edge.targetComponentId == targetComponentId;
         }) != dag.edges.end();
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

  CHECK(requireTypeResolutionGraphDump(source, "/main") == expected);
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
import /std/collections/*
import /std/collections/experimental_map/*

[return<void>]
unexpectedError([ContainerError] err) {
}

[return<Result<int, ContainerError>> effects(heap_alloc) on_error<ContainerError, /unexpectedError>]
main() {
  [Map<string, i32>] values{/std/collections/mapPair("left"raw_utf8, 4i32)}
  [auto] branch{
    if(true,
      then(){ return(1i32) },
      else(){ return(2i32) })
  }
  [auto] selected{try(values.tryAt("left"raw_utf8))}
  return(Result.ok(plus(branch, selected)))
}
)";

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &branchEntry = requireLocalBindingSnapshotEntry(snapshot, "/main", "branch");
  CHECK(branchEntry.bindingTypeText == "i32");
  CHECK(!branchEntry.initializerHasTry);
  CHECK(branchEntry.initializerTryOperandResolvedPath.empty());
  CHECK(branchEntry.initializerTryOperandBindingTypeText.empty());
  CHECK(branchEntry.initializerTryValueTypeText.empty());
  CHECK(branchEntry.initializerTryErrorTypeText.empty());
  CHECK(branchEntry.initializerTryContextReturnKindText.empty());
  CHECK(branchEntry.initializerTryOnErrorHandlerPath.empty());
  CHECK(branchEntry.initializerTryOnErrorErrorTypeText.empty());
  CHECK(branchEntry.initializerTryOnErrorBoundArgCount == 0);

  const auto &selectedEntry = requireLocalBindingSnapshotEntry(snapshot, "/main", "selected");
  CHECK(selectedEntry.bindingTypeText == "i32");
  CHECK(selectedEntry.initializerResolvedPath == "/std/collections/map/tryAt");
  CHECK(selectedEntry.initializerBindingTypeText == "Result<i32, ContainerError>");
  CHECK(selectedEntry.initializerReceiverBindingTypeText == "Map<string, i32>");
  CHECK(selectedEntry.initializerQueryTypeText == "Result<i32, ContainerError>");
  CHECK(selectedEntry.initializerResultHasValue);
  CHECK(selectedEntry.initializerResultValueTypeText == "i32");
  CHECK(selectedEntry.initializerResultErrorTypeText == "ContainerError");
  CHECK(selectedEntry.initializerHasTry);
  CHECK(selectedEntry.initializerTryOperandResolvedPath == "/std/collections/map/tryAt");
  CHECK(selectedEntry.initializerTryOperandBindingTypeText == "Result<i32, ContainerError>");
  CHECK(selectedEntry.initializerTryValueTypeText == "i32");
  CHECK(selectedEntry.initializerTryErrorTypeText == "ContainerError");
  CHECK(selectedEntry.initializerTryContextReturnKindText == "array");
  CHECK(selectedEntry.initializerTryOnErrorHandlerPath == "/unexpectedError");
  CHECK(selectedEntry.initializerTryOnErrorErrorTypeText == "ContainerError");
  CHECK(selectedEntry.initializerTryOnErrorBoundArgCount == 1);
}

TEST_CASE("type resolution local try metadata stays aligned with try snapshot metadata") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<void>]
unexpectedError([ContainerError] err) {
}

[return<Result<int, ContainerError>> effects(heap_alloc) on_error<ContainerError, /unexpectedError>]
main() {
  [Map<string, i32>] values{/std/collections/mapPair("left"raw_utf8, 4i32)}
  [auto] selected{try(values.tryAt("left"raw_utf8))}
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
  const auto &tryEntry = requireTryValueSnapshotEntry(trySnapshot, "/main", "/std/collections/map/tryAt");
  CHECK(localEntry.initializerHasTry);
  CHECK(localEntry.initializerTryOperandResolvedPath == tryEntry.operandResolvedPath);
  CHECK(localEntry.initializerTryOperandBindingTypeText == tryEntry.operandBindingTypeText);
  CHECK(localEntry.initializerTryValueTypeText == tryEntry.valueTypeText);
  CHECK(localEntry.initializerTryErrorTypeText == tryEntry.errorTypeText);
  CHECK(localEntry.initializerTryContextReturnKindText == tryEntry.contextReturnKindText);
  CHECK(localEntry.initializerTryOnErrorHandlerPath == tryEntry.onErrorHandlerPath);
  CHECK(localEntry.initializerTryOnErrorErrorTypeText == tryEntry.onErrorErrorTypeText);
  CHECK(localEntry.initializerTryOnErrorBoundArgCount == tryEntry.onErrorBoundArgCount);
}

TEST_CASE("type resolution try operand call metadata stays aligned with call snapshot") {
  const std::string source = R"(
[return<void>]
unexpectedError([MyError] err) {
}

MyError() {
}

[return<Result<int, MyError>>]
makeValue() {
  return(Result.ok(4i32))
}

[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(makeValue())}
  return(Result.ok(selected))
}
)";

  std::string error;
  primec::semantics::TypeResolutionTryValueSnapshot trySnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionTryValueSnapshotForTesting(
      parseProgram(source), "/main", error, trySnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionCallBindingSnapshot callSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, callSnapshot));
  CHECK(error.empty());

  const auto &tryEntry = requireTryValueSnapshotEntry(trySnapshot, "/main", "/makeValue");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/makeValue");
  CHECK(tryEntry.operandResolvedPath == callEntry.resolvedPath);
  CHECK(tryEntry.operandBindingTypeText == callEntry.bindingTypeText);
  CHECK(tryEntry.valueTypeText == "i32");
  CHECK(tryEntry.errorTypeText == "MyError");
}

TEST_CASE("type resolution local query metadata stays aligned with query snapshots") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [Map<string, i32>] values{/std/collections/mapPair("left"raw_utf8, 4i32)}
  [auto] selected{try(values.tryAt("left"raw_utf8))}
  return(Result.ok(selected))
}
)";

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot localSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, localSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryCallSnapshot queryCallSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, queryCallSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryBindingSnapshot queryBindingSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryBindingSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryResultTypeSnapshot queryResultSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
      parseProgram(source), "/main", error, queryResultSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryReceiverBindingSnapshot queryReceiverSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryReceiverBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryReceiverSnapshot));
  CHECK(error.empty());

  const auto &localEntry = requireLocalBindingSnapshotEntry(localSnapshot, "/main", "selected");
  const auto &callEntry = requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/std/collections/map/tryAt");
  const auto &bindingEntry =
      requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/std/collections/map/tryAt");
  const auto &resultEntry =
      requireQueryResultTypeSnapshotEntry(queryResultSnapshot, "/main", "/std/collections/map/tryAt");
  const auto &receiverEntry =
      requireQueryReceiverBindingSnapshotEntry(queryReceiverSnapshot, "/main", "/std/collections/map/tryAt");

  CHECK(localEntry.initializerResolvedPath == callEntry.resolvedPath);
  CHECK(localEntry.initializerBindingTypeText == bindingEntry.bindingTypeText);
  CHECK(localEntry.initializerQueryTypeText == callEntry.typeText);
  CHECK(localEntry.initializerReceiverBindingTypeText == receiverEntry.receiverBindingTypeText);
  CHECK(localEntry.initializerResultHasValue == resultEntry.hasValue);
  CHECK(localEntry.initializerResultValueTypeText == resultEntry.valueTypeText);
  CHECK(localEntry.initializerResultErrorTypeText == resultEntry.errorTypeText);
}

TEST_CASE("type resolution local call metadata stays aligned with call snapshot") {
  const std::string source =
      "[return<auto>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  return(selected)\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot localSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, localSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionCallBindingSnapshot callSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, callSnapshot));
  CHECK(error.empty());

  const auto &localEntry = requireLocalBindingSnapshotEntry(localSnapshot, "/main", "selected");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id__t25a78a513414c3bf");
  CHECK(localEntry.initializerResolvedPath == callEntry.resolvedPath);
  CHECK(localEntry.initializerBindingTypeText == callEntry.bindingTypeText);
  CHECK(localEntry.initializerReceiverBindingTypeText.empty());
  CHECK(localEntry.initializerQueryTypeText.empty());
  CHECK(!localEntry.initializerResultHasValue);
}

TEST_SUITE_END();

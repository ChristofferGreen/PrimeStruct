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

const primec::semantics::TypeResolutionReturnSnapshotEntry &requireReturnSnapshotEntry(
    const primec::semantics::TypeResolutionReturnSnapshot &snapshot,
    const std::string &definitionPath) {
  const auto it = std::find_if(snapshot.entries.begin(), snapshot.entries.end(), [&](const auto &entry) {
    return entry.definitionPath == definitionPath;
  });
  REQUIRE(it != snapshot.entries.end());
  return *it;
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

const primec::semantics::TypeResolutionCallBindingSnapshotEntry &requireCallBindingSnapshotEntry(
    const primec::semantics::TypeResolutionCallBindingSnapshot &snapshot,
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

TEST_CASE("type resolution return snapshot keeps inferred experimental map binding metadata") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<int> effects(heap_alloc)]
main() {
  [auto] values{buildValues()}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  primec::semantics::TypeResolutionReturnSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionReturnSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &entry = requireReturnSnapshotEntry(snapshot, "/buildValues");
  CHECK(entry.returnKind == "array");
  CHECK(entry.structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0);
  CHECK(entry.bindingTypeText == "Map<string, i32>");
}

TEST_CASE("type resolution local binding snapshot keeps control-flow collection binding metadata") {
  const std::string source = R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32, 5i32))
}

[return<i32>]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(count(values))
}
)";
  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &entry = requireLocalBindingSnapshotEntry(snapshot, "/main", "values");
  CHECK(entry.bindingTypeText == "array<i32>");
  CHECK(entry.initializerQueryTypeText == "array<i32>");
  CHECK(!entry.initializerResultHasValue);
  CHECK(entry.initializerResultValueTypeText.empty());
  CHECK(entry.initializerResultErrorTypeText.empty());
}

TEST_CASE("type resolution local binding snapshot keeps local result metadata") {
  const std::string source = R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<auto> effects(heap_alloc)]
main() {
  [auto] selected{/std/collections/map/tryAt(selectValues(), "left"raw_utf8)}
  return(selected)
}
)";
  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &entry = requireLocalBindingSnapshotEntry(snapshot, "/main", "selected");
  CHECK(entry.bindingTypeText == "Result<i32, ContainerError>");
  CHECK(entry.initializerQueryTypeText == "Result<i32, ContainerError>");
  CHECK(entry.initializerResultHasValue);
  CHECK(entry.initializerResultValueTypeText == "i32");
  CHECK(entry.initializerResultErrorTypeText == "ContainerError");
}

TEST_CASE("type resolution query call snapshot keeps map receiver query metadata") {
  const std::string source = R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [i32] total{plus(selectValues().count(), try(selectValues().tryAt("left"raw_utf8)))}
  return(Result.ok(total))
}
)";
  std::string error;
  primec::semantics::TypeResolutionQueryCallSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &countEntry = requireQueryCallSnapshotEntry(snapshot, "/main", "/std/collections/map/count");
  CHECK(countEntry.typeText == "i32");

  const auto &tryAtEntry = requireQueryCallSnapshotEntry(snapshot, "/main", "/std/collections/map/tryAt");
  CHECK(tryAtEntry.typeText == "Result<i32, ContainerError>");
}

TEST_CASE("type resolution query binding snapshot keeps map query result bindings") {
  const std::string source = R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [i32] total{plus(selectValues().count(), try(selectValues().tryAt("left"raw_utf8)))}
  return(Result.ok(total))
}
)";
  std::string error;
  primec::semantics::TypeResolutionQueryBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &countEntry = requireQueryBindingSnapshotEntry(snapshot, "/main", "/std/collections/map/count");
  CHECK(countEntry.bindingTypeText == "i32");

  const auto &tryAtEntry = requireQueryBindingSnapshotEntry(snapshot, "/main", "/std/collections/map/tryAt");
  CHECK(tryAtEntry.bindingTypeText == "Result<i32, ContainerError>");
}

TEST_CASE("type resolution query result snapshot keeps map try result metadata") {
  const std::string source = R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [i32] total{plus(selectValues().count(), try(selectValues().tryAt("left"raw_utf8)))}
  return(Result.ok(total))
}
)";
  std::string error;
  primec::semantics::TypeResolutionQueryResultTypeSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &tryAtEntry =
      requireQueryResultTypeSnapshotEntry(snapshot, "/main", "/std/collections/map/tryAt");
  CHECK(tryAtEntry.hasValue);
  CHECK(tryAtEntry.valueTypeText == "i32");
  CHECK(tryAtEntry.errorTypeText == "ContainerError");
}

TEST_CASE("type resolution call binding snapshot keeps helper-returned map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<auto> effects(heap_alloc)]
wrapValues([Map<string, i32>] values) {
  return(values)
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [auto] built{buildValues()}
  [Map<string, i32>] values{wrapValues(built)}
  return(Result.ok(0i32))
}
)";
  std::string error;
  primec::semantics::TypeResolutionCallBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &buildEntry = requireCallBindingSnapshotEntry(snapshot, "/main", "/buildValues");
  CHECK(buildEntry.bindingTypeText == "Map<string, i32>");

  const auto &wrapEntry = requireCallBindingSnapshotEntry(snapshot, "/main", "/wrapValues");
  CHECK(wrapEntry.bindingTypeText == "Map<string, i32>");
}

TEST_CASE("type resolution query receiver binding snapshot keeps wrapped map receiver bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
wrapValues([Map<string, i32>] values) {
  return(values)
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [auto] values{wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  [i32] total{plus(values.count(), try(values.tryAt("left"raw_utf8)))}
  return(Result.ok(total))
}
)";
  std::string error;
  primec::semantics::TypeResolutionQueryReceiverBindingSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryReceiverBindingSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &countEntry =
      requireQueryReceiverBindingSnapshotEntry(snapshot, "/main", "/std/collections/map/count");
  CHECK(countEntry.receiverBindingTypeText == "Map<string, i32>");

  const auto &tryAtEntry =
      requireQueryReceiverBindingSnapshotEntry(snapshot, "/main", "/std/collections/map/tryAt");
  CHECK(tryAtEntry.receiverBindingTypeText == "Map<string, i32>");
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

TEST_CASE("type resolution return snapshot shares template-specialization preparation") {
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

  std::string error;
  primec::semantics::TypeResolutionReturnSnapshot snapshot;
  REQUIRE(primec::semantics::computeTypeResolutionReturnSnapshotForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  const auto &specialized = requireReturnSnapshotEntry(snapshot, "/id__t25a78a513414c3bf");
  CHECK(specialized.returnKind == "i32");
  CHECK(specialized.bindingTypeText == "i32");

  const auto &mainEntry = requireReturnSnapshotEntry(snapshot, "/main");
  CHECK(mainEntry.returnKind == "i32");
  CHECK(mainEntry.bindingTypeText == "i32");
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

TEST_SUITE_END();

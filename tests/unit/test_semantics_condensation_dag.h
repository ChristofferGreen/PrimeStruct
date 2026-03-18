TEST_SUITE_BEGIN("primestruct.semantics.condensation_dag");

TEST_CASE("condensation dag preserves acyclic chain order") {
  const primec::semantics::CondensationDagSnapshot dag =
      primec::semantics::computeCondensationDagForTesting(
          4, {{0, 1}, {1, 2}, {2, 3}});

  REQUIRE(dag.nodes.size() == 4);
  CHECK(dag.nodes[0].memberNodeIds == std::vector<uint32_t>{0});
  CHECK(dag.nodes[0].incomingComponentIds.empty());
  CHECK(dag.nodes[0].outgoingComponentIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[1].memberNodeIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[1].incomingComponentIds == std::vector<uint32_t>{0});
  CHECK(dag.nodes[1].outgoingComponentIds == std::vector<uint32_t>{2});
  CHECK(dag.nodes[2].memberNodeIds == std::vector<uint32_t>{2});
  CHECK(dag.nodes[2].incomingComponentIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[2].outgoingComponentIds == std::vector<uint32_t>{3});
  CHECK(dag.nodes[3].memberNodeIds == std::vector<uint32_t>{3});
  CHECK(dag.nodes[3].incomingComponentIds == std::vector<uint32_t>{2});
  CHECK(dag.nodes[3].outgoingComponentIds.empty());

  REQUIRE(dag.edges.size() == 3);
  CHECK(dag.edges[0].sourceComponentId == 0);
  CHECK(dag.edges[0].targetComponentId == 1);
  CHECK(dag.edges[1].sourceComponentId == 1);
  CHECK(dag.edges[1].targetComponentId == 2);
  CHECK(dag.edges[2].sourceComponentId == 2);
  CHECK(dag.edges[2].targetComponentId == 3);
  CHECK(dag.topologicalComponentIds == std::vector<uint32_t>({0, 1, 2, 3}));
  CHECK(dag.componentIdByNodeId == std::vector<uint32_t>({0, 1, 2, 3}));
}

TEST_CASE("condensation dag collapses cycles and deduplicates cross-component edges") {
  const primec::semantics::CondensationDagSnapshot dag =
      primec::semantics::computeCondensationDagForTesting(
          5, {{0, 1}, {1, 0}, {0, 2}, {1, 2}, {1, 2}, {2, 3}, {3, 4}, {4, 3}});

  REQUIRE(dag.nodes.size() == 3);
  CHECK(dag.nodes[0].memberNodeIds == std::vector<uint32_t>({0, 1}));
  CHECK(dag.nodes[0].incomingComponentIds.empty());
  CHECK(dag.nodes[0].outgoingComponentIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[1].memberNodeIds == std::vector<uint32_t>{2});
  CHECK(dag.nodes[1].incomingComponentIds == std::vector<uint32_t>{0});
  CHECK(dag.nodes[1].outgoingComponentIds == std::vector<uint32_t>{2});
  CHECK(dag.nodes[2].memberNodeIds == std::vector<uint32_t>({3, 4}));
  CHECK(dag.nodes[2].incomingComponentIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[2].outgoingComponentIds.empty());

  REQUIRE(dag.edges.size() == 2);
  CHECK(dag.edges[0].sourceComponentId == 0);
  CHECK(dag.edges[0].targetComponentId == 1);
  CHECK(dag.edges[1].sourceComponentId == 1);
  CHECK(dag.edges[1].targetComponentId == 2);
  CHECK(dag.topologicalComponentIds == std::vector<uint32_t>({0, 1, 2}));
  CHECK(dag.componentIdByNodeId == std::vector<uint32_t>({0, 0, 1, 2, 2}));
}

TEST_CASE("condensation dag topological order stays deterministic across disconnected roots") {
  const primec::semantics::CondensationDagSnapshot dag =
      primec::semantics::computeCondensationDagForTesting(
          7, {{0, 1}, {1, 0}, {0, 4}, {2, 3}, {2, 5}, {4, 6}, {5, 6}});

  REQUIRE(dag.nodes.size() == 6);
  CHECK(dag.nodes[0].memberNodeIds == std::vector<uint32_t>({0, 1}));
  CHECK(dag.nodes[0].outgoingComponentIds == std::vector<uint32_t>{3});
  CHECK(dag.nodes[1].memberNodeIds == std::vector<uint32_t>{2});
  CHECK(dag.nodes[1].outgoingComponentIds == std::vector<uint32_t>({2, 4}));
  CHECK(dag.nodes[2].memberNodeIds == std::vector<uint32_t>{3});
  CHECK(dag.nodes[2].incomingComponentIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[2].outgoingComponentIds.empty());
  CHECK(dag.nodes[3].memberNodeIds == std::vector<uint32_t>{4});
  CHECK(dag.nodes[3].incomingComponentIds == std::vector<uint32_t>{0});
  CHECK(dag.nodes[3].outgoingComponentIds == std::vector<uint32_t>{5});
  CHECK(dag.nodes[4].memberNodeIds == std::vector<uint32_t>{5});
  CHECK(dag.nodes[4].incomingComponentIds == std::vector<uint32_t>{1});
  CHECK(dag.nodes[4].outgoingComponentIds == std::vector<uint32_t>{5});
  CHECK(dag.nodes[5].memberNodeIds == std::vector<uint32_t>{6});
  CHECK(dag.nodes[5].incomingComponentIds == std::vector<uint32_t>({3, 4}));
  CHECK(dag.nodes[5].outgoingComponentIds.empty());

  REQUIRE(dag.edges.size() == 5);
  CHECK(dag.edges[0].sourceComponentId == 0);
  CHECK(dag.edges[0].targetComponentId == 3);
  CHECK(dag.edges[1].sourceComponentId == 1);
  CHECK(dag.edges[1].targetComponentId == 2);
  CHECK(dag.edges[2].sourceComponentId == 1);
  CHECK(dag.edges[2].targetComponentId == 4);
  CHECK(dag.edges[3].sourceComponentId == 3);
  CHECK(dag.edges[3].targetComponentId == 5);
  CHECK(dag.edges[4].sourceComponentId == 4);
  CHECK(dag.edges[4].targetComponentId == 5);
  CHECK(dag.topologicalComponentIds == std::vector<uint32_t>({0, 1, 2, 3, 4, 5}));
  CHECK(dag.componentIdByNodeId == std::vector<uint32_t>({0, 0, 1, 2, 3, 4, 5}));
}

TEST_SUITE_END();

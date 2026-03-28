#include "primec/testing/SemanticsValidationHelpers.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.scc");

TEST_CASE("strongly connected components keep acyclic nodes separate in node order") {
  const primec::semantics::StronglyConnectedComponentsSnapshot components =
      primec::semantics::computeStronglyConnectedComponentsForTesting(
          4, {{0, 1}, {1, 2}, {2, 3}});

  REQUIRE(components.components.size() == 4);
  CHECK(components.components[0].nodeIds == std::vector<uint32_t>{0});
  CHECK(components.components[1].nodeIds == std::vector<uint32_t>{1});
  CHECK(components.components[2].nodeIds == std::vector<uint32_t>{2});
  CHECK(components.components[3].nodeIds == std::vector<uint32_t>{3});
  CHECK(components.componentIdByNodeId == std::vector<uint32_t>({0, 1, 2, 3}));
}

TEST_CASE("strongly connected components merge a single cycle into one component") {
  const primec::semantics::StronglyConnectedComponentsSnapshot components =
      primec::semantics::computeStronglyConnectedComponentsForTesting(
          4, {{0, 1}, {1, 2}, {2, 0}, {2, 3}});

  REQUIRE(components.components.size() == 2);
  CHECK(components.components[0].nodeIds == std::vector<uint32_t>({0, 1, 2}));
  CHECK(components.components[1].nodeIds == std::vector<uint32_t>{3});
  CHECK(components.componentIdByNodeId == std::vector<uint32_t>({0, 0, 0, 1}));
}

TEST_CASE("strongly connected components keep multiple cycles in deterministic component order") {
  const primec::semantics::StronglyConnectedComponentsSnapshot components =
      primec::semantics::computeStronglyConnectedComponentsForTesting(
          6, {{0, 1}, {1, 0}, {1, 2}, {2, 3}, {3, 2}, {3, 4}, {4, 5}, {5, 4}});

  REQUIRE(components.components.size() == 3);
  CHECK(components.components[0].nodeIds == std::vector<uint32_t>({0, 1}));
  CHECK(components.components[1].nodeIds == std::vector<uint32_t>({2, 3}));
  CHECK(components.components[2].nodeIds == std::vector<uint32_t>({4, 5}));
  CHECK(components.componentIdByNodeId == std::vector<uint32_t>({0, 0, 1, 1, 2, 2}));
}

TEST_CASE("strongly connected components stay deterministic across disconnected subgraphs") {
  const primec::semantics::StronglyConnectedComponentsSnapshot components =
      primec::semantics::computeStronglyConnectedComponentsForTesting(
          7, {{0, 1}, {1, 0}, {2, 3}, {4, 5}, {5, 6}, {6, 4}});

  REQUIRE(components.components.size() == 4);
  CHECK(components.components[0].nodeIds == std::vector<uint32_t>({0, 1}));
  CHECK(components.components[1].nodeIds == std::vector<uint32_t>{2});
  CHECK(components.components[2].nodeIds == std::vector<uint32_t>{3});
  CHECK(components.components[3].nodeIds == std::vector<uint32_t>({4, 5, 6}));
  CHECK(components.componentIdByNodeId == std::vector<uint32_t>({0, 0, 1, 2, 3, 3, 3}));
}

TEST_SUITE_END();

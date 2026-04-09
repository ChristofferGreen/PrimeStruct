#include "third_party/doctest.h"

#include "primec/SemanticsDefinitionPartitioner.h"
#include "primec/SemanticsDefinitionPrepass.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.definition_partitioner");

namespace {

std::vector<std::size_t> flattenStableIndices(
    const std::vector<primec::semantics::DefinitionPartitionChunk> &partitions) {
  std::vector<std::size_t> indices;
  for (const auto &partition : partitions) {
    indices.insert(indices.end(),
                   partition.declarationStableIndices.begin(),
                   partition.declarationStableIndices.end());
  }
  return indices;
}

} // namespace

TEST_CASE("definition partitioner emits deterministic balanced contiguous chunks") {
  const std::string source = R"(
[return<void>]
a() {}

[return<void>]
b() {}

[return<void>]
c() {}

[return<void>]
d() {}

[return<void>]
e() {}

[return<void>]
f() {}

[return<void>]
g() {}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);
  const std::vector<primec::semantics::DefinitionPartitionChunk> partitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 3);

  REQUIRE(partitions.size() == 3);
  CHECK(partitions[0].partitionKey == 0);
  CHECK(partitions[1].partitionKey == 1);
  CHECK(partitions[2].partitionKey == 2);
  CHECK(partitions[0].declarationStableIndices ==
        std::vector<std::size_t>{0, 1, 2});
  CHECK(partitions[1].declarationStableIndices ==
        std::vector<std::size_t>{3, 4});
  CHECK(partitions[2].declarationStableIndices ==
        std::vector<std::size_t>{5, 6});
}

TEST_CASE("definition partitioner handles zero and oversized partition counts deterministically") {
  const std::string source = R"(
[return<void>]
left() {}

[return<void>]
right() {}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);

  const std::vector<primec::semantics::DefinitionPartitionChunk> zeroPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 0);
  CHECK(zeroPartitions.empty());

  const std::vector<primec::semantics::DefinitionPartitionChunk> oversizedPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 4);
  REQUIRE(oversizedPartitions.size() == 4);
  CHECK(oversizedPartitions[0].declarationStableIndices ==
        std::vector<std::size_t>{0});
  CHECK(oversizedPartitions[1].declarationStableIndices ==
        std::vector<std::size_t>{1});
  CHECK(oversizedPartitions[2].declarationStableIndices.empty());
  CHECK(oversizedPartitions[3].declarationStableIndices.empty());
}

TEST_CASE("definition partitioner repeat runs are stable and cover each declaration once") {
  const std::string source = R"(
[return<void>]
alpha() {}

[return<void>]
beta() {}

[return<void>]
alpha() {}

[return<void>]
gamma() {}

[return<void>]
delta() {}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);

  const std::vector<primec::semantics::DefinitionPartitionChunk> firstPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 3);
  const std::vector<primec::semantics::DefinitionPartitionChunk> secondPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 3);
  REQUIRE(firstPartitions.size() == secondPartitions.size());
  for (std::size_t i = 0; i < firstPartitions.size(); ++i) {
    CHECK(firstPartitions[i].partitionKey == secondPartitions[i].partitionKey);
    CHECK(firstPartitions[i].declarationStableIndices ==
          secondPartitions[i].declarationStableIndices);
  }

  std::vector<std::size_t> flattened = flattenStableIndices(firstPartitions);
  std::sort(flattened.begin(), flattened.end());
  CHECK(flattened == std::vector<std::size_t>{0, 1, 2, 3, 4});
}

TEST_SUITE_END();

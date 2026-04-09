#pragma once

#include "primec/SemanticsDefinitionPrepass.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace primec::semantics {

struct DefinitionPartitionChunk {
  uint32_t partitionKey = 0;
  std::vector<std::size_t> declarationStableIndices;
};

// Deterministically partitions definition declarations into N chunks.
// Chunks preserve stable declaration order and use balanced contiguous slices.
std::vector<DefinitionPartitionChunk>
partitionDefinitionsDeterministic(const DefinitionPrepassSnapshot &snapshot,
                                  std::size_t partitionCount);

} // namespace primec::semantics

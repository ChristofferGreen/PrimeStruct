#pragma once

#include "primec/SemanticsDefinitionPrepass.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace primec::semantics {

struct DefinitionPartitionChunk {
  uint32_t partitionKey = 0;
  std::size_t stableOrderOffset = 0;
  std::size_t stableOrderCount = 0;
};

// Deterministically partitions definition declarations into N chunks.
// Chunks preserve stable declaration order and use balanced contiguous slices.
std::vector<DefinitionPartitionChunk>
partitionDefinitionsDeterministic(const DefinitionPrepassSnapshot &snapshot,
                                  std::size_t partitionCount);

} // namespace primec::semantics

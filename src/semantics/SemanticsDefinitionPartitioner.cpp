#include "primec/SemanticsDefinitionPartitioner.h"

namespace primec::semantics {

std::vector<DefinitionPartitionChunk>
partitionDefinitionsDeterministic(const DefinitionPrepassSnapshot &snapshot,
                                  std::size_t partitionCount) {
  if (partitionCount == 0) {
    return {};
  }

  std::vector<DefinitionPartitionChunk> partitions;
  partitions.reserve(partitionCount);

  const std::size_t declarationCount = snapshot.declarationsInStableOrder.size();
  const std::size_t baseSize = declarationCount / partitionCount;
  const std::size_t extra = declarationCount % partitionCount;
  std::size_t nextStableIndex = 0;

  for (std::size_t partitionIndex = 0; partitionIndex < partitionCount; ++partitionIndex) {
    DefinitionPartitionChunk chunk;
    chunk.partitionKey = static_cast<uint32_t>(partitionIndex);
    const std::size_t chunkSize = baseSize + (partitionIndex < extra ? 1 : 0);
    chunk.declarationStableIndices.reserve(chunkSize);
    for (std::size_t i = 0; i < chunkSize; ++i) {
      chunk.declarationStableIndices.push_back(
          snapshot.declarationsInStableOrder[nextStableIndex].stableIndex);
      ++nextStableIndex;
    }
    partitions.push_back(std::move(chunk));
  }

  return partitions;
}

} // namespace primec::semantics

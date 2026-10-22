#include "primec/runtime/VmTextHeap.h"

#include <algorithm>
#include <utility>

namespace primec {

uint64_t VmTextHeap::blockSizeFor(uint64_t size) {
  if (size == 0) {
    return 0;
  }
  if (size > LargeThreshold) {
    return size;
  }
  if (size <= MinBlockBytes) {
    return MinBlockBytes;
  }
  // 32, 40, 48, 56, 64, 80, 96, 112, 128, ... : four sizes per doubling. Find the doubling
  // (base, 2 * base] that holds `size`, then round up to a quarter of `base`.
  uint64_t base = MinBlockBytes;
  while (base * 2 < size) {
    base *= 2;
  }
  const uint64_t step = base / 4;
  return base + (size - base + step - 1) / step * step;
}

std::map<uint64_t, VmTextHeap::Chunk>::iterator VmTextHeap::chunkContaining(uint64_t address) {
  const uint64_t offset = offsetOf(address);
  auto it = chunks_.upper_bound(offset);
  if (it == chunks_.begin()) {
    return chunks_.end();
  }
  --it;
  return offset < it->first + ChunkBytes ? it : chunks_.end();
}

std::map<uint64_t, VmTextHeap::Chunk>::const_iterator VmTextHeap::chunkContaining(uint64_t address) const {
  const uint64_t offset = offsetOf(address);
  auto it = chunks_.upper_bound(offset);
  if (it == chunks_.begin()) {
    return chunks_.end();
  }
  --it;
  return offset < it->first + ChunkBytes ? it : chunks_.end();
}

VmTextHeap::Chunk *VmTextHeap::chunkWithFreeBlock(uint64_t blockBytes) {
  for (auto &entry : chunks_) {
    if (entry.second.blockBytes == blockBytes && !entry.second.freeBlocks.empty()) {
      return &entry.second;
    }
  }
  return nullptr;
}

bool VmTextHeap::allocate(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error) {
  address = 0;
  if (size == 0) {
    return true;
  }
  const uint64_t blockBytes = blockSizeFor(size);
  if (size > LargeThreshold) {
    if (limit_ != 0 && heldBytes() + blockBytes > limit_) {
      error = "string memory limit exceeded";
      return false;
    }
    uint64_t region = 0;
    if (!core.allocateBytes(blockBytes, region, error)) {
      return false;
    }
    large_[offsetOf(region)] = blockBytes;
    largeBytes_ += blockBytes;
    bytesInUse_ += blockBytes;
    address = region;
    return true;
  }
  Chunk *chunk = chunkWithFreeBlock(blockBytes);
  if (chunk == nullptr) {
    if (limit_ != 0 && heldBytes() + ChunkBytes > limit_) {
      error = "string memory limit exceeded";
      return false;
    }
    uint64_t region = 0;
    if (!core.allocateBytes(ChunkBytes, region, error)) {
      return false;
    }
    Chunk fresh;
    fresh.base = region;
    fresh.blockBytes = blockBytes;
    fresh.blockCount = ChunkBytes / blockBytes;
    fresh.freeBlocks.reserve(static_cast<size_t>(fresh.blockCount));
    for (uint64_t i = fresh.blockCount; i > 0; --i) {
      fresh.freeBlocks.push_back(static_cast<uint32_t>(i - 1));
    }
    chunk = &(chunks_[offsetOf(region)] = std::move(fresh));
  }
  const uint32_t index = chunk->freeBlocks.back();
  chunk->freeBlocks.pop_back();
  bytesInUse_ += blockBytes;
  address = chunk->base + index * blockBytes;
  return true;
}

bool VmTextHeap::release(VmHeapCore &core, uint64_t address, std::string &error) {
  if (address == 0) {
    return true;
  }
  if (!VmHeapCore::isByteAddress(address)) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  const auto large = large_.find(offsetOf(address));
  if (large != large_.end()) {
    const uint64_t bytes = large->second;
    if (!core.releaseBytes(address, error)) {
      return false;
    }
    large_.erase(large);
    largeBytes_ -= bytes;
    bytesInUse_ -= bytes;
    return true;
  }
  const auto found = chunkContaining(address);
  if (found == chunks_.end()) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  Chunk &chunk = found->second;
  const uint64_t within = offsetOf(address) - found->first;
  if (within % chunk.blockBytes != 0 || ((address ^ chunk.base) >> VmHeapCore::GenerationShift) != 0) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  const uint32_t index = static_cast<uint32_t>(within / chunk.blockBytes);
  if (std::find(chunk.freeBlocks.begin(), chunk.freeBlocks.end(), index) != chunk.freeBlocks.end()) {
    error = "string heap block freed twice: " + std::to_string(address);
    return false;
  }
  // Freed text is zeroed so a reused block starts empty.
  uint8_t *first = core.byteAt(address);
  if (first == nullptr) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  std::fill(first, first + chunk.blockBytes, uint8_t{0});
  chunk.freeBlocks.push_back(index);
  bytesInUse_ -= chunk.blockBytes;
  if (chunk.freeBlocks.size() == chunk.blockCount) {
    // Keep one fully free chunk; return any other.
    uint64_t emptyChunks = 0;
    for (const auto &entry : chunks_) {
      if (entry.second.freeBlocks.size() == entry.second.blockCount) {
        ++emptyChunks;
      }
    }
    if (emptyChunks > 1) {
      const uint64_t base = chunk.base;
      chunks_.erase(found);
      return core.releaseBytes(base, error);
    }
  }
  return true;
}

bool VmTextHeap::blockSize(uint64_t address, uint64_t &size, std::string &error) const {
  size = 0;
  if (address == 0) {
    return true;
  }
  const auto large = large_.find(offsetOf(address));
  if (large != large_.end()) {
    size = large->second;
    return true;
  }
  const auto found = chunkContaining(address);
  if (found == chunks_.end() || (offsetOf(address) - found->first) % found->second.blockBytes != 0) {
    error = "invalid string heap address: " + std::to_string(address);
    return false;
  }
  size = found->second.blockBytes;
  return true;
}

TextHeapStats VmTextHeap::stats() const {
  TextHeapStats out;
  out.chunks = chunks_.size();
  out.bytesInUse = bytesInUse_;
  for (const auto &entry : chunks_) {
    out.bytesFree += entry.second.freeBlocks.size() * entry.second.blockBytes;
  }
  out.largeBlocks = large_.size();
  out.largeBytes = largeBytes_;
  return out;
}

bool callTextHostFunction(VmTextHeap &heap,
                          VmHeapCore &core,
                          std::string_view name,
                          const uint64_t *args,
                          uint64_t &result,
                          bool &known,
                          std::string &error) {
  known = true;
  result = 0;
  if (name == "__pstext_alloc") {
    return heap.allocate(core, args[0], result, error);
  }
  if (name == "__pstext_try_alloc") {
    std::string ignored;
    if (!heap.allocate(core, args[0], result, ignored)) {
      result = 0;
    }
    return true;
  }
  if (name == "__pstext_free") {
    return heap.release(core, args[0], error);
  }
  if (name == "__pstext_capacity") {
    return heap.blockSize(args[0], result, error);
  }
  if (name == "__pstext_stat") {
    const TextHeapStats stats = heap.stats();
    switch (args[0]) {
    case 0:
      result = stats.chunks;
      return true;
    case 1:
      result = stats.bytesInUse;
      return true;
    case 2:
      result = stats.bytesFree;
      return true;
    case 3:
      result = stats.largeBlocks;
      return true;
    case 4:
      result = stats.largeBytes;
      return true;
    default:
      error = "unknown string heap statistic: " + std::to_string(args[0]);
      return false;
    }
  }
  if (name == "__pstext_set_limit") {
    heap.setLimit(args[0]);
    return true;
  }
  known = false;
  return true;
}

} // namespace primec

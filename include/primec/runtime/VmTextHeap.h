#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "primec/runtime/VmHeapCore.h"

namespace primec {

// The string heap that backs `String` (docs/spec/strings-and-views.md, Memory). Text up to
// LargeThreshold bytes comes from 256 KiB chunks, each divided into equal blocks of one size class
// (four classes per doubling from 32 bytes to 64 KiB, so at most about a quarter of a block is
// unused); longer text gets a byte region of its own. Chunks and large blocks are byte regions of
// the VM heap, so a block address works with LoadU8/StoreU8 and address + i is byte i.
//
// A freed block goes back on its chunk's free list. A chunk whose blocks are all free is returned to
// the VM heap, except one kept for reuse; a large block is returned as soon as it is freed. The heap
// has an optional byte limit over the chunks and large blocks it holds.
struct TextHeapStats {
  uint64_t chunks = 0;
  uint64_t bytesInUse = 0;
  uint64_t bytesFree = 0;
  uint64_t largeBlocks = 0;
  uint64_t largeBytes = 0;
};

class VmTextHeap {
public:
  static constexpr uint64_t ChunkBytes = 256 * 1024;
  static constexpr uint64_t MinBlockBytes = 32;
  static constexpr uint64_t LargeThreshold = 64 * 1024;

  // The block size the heap gives text of `size` bytes: the smallest size class that holds it, or
  // `size` itself above LargeThreshold. Zero for a size of zero.
  static uint64_t blockSizeFor(uint64_t size);

  // A zeroed block of at least `size` bytes (address 0 for zero). Fails with
  // "string memory limit exceeded" when the limit would be passed.
  bool allocate(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error);
  // Returns a block; address 0 is a no-op.
  bool release(VmHeapCore &core, uint64_t address, std::string &error);
  // The size of the block at `address` (a block start).
  bool blockSize(uint64_t address, uint64_t &size, std::string &error) const;

  TextHeapStats stats() const;
  // 0 removes the limit.
  void setLimit(uint64_t bytes) { limit_ = bytes; }
  uint64_t limit() const { return limit_; }

private:
  struct Chunk {
    uint64_t base = 0;  // byte-region address of the chunk
    uint64_t blockBytes = 0;
    uint64_t blockCount = 0;
    std::vector<uint32_t> freeBlocks;  // block indices, most recently freed last
  };

  uint64_t heldBytes() const { return chunks_.size() * ChunkBytes + largeBytes_; }
  Chunk *chunkWithFreeBlock(uint64_t blockBytes);
  std::map<uint64_t, Chunk>::iterator chunkContaining(uint64_t address);
  std::map<uint64_t, Chunk>::const_iterator chunkContaining(uint64_t address) const;
  static uint64_t offsetOf(uint64_t address) { return address & VmHeapCore::ByteOffsetMask; }

  // Chunks by the byte offset of their region; the blocks of a chunk never straddle it.
  std::map<uint64_t, Chunk> chunks_;
  // Large blocks by byte offset: size.
  std::map<uint64_t, uint64_t> large_;
  uint64_t largeBytes_ = 0;
  uint64_t bytesInUse_ = 0;
  uint64_t limit_ = 0;
};

// The host functions the VM answers itself (the stdlib `String` declares them with `[host]`):
//   __pstext_alloc(size) -> Pointer<u8>      __pstext_try_alloc(size) -> Pointer<u8> (null on failure)
//   __pstext_free(Pointer<u8>)               __pstext_capacity(Pointer<u8>) -> u64 (block size)
//   __pstext_stat(which) -> u64              0 chunks, 1 bytesInUse, 2 bytesFree, 3 largeBlocks, 4 largeBytes
//   __pstext_length(Pointer<u8>) -> i32 (text length)   __pstext_usable(Pointer<u8>) -> u64 (block size less the NUL)
//   __pstext_set_limit(bytes)
inline constexpr std::string_view TextHostPrefix = "__pstext_";

// The `__pstext_*` function an import names (the stdlib declares them inside a namespace, so the import
// name carries a path), or empty when the import is not one of them.
inline std::string_view textHostFunctionName(std::string_view importName) {
  const size_t slash = importName.rfind('/');
  const std::string_view leaf = slash == std::string_view::npos ? importName : importName.substr(slash + 1);
  return leaf.rfind(TextHostPrefix, 0) == 0 ? leaf : std::string_view{};
}

// Runs the text host function `name` on `args`; `known` is false when the name is not one of them.
bool callTextHostFunction(VmTextHeap &heap,
                          VmHeapCore &core,
                          std::string_view name,
                          const uint64_t *args,
                          uint64_t &result,
                          bool &known,
                          std::string &error);

} // namespace primec

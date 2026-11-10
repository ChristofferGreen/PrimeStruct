// The string heap that backs `String`, written once and used twice (see VmTextHeap.h): compiled into the VM,
// and pasted as text into the programs the optimized C++ backend generates. It must stay self-contained:
// standard headers (algorithm, map, mutex, set, string, string_view, vector) and VmHeapCore only, no preprocessor lines,
// balanced parentheses.
PRIMEC_VM_TEXT_HEAP_BODY(
struct TextHeapStats {
  uint64_t chunks = 0;
  uint64_t bytesInUse = 0;
  uint64_t bytesFree = 0;
  uint64_t largeBlocks = 0;
  uint64_t largeBytes = 0;
  uint64_t allocations = 0;  // blocks handed out since the heap was made (empty requests excluded)
  uint64_t arenaDepth = 0;   // arenas begun and not yet ended
  uint64_t arenaBytes = 0;   // bytes of blocks handed out by the arenas that are still open
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
  // The heap of a thread arena first frees the blocks other threads freed for it, so a block freed twice fails here.
  bool allocate(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error) {
    if (lock_ == nullptr) [[likely]] {
      return allocateOwn(core, size, address, error);
    }
    return allocateShared(core, size, address, error);
  }
  // Returns a block; address 0 is a no-op.
  bool release(VmHeapCore &core, uint64_t address, std::string &error) {
    if (lock_ == nullptr) [[likely]] {
      return releaseOwn(core, address, error);
    }
    return releaseShared(core, address, error);
  }
  // The size of the block at `address` (a block start).
  bool blockSize(uint64_t address, uint64_t &size, std::string &error) const {
    if (lock_ == nullptr) [[likely]] {
      return blockSizeOwn(address, size, error);
    }
    const VmHeapCore::Guard guard(lock_);
    return blockSizeOwn(address, size, error);
  }

  TextHeapStats stats() const;

  // String arenas: while one is open every block comes from a bump region of its own and `release` of such a block
  // does nothing; `endArena` returns all the regions of the innermost arena at once. Text that outlives its arena has
  // its addresses invalidated, so a later use faults. Arenas nest; the limit counts their regions.
  void beginArena() {
    const VmHeapCore::Guard guard(lock_);
    if (arenasOff_) {
      ++arenasOffDepth_;
    } else {
      arenas_.emplace_back();
    }
  }
  bool endArena(VmHeapCore &core, std::string &error);

  // Thread heaps (VmHeapArenas.h). With a lock set, every operation takes it: the heap of an arena whose blocks
  // other threads ask the size of, or which several threads share. `disableArenas` is for a shared heap, where one
  // thread's string arena would take the other threads' blocks: begin and end then only count, and every block is an
  // ordinary one.
  void setLock(std::mutex *lock) { lock_ = lock; }
  void disableArenas() { arenasOff_ = true; }

  // The address of a module string literal's bytes (`__pstext_literal`): the literal is copied, NUL terminated,
  // into a byte region of its own the first time it is asked for, and the region lives as long as the heap.
  // `tableSize` is the number of literals of the module, `index` the one asked for; an empty literal has address 0.
  bool literal(VmHeapCore &core, uint64_t index, uint64_t tableSize, const char *data, uint64_t size,
               uint64_t &address, std::string &error);
  // 0 removes the limit.
  void setLimit(uint64_t bytes) { limit_ = bytes; }
  uint64_t limit() const { return limit_; }

private:
  struct Chunk {
    uint64_t base = 0;  // byte-region address of the chunk
    uint64_t blockBytes = 0;
    uint64_t blockCount = 0;
    std::vector<uint32_t> freeBlocks;  // block indices, most recently freed last
    std::vector<uint8_t> isFree;       // per block: 1 while it is on freeBlocks (frees check it in O(1))
    uint64_t bytes = ChunkBytes;       // size of the region (arena chunks start small and double)
    bool arena = false;                // a chunk of an arena: blocks are never reused or freed one by one
    uint64_t bump = 0;                 // arena chunks: offset of the next unused block (no free list)
  };

  struct Arena {
    std::vector<uint64_t> chunks;         // byte offsets of the chunk regions it took
    std::vector<uint64_t> large;          // addresses of its large blocks
    std::map<uint64_t, uint64_t> current; // block size -> byte offset of the chunk it is filling
    std::map<uint64_t, uint64_t> nextBytes; // block size -> size of the next chunk it takes
    uint64_t lastBlockBytes = 0;          // the size class of the last allocation and the chunk it came from
    Chunk *lastChunk = nullptr;
    uint64_t bytes = 0;                   // bytes of the small blocks handed out
  };
  // The operations themselves, with the lock (if there is one) held.
  bool allocateOwn(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error);
  bool releaseOwn(VmHeapCore &core, uint64_t address, std::string &error);
  bool blockSizeOwn(uint64_t address, uint64_t &size, std::string &error) const;
  bool allocateShared(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error);
  bool releaseShared(VmHeapCore &core, uint64_t address, std::string &error);
  bool allocateInArena(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error);

  uint64_t heldBytes() const { return chunkBytesHeld_ + largeBytes_; }
  Chunk *chunkWithFreeBlock(uint64_t blockBytes);
  std::map<uint64_t, Chunk>::iterator chunkContaining(uint64_t address);
  std::map<uint64_t, Chunk>::const_iterator chunkContaining(uint64_t address) const;
  static uint64_t offsetOf(uint64_t address) { return address & VmHeapCore::ByteOffsetMask; }

  // Chunks by the byte offset of their region; the blocks of a chunk never straddle it.
  std::map<uint64_t, Chunk> chunks_;
  // Per block size, the byte offsets of the chunks that have a free block (lowest first, so allocation takes the
  // lowest chunk as a scan of chunks_ would).
  std::map<uint64_t, std::set<uint64_t>> withFree_;
  // Chunks whose blocks are all free; one is kept, any other is returned.
  uint64_t emptyChunks_ = 0;
  // Large blocks by byte offset: size.
  std::map<uint64_t, uint64_t> large_;
  uint64_t largeBytes_ = 0;
  uint64_t allocations_ = 0;
  uint64_t chunkBytesHeld_ = 0;  // bytes of all chunk regions
  std::vector<Arena> arenas_;
  std::set<uint64_t> arenaLarge_;  // byte offsets of large blocks that belong to an arena
  uint64_t bytesInUse_ = 0;
  uint64_t limit_ = 0;
  // Region address of each literal asked for through `literal`, by string index (0 = none yet).
  std::vector<uint64_t> literalAddresses_;
  std::mutex *lock_ = nullptr;
  bool arenasOff_ = false;
  uint64_t arenasOffDepth_ = 0;
};

inline uint64_t VmTextHeap::blockSizeFor(uint64_t size) {
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

inline std::map<uint64_t, VmTextHeap::Chunk>::iterator VmTextHeap::chunkContaining(uint64_t address) {
  const uint64_t offset = offsetOf(address);
  auto it = chunks_.upper_bound(offset);
  if (it == chunks_.begin()) {
    return chunks_.end();
  }
  --it;
  return offset < it->first + it->second.bytes ? it : chunks_.end();
}

inline std::map<uint64_t, VmTextHeap::Chunk>::const_iterator VmTextHeap::chunkContaining(uint64_t address) const {
  const uint64_t offset = offsetOf(address);
  auto it = chunks_.upper_bound(offset);
  if (it == chunks_.begin()) {
    return chunks_.end();
  }
  --it;
  return offset < it->first + it->second.bytes ? it : chunks_.end();
}

inline VmTextHeap::Chunk *VmTextHeap::chunkWithFreeBlock(uint64_t blockBytes) {
  const auto candidates = withFree_.find(blockBytes);
  if (candidates == withFree_.end() || candidates->second.empty()) {
    return nullptr;
  }
  return &chunks_.find(*candidates->second.begin())->second;
}

inline bool VmTextHeap::allocateOwn(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error) {
  address = 0;
  if (size == 0) {
    return true;
  }
  ++allocations_;
  if (!arenas_.empty()) {
    return allocateInArena(core, size, address, error);
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
    fresh.isFree.assign(static_cast<size_t>(fresh.blockCount), uint8_t{1});
    chunk = &(chunks_[offsetOf(region)] = std::move(fresh));
    chunkBytesHeld_ += ChunkBytes;
    withFree_[blockBytes].insert(offsetOf(region));
    ++emptyChunks_;
  }
  if (chunk->freeBlocks.size() == chunk->blockCount) {
    --emptyChunks_;  // it is about to hold a block
  }
  const uint32_t index = chunk->freeBlocks.back();
  chunk->freeBlocks.pop_back();
  chunk->isFree[index] = 0;
  if (chunk->freeBlocks.empty()) {
    withFree_[blockBytes].erase(offsetOf(chunk->base));
  }
  bytesInUse_ += blockBytes;
  address = chunk->base + index * blockBytes;
  return true;
}

inline bool VmTextHeap::allocateInArena(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error) {
  Arena &arena = arenas_.back();
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
    arenaLarge_.insert(offsetOf(region));
    arena.large.push_back(region);
    largeBytes_ += blockBytes;
    bytesInUse_ += blockBytes;
    address = region;
    return true;
  }
  Chunk *chunk = nullptr;
  if (arena.lastChunk != nullptr && arena.lastBlockBytes == blockBytes &&
      arena.lastChunk->bump + blockBytes <= arena.lastChunk->blockCount * blockBytes) {
    chunk = arena.lastChunk;
  }
  const auto current = chunk == nullptr ? arena.current.find(blockBytes) : arena.current.end();
  if (current != arena.current.end()) {
    Chunk &candidate = chunks_.find(current->second)->second;
    if (candidate.bump + blockBytes <= candidate.blockCount * blockBytes) {
      chunk = &candidate;
    }
  }
  if (chunk == nullptr) {
    // An arena's chunks start small (a request rarely needs a whole chunk) and double up to ChunkBytes.
    uint64_t chunkBytes = arena.nextBytes.count(blockBytes) > 0 ? arena.nextBytes[blockBytes] : 0;
    if (chunkBytes == 0) {
      chunkBytes = std::max<uint64_t>(4096, blockBytes * 4);
    }
    chunkBytes = std::min<uint64_t>(chunkBytes, ChunkBytes);
    arena.nextBytes[blockBytes] = std::min<uint64_t>(chunkBytes * 2, ChunkBytes);
    if (limit_ != 0 && heldBytes() + chunkBytes > limit_) {
      error = "string memory limit exceeded";
      return false;
    }
    uint64_t region = 0;
    if (!core.allocateBytes(chunkBytes, region, error)) {
      return false;
    }
    Chunk fresh;
    fresh.base = region;
    fresh.blockBytes = blockBytes;
    fresh.bytes = chunkBytes;
    fresh.blockCount = chunkBytes / blockBytes;
    fresh.arena = true;
    chunk = &(chunks_[offsetOf(region)] = std::move(fresh));
    chunkBytesHeld_ += chunkBytes;
    arena.current[blockBytes] = offsetOf(region);
    arena.chunks.push_back(offsetOf(region));
  }
  address = chunk->base + chunk->bump;
  chunk->bump += blockBytes;
  arena.lastBlockBytes = blockBytes;
  arena.lastChunk = chunk;
  bytesInUse_ += blockBytes;
  arena.bytes += blockBytes;
  return true;
}

inline bool VmTextHeap::endArena(VmHeapCore &core, std::string &error) {
  const VmHeapCore::Guard guard(lock_);
  if (arenasOff_ && arenasOffDepth_ != 0) {
    --arenasOffDepth_;
    return true;
  }
  if (arenas_.empty()) {
    error = "string arena ended without one being open";
    return false;
  }
  Arena arena = std::move(arenas_.back());
  arenas_.pop_back();
  for (const uint64_t offset : arena.chunks) {
    const auto found = chunks_.find(offset);
    const uint64_t base = found->second.base;
    chunkBytesHeld_ -= found->second.bytes;
    chunks_.erase(found);
    if (!core.releaseBytes(base, error)) {
      return false;
    }
  }
  bytesInUse_ -= arena.bytes;
  for (const uint64_t address : arena.large) {
    const auto found = large_.find(offsetOf(address));
    const uint64_t bytes = found->second;
    large_.erase(found);
    arenaLarge_.erase(offsetOf(address));
    largeBytes_ -= bytes;
    bytesInUse_ -= bytes;
    if (!core.releaseBytes(address, error)) {
      return false;
    }
  }
  return true;
}

inline bool VmTextHeap::releaseOwn(VmHeapCore &core, uint64_t address, std::string &error) {
  if (address == 0) {
    return true;
  }
  if (!VmHeapCore::isByteAddress(address)) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  const auto large = large_.find(offsetOf(address));
  if (large != large_.end() && arenaLarge_.count(offsetOf(address)) > 0) {
    return true;  // freed with its arena
  }
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
  if (chunk.arena) {
    return true;  // freed with its arena
  }
  const uint64_t within = offsetOf(address) - found->first;
  if (within % chunk.blockBytes != 0 || ((address ^ chunk.base) >> VmHeapCore::GenerationShift) != 0) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  const uint64_t blockIndex = within / chunk.blockBytes;
  if (blockIndex >= chunk.blockCount) {
    error = "invalid string heap free address: " + std::to_string(address);
    return false;
  }
  const uint32_t index = static_cast<uint32_t>(blockIndex);
  if (chunk.isFree[index] != 0) {
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
  if (chunk.freeBlocks.empty()) {
    withFree_[chunk.blockBytes].insert(found->first);
  }
  chunk.freeBlocks.push_back(index);
  chunk.isFree[index] = 1;
  bytesInUse_ -= chunk.blockBytes;
  if (chunk.freeBlocks.size() == chunk.blockCount) {
    // Keep one fully free chunk; return any other.
    ++emptyChunks_;
    if (emptyChunks_ > 1) {
      const uint64_t base = chunk.base;
      withFree_[chunk.blockBytes].erase(found->first);
      chunkBytesHeld_ -= chunk.bytes;
      chunks_.erase(found);
      --emptyChunks_;
      return core.releaseBytes(base, error);
    }
  }
  return true;
}

inline bool VmTextHeap::blockSizeOwn(uint64_t address, uint64_t &size, std::string &error) const {
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

inline bool VmTextHeap::literal(VmHeapCore &core, uint64_t index, uint64_t tableSize, const char *data, uint64_t size,
                                uint64_t &address, std::string &error) {
  address = 0;
  if (index >= tableSize) {
    error = "invalid string index in IR";
    return false;
  }
  if (size == 0) {
    return true;
  }
  const VmHeapCore::Guard guard(lock_);
  if (literalAddresses_.size() < tableSize) {
    literalAddresses_.resize(static_cast<size_t>(tableSize), 0);
  }
  if (literalAddresses_[static_cast<size_t>(index)] == 0) {
    uint64_t region = 0;
    if (!core.allocateBytes(size + 1, region, error)) {
      return false;
    }
    std::copy(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + size, core.byteAt(region));
    literalAddresses_[static_cast<size_t>(index)] = region;
  }
  address = literalAddresses_[static_cast<size_t>(index)];
  return true;
}

inline TextHeapStats VmTextHeap::stats() const {
  TextHeapStats out;
  const VmHeapCore::Guard guard(lock_);
  out.chunks = chunks_.size();
  out.bytesInUse = bytesInUse_;
  for (const auto &entry : chunks_) {
    out.bytesFree += entry.second.freeBlocks.size() * entry.second.blockBytes;
  }
  out.largeBlocks = large_.size();
  out.largeBytes = largeBytes_;
  out.allocations = allocations_;
  out.arenaDepth = arenas_.size();
  for (const Arena &arena : arenas_) {
    out.arenaBytes += arena.bytes;
  }
  return out;
}

// Frees the blocks other threads freed for this heap's thread (VmHeapCore::releaseRemote). The owner calls it
// before it allocates when `core.hasRemoteFrees()`; a block freed twice fails here with the usual message.
inline bool drainRemoteFrees(VmTextHeap &text, VmHeapCore &core, std::string &error) {
  std::vector<std::pair<uint64_t, bool>> pending;
  core.takeRemoteFrees(pending);
  for (const std::pair<uint64_t, bool> &entry : pending) {
    if (!(entry.second ? text.release(core, entry.first, error) : core.release(entry.first, error))) {
      return false;
    }
  }
  return true;
}

inline bool VmTextHeap::allocateShared(VmHeapCore &core, uint64_t size, uint64_t &address, std::string &error) {
  address = 0;
  if (core.hasRemoteFrees() && !drainRemoteFrees(*this, core, error)) {
    return false;
  }
  const VmHeapCore::Guard guard(lock_);
  return allocateOwn(core, size, address, error);
}

inline bool VmTextHeap::releaseShared(VmHeapCore &core, uint64_t address, std::string &error) {
  if (address != 0 && VmHeapCore::isByteAddress(address) && core.isRemote(address)) {
    return core.releaseRemote(address, true, error);  // its owner frees it before its next allocation
  }
  const VmHeapCore::Guard guard(lock_);
  return releaseOwn(core, address, error);
}

// The size of a text block of another thread: its own string heap answers.
[[gnu::noinline]] inline bool textBlockSizeRemote(const VmHeapCore &core, uint64_t address, uint64_t &size,
                                                  std::string &error) {
  const VmHeapCore *owner = core.arenaOf(address);
  if (owner == nullptr) {
    size = 0;
    error = "invalid string heap address: " + std::to_string(address);
    return false;
  }
  return static_cast<const VmTextHeap *>(owner->companion())->blockSize(address, size, error);
}

// The size of a text block, which may be another thread's.
inline bool textBlockSize(const VmTextHeap &heap, const VmHeapCore &core, uint64_t address, uint64_t &size,
                          std::string &error) {
  if (core.threaded() && core.isRemote(address)) [[unlikely]] {
    return textBlockSizeRemote(core, address, size, error);
  }
  return heap.blockSize(address, size, error);
}

inline bool callTextHostFunction(VmTextHeap &heap,
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
    if (core.threaded() && core.hasRemoteFrees() && !drainRemoteFrees(heap, core, error)) {
      return false;
    }
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
    return textBlockSize(heap, core, args[0], result, error);
  }
  if (name == "__pstext_length") {
    // The text length of a block: bytes before the NUL, within the block.
    uint64_t size = 0;
    if (!textBlockSize(heap, core, args[0], size, error)) {
      return false;
    }
    const uint8_t *first = args[0] == 0 ? nullptr : core.byteAt(args[0]);
    while (first != nullptr && result < size && first[result] != 0) {
      ++result;
    }
    return true;
  }
  if (name == "__pstext_usable") {
    // The bytes of text a block holds: its size less the NUL byte (0 for the null block).
    uint64_t size = 0;
    if (!textBlockSize(heap, core, args[0], size, error)) {
      return false;
    }
    result = size == 0 ? 0 : size - 1;
    return true;
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
    case 5:
      result = stats.allocations;
      return true;
    case 6:
      result = stats.arenaDepth;
      return true;
    case 7:
      result = stats.arenaBytes;
      return true;
    case 8:
      result = core.liveSlotAllocationCount();
      return true;
    default:
      error = "unknown string heap statistic: " + std::to_string(args[0]);
      return false;
    }
  }
  if (name == "__pstext_arena_begin") {
    heap.beginArena();
    return true;
  }
  if (name == "__pstext_arena_end") {
    return heap.endArena(core, error);
  }
  if (name == "__pstext_set_limit") {
    heap.setLimit(args[0]);
    return true;
  }
  // `__pstext_hash(address, count, seed)`: SipHash-1-3 of the bytes with a 64-bit seed (the second key half is derived
  // from it), the same value on every backend. A count of 0 hashes the empty text whatever the address.
  if (name == "__pstext_hash") {
    const uint64_t length = args[1];
    const uint8_t *data = length == 0 ? nullptr : core.byteSpan(args[0], length);
    if (length != 0 && data == nullptr) {
      error = "invalid text range: " + std::to_string(length) + " bytes at " + std::to_string(args[0]);
      return false;
    }
    const uint64_t key0 = args[2];
    const uint64_t key1 = args[2] * 0x9E3779B97F4A7C15ULL + 0xD1B54A32D192ED03ULL;
    uint64_t v0 = key0 ^ 0x736f6d6570736575ULL;
    uint64_t v1 = key1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = key0 ^ 0x6c7967656e657261ULL;
    uint64_t v3 = key1 ^ 0x7465646279746573ULL;
    const auto rotl = [](uint64_t x, int b) { return (x << b) | (x >> (64 - b)); };
    const auto round = [&]() {
      v0 += v1;
      v1 = rotl(v1, 13);
      v1 ^= v0;
      v0 = rotl(v0, 32);
      v2 += v3;
      v3 = rotl(v3, 16);
      v3 ^= v2;
      v0 += v3;
      v3 = rotl(v3, 21);
      v3 ^= v0;
      v2 += v1;
      v1 = rotl(v1, 17);
      v1 ^= v2;
      v2 = rotl(v2, 32);
    };
    uint64_t at = 0;
    for (; at + 8 <= length; at += 8) {
      uint64_t word = 0;
      for (int k = 7; k >= 0; --k) {
        word = (word << 8) | data[at + static_cast<uint64_t>(k)];
      }
      v3 ^= word;
      round();
      v0 ^= word;
    }
    uint64_t last = length << 56;
    for (uint64_t k = 0; at + k < length; ++k) {
      last |= static_cast<uint64_t>(data[at + k]) << (8 * k);
    }
    v3 ^= last;
    round();
    v0 ^= last;
    v2 ^= 0xff;
    round();
    round();
    round();
    result = v0 ^ v1 ^ v2 ^ v3;
    return true;
  }
  // Floating-point to text with std::to_chars: `__pstext_format_f64(value, style, precision, destination, capacity)` is
  // the length of the text (-1 for a precision to_chars cannot do); the text is written when `capacity` holds it.
  // Styles: 0 shortest round trip as an f64, 1 fixed with `precision` digits, 2 scientific with `precision` digits,
  // 3 shortest round trip as an f32.
  if (name == "__pstext_format_f64") {
    double value = 0.0;
    std::copy(reinterpret_cast<const uint8_t *>(&args[0]), reinterpret_cast<const uint8_t *>(&args[0]) + sizeof(double),
              reinterpret_cast<uint8_t *>(&value));
    const int32_t style = static_cast<int32_t>(args[1]);
    const int32_t precision = static_cast<int32_t>(args[2]);
    char buffer[1200];
    std::to_chars_result made{buffer, std::errc::invalid_argument};
    if (style == 0) {
      made = std::to_chars(buffer, buffer + sizeof(buffer), value);
    } else if (style == 3) {
      made = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<float>(value));
    } else if ((style == 1 || style == 2) && precision >= 0 && precision <= 700) {
      made = std::to_chars(buffer, buffer + sizeof(buffer), value,
                           style == 1 ? std::chars_format::fixed : std::chars_format::scientific, precision);
    }
    if (made.ec != std::errc()) {
      result = ~uint64_t{0};
      return true;
    }
    const uint64_t length = static_cast<uint64_t>(made.ptr - buffer);
    if (args[4] >= length && length != 0) {
      uint8_t *destination = core.byteSpan(args[3], length);
      if (destination == nullptr) {
        error = "invalid text range: " + std::to_string(length) + " bytes at " + std::to_string(args[3]);
        return false;
      }
      std::copy(reinterpret_cast<const uint8_t *>(buffer), reinterpret_cast<const uint8_t *>(buffer) + length, destination);
    }
    result = length;
    return true;
  }
  // Whole-file text: `__pstext_file_size(path, pathCount)` is the size of the file (or the negated errno),
  // `__pstext_file_read(path, pathCount, destination, capacity)` reads up to `capacity` bytes into a byte region (the
  // count read, or the negated errno) and `__pstext_file_write(path, pathCount, source, count, append)` writes (or
  // appends) `count` bytes (0, or the negated errno). The path is a byte range, not NUL terminated.
  if (name == "__pstext_file_size" || name == "__pstext_file_read" || name == "__pstext_file_write") {
    const uint64_t pathCount = args[1];
    std::string path;
    if (pathCount != 0) {
      const uint8_t *first = core.byteSpan(args[0], pathCount);
      if (first == nullptr) {
        error = "invalid text range: " + std::to_string(pathCount) + " bytes at " + std::to_string(args[0]);
        return false;
      }
      path.assign(reinterpret_cast<const char *>(first), static_cast<size_t>(pathCount));
    }
    const auto failure = [&result](int code) { result = static_cast<uint64_t>(-static_cast<int64_t>(code)); };
    errno = 0;
    if (name == "__pstext_file_size") {
      std::FILE *file = std::fopen(path.c_str(), "rb");
      if (file == nullptr) {
        failure(errno != 0 ? errno : EIO);
        return true;
      }
      std::fseek(file, 0, SEEK_END);
      const long size = std::ftell(file);
      std::fclose(file);
      if (size < 0) {
        failure(errno != 0 ? errno : EIO);
      } else {
        result = static_cast<uint64_t>(size);
      }
      return true;
    }
    if (name == "__pstext_file_read") {
      const uint64_t capacity = args[3];
      uint8_t *destination = capacity == 0 ? nullptr : core.byteSpan(args[2], capacity);
      if (capacity != 0 && destination == nullptr) {
        error = "invalid text range: " + std::to_string(capacity) + " bytes at " + std::to_string(args[2]);
        return false;
      }
      std::FILE *file = std::fopen(path.c_str(), "rb");
      if (file == nullptr) {
        failure(errno != 0 ? errno : EIO);
        return true;
      }
      const size_t got = capacity == 0 ? 0 : std::fread(destination, 1, static_cast<size_t>(capacity), file);
      const bool failed = std::ferror(file) != 0;
      std::fclose(file);
      if (failed) {
        failure(errno != 0 ? errno : EIO);
      } else {
        result = static_cast<uint64_t>(got);
      }
      return true;
    }
    const uint64_t count = args[3];
    const uint8_t *source = count == 0 ? nullptr : core.byteSpan(args[2], count);
    if (count != 0 && source == nullptr) {
      error = "invalid text range: " + std::to_string(count) + " bytes at " + std::to_string(args[2]);
      return false;
    }
    std::FILE *file = std::fopen(path.c_str(), args[4] != 0 ? "ab" : "wb");
    if (file == nullptr) {
      failure(errno != 0 ? errno : EIO);
      return true;
    }
    const size_t wrote = count == 0 ? 0 : std::fwrite(source, 1, static_cast<size_t>(count), file);
    const bool failed = wrote != count || std::fflush(file) != 0;
    const int savedErrno = errno;
    std::fclose(file);
    if (failed) {
      failure(savedErrno != 0 ? savedErrno : EIO);
    }
    return true;
  }
  // Decimal text to double with the C library's correctly rounded conversion (the text is checked to be a plain
  // decimal number by the caller): `__pstext_f64_ok(address, count)` is 1 when the text converts to a finite value,
  // `__pstext_parse_f64(address, count)` is the value's bits.
  if (name == "__pstext_f64_ok" || name == "__pstext_parse_f64") {
    const uint64_t length = args[1];
    std::string digits;
    if (length != 0) {
      const uint8_t *first = core.byteSpan(args[0], length);
      if (first == nullptr) {
        error = "invalid text range: " + std::to_string(length) + " bytes at " + std::to_string(args[0]);
        return false;
      }
      digits.assign(reinterpret_cast<const char *>(first), static_cast<size_t>(length));
    }
    char *end = nullptr;
    const double value = std::strtod(digits.c_str(), &end);
    const bool ok = !digits.empty() && end == digits.c_str() + digits.size() && value - value == 0.0;
    if (name == "__pstext_f64_ok") {
      result = ok ? 1 : 0;
    } else {
      static_assert(sizeof(double) == sizeof(uint64_t), "a double fits a slot");
      std::copy(reinterpret_cast<const uint8_t *>(&value), reinterpret_cast<const uint8_t *>(&value) + sizeof(double),
                reinterpret_cast<uint8_t *>(&result));
    }
    return true;
  }
  // The bulk byte operations of String and the byte containers: one range check per call, then a library
  // loop. Each address names a byte region (a text block or a vector<u8> buffer); a range that leaves its region
  // faults. A count of 0 touches nothing, whatever the addresses.
  if (name == "__pstext_move" || name == "__pstext_fill" || name == "__pstext_compare" || name == "__pstext_find") {
    const bool isMove = name == "__pstext_move";
    const bool isFill = name == "__pstext_fill";
    const bool isFind = name == "__pstext_find";
    // move(destination, source, count), fill(destination, value, count), compare(a, b, count),
    // find(address, count, value)
    const uint64_t count = isFind ? args[1] : args[2];
    if (count == 0) {
      result = isFind ? ~uint64_t{0} : 0;
      return true;
    }
    uint8_t *first = core.byteSpan(args[0], count);
    const uint8_t *second = (isFill || isFind) ? first : core.byteSpan(args[1], count);
    if (first == nullptr || second == nullptr) {
      error = "invalid text range: " + std::to_string(count) + " bytes at " + std::to_string(args[0]);
      return false;
    }
    if (isMove) {
      if (first < second) {
        std::copy(second, second + count, first);
      } else if (first > second) {
        std::copy_backward(second, second + count, first + count);
      }
    } else if (isFill) {
      std::fill(first, first + count, static_cast<uint8_t>(args[1]));
    } else if (isFind) {
      const uint8_t *hit = std::find(first, first + count, static_cast<uint8_t>(args[2]));
      result = hit == first + count ? ~uint64_t{0} : static_cast<uint64_t>(hit - first);
    } else {
      const auto differs = std::mismatch(first, first + count, second);
      if (differs.first != first + count) {
        result = *differs.first < *differs.second ? ~uint64_t{0} : uint64_t{1};
      }
    }
    return true;
  }
  known = false;
  return true;
}
)

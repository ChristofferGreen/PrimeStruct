#include "primec/CompileArena.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

// TODO-5233/TODO-5234/TODO-5235: see docs/CompilerArenaAllocator.md for the
// full design history. Short version: an earlier iteration (TODO-5234)
// reset the arena's free lists/bump cursor at every new compile scope (so a
// long-lived test binary's arena wouldn't grow across thousands of
// TEST_CASEs), and that reset step reproducibly corrupted function-local
// `static const` ("magic static") values - std::string/vector constants
// computed once on first call and reused for the rest of the process. If
// one of those got constructed while a compile scope was active, its
// backing bytes were arena-allocated; the *next* scope's reset then
// silently handed those exact bytes to a brand new object while the magic
// static was still alive and expected to be readable - corrupting it.
// TODO-5234 fell back to a never-reset arena (CLI binaries only, test
// binaries entirely on the system allocator) rather than ship that.
//
// TODO-5235 closes the gap with two general escape hatches instead of
// enumerating every magic static's fix individually:
//
//   - SystemHeapScope (and the systemHeapValue() convenience wrapper in the
//     header): forces allocations on the current thread to the system heap
//     regardless of whether a compile scope is active. Every known magic
//     static under src/semantics, src/ir_lowerer, and src/parser now builds
//     its value inside one of these, so its backing memory is never arena
//     memory in the first place and a reset can never invalidate it.
//   - registerArenaResetCallback(): for the handful of *known*,
//     intentionally-persistent thread_local caches (as opposed to
//     unenumerable magic statics) - normalizeBindingTypeName's cache,
//     findStdlibSurfaceMetadataByResolvedPath's cache, SourceLocationMapper's
//     cached-mapper-by-address - registers a callback that clears the cache
//     on every reset, so no cached entry can dangle into memory the reset
//     just reclaimed.
//
// With both in place, resets are safe again: the arena now resets whenever
// the outermost ScopedCompileArena on a thread is destroyed, which CLI
// binaries do once (at process exit - a no-op in practice) and test
// binaries do once per TEST_CASE (see tests/unit/test_main.cpp's listener).

namespace primec {
namespace {

// Every allocation this TU hands out - whether or not an arena is active -
// carries this 16-byte header immediately before the returned pointer, so
// that operator delete can always determine how to free it purely from the
// pointer itself, never from "is an arena currently active on this thread"
// (which can differ between the thread that allocated and the thread that
// frees an object, e.g. across a std::async boundary).
struct BlockHeader {
  void *owner;               // nullptr => plain system allocation; else the owning CompileArena*.
  std::uint32_t classIndex;  // valid iff owner != nullptr.
  std::uint32_t reserved = 0;
};

constexpr std::size_t kHeaderSize = sizeof(BlockHeader);
static_assert(kHeaderSize == 16, "BlockHeader is expected to be exactly one 16-byte slot");
static_assert(alignof(std::max_align_t) <= 16, "arena assumes <=16-byte default alignment");

// Size classes for payload bytes. Anything larger than the last class falls
// straight through to the system allocator (still via the uniform header,
// so delete stays branch-free-of-ambient-state) - large allocations are rare
// in the profiled workload and not worth arena-tracking; the allocation
// COUNT that dominates the profile (strings, small vectors, hash-table
// nodes) is concentrated well under 1KB.
constexpr std::size_t kSizeClasses[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
constexpr std::size_t kNumSizeClasses = sizeof(kSizeClasses) / sizeof(kSizeClasses[0]);
constexpr std::size_t kChunkSize = 4u * 1024u * 1024u; // 4MB

struct FreeNode {
  FreeNode *next;
};

// One arena per thread, lazily created on first use and never destroyed or
// reset - see the file comment for why "never reset" is the load-bearing
// safety property here. Free lists still recycle same-thread frees *within*
// that single, process-lifetime scope (this is still what avoids the
// "peak memory balloons to the compile's *cumulative* allocation volume
// instead of its live working set" risk a pure bump-and-never-free
// allocator would have - see docs/CompilerArenaAllocator.md).
class CompileArena {
public:
  void *allocate(std::size_t classIndex) {
    FreeNode *&head = freeLists_[classIndex];
    if (head != nullptr) {
      void *block = head;
      head = head->next;
      return block;
    }
    return bumpAllocate(kHeaderSize + kSizeClasses[classIndex]);
  }

  void deallocate(void *block, std::size_t classIndex) {
    auto *node = static_cast<FreeNode *>(block);
    node->next = freeLists_[classIndex];
    freeLists_[classIndex] = node;
  }

  // Rewinds every chunk's bump cursor back to empty and clears the free
  // lists, so the next allocation reuses the same backing chunks from
  // scratch. Chunks themselves are never unmapped/freed here - only their
  // "used" bookkeeping resets - which is what keeps repeated resets across
  // thousands of TEST_CASEs from re-doing the malloc/free-family chunk
  // growth work each time. Safe to call only when nothing still alive
  // references arena memory from the scope that's ending - see
  // SystemHeapScope/registerArenaResetCallback in the header for how the
  // two remaining hazard classes (magic statics, persistent thread_local
  // caches) are kept out of arena memory in the first place.
  void reset() {
    for (Chunk *chunk = firstChunk_; chunk != nullptr; chunk = chunk->next) {
      chunk->used = 0;
    }
    currentChunk_ = firstChunk_;
    for (FreeNode *&head : freeLists_) {
      head = nullptr;
    }
  }

private:
  // Intrusive singly-linked list, every node (and its data buffer) obtained
  // via plain std::malloc rather than `new` - deliberately, so that growing
  // the arena's own bookkeeping never itself routes through arenaAllocate()
  // (which would recurse back into this same arena instance's chunk list
  // while it's already mid-mutation: e.g. a std::vector<Chunk> here would
  // need to grow *itself* via operator new exactly when this function is
  // adding a chunk because the arena is out of room, re-entering this same
  // method on the same object with its invariants half-updated. Plain
  // malloc-backed nodes sidestep that entirely).
  struct Chunk {
    unsigned char *data = nullptr;
    std::size_t size = 0;
    std::size_t used = 0;
    Chunk *next = nullptr;
  };

  static Chunk *allocateChunkNode(std::size_t dataSize) {
    auto *chunk = static_cast<Chunk *>(std::malloc(sizeof(Chunk)));
    auto *data = static_cast<unsigned char *>(std::malloc(dataSize));
    if (chunk == nullptr || data == nullptr) {
      std::free(chunk);
      std::free(data);
      throw std::bad_alloc();
    }
    chunk->data = data;
    chunk->size = dataSize;
    chunk->used = 0;
    chunk->next = nullptr;
    return chunk;
  }

  void *bumpAllocate(std::size_t blockSize) {
    // blockSize is always a multiple of kHeaderSize (16) since every size
    // class is itself a multiple of 16, so this never needs extra alignment
    // padding beyond starting each chunk 16-aligned (malloc already returns
    // max_align_t-aligned memory).
    while (currentChunk_ != nullptr) {
      if (currentChunk_->size - currentChunk_->used >= blockSize) {
        void *block = currentChunk_->data + currentChunk_->used;
        currentChunk_->used += blockSize;
        return block;
      }
      if (currentChunk_->next == nullptr) {
        break;
      }
      currentChunk_ = currentChunk_->next;
    }
    const std::size_t newChunkSize = blockSize > kChunkSize ? blockSize : kChunkSize;
    Chunk *chunk = allocateChunkNode(newChunkSize);
    chunk->used = blockSize;
    if (currentChunk_ != nullptr) {
      currentChunk_->next = chunk;
    } else {
      firstChunk_ = chunk;
    }
    currentChunk_ = chunk;
    return chunk->data;
  }

  Chunk *firstChunk_ = nullptr;
  Chunk *currentChunk_ = nullptr;
  FreeNode *freeLists_[kNumSizeClasses] = {};
};

// Lazily created, never destroyed (deliberately - this TU intentionally
// leaks the arena object itself at process exit, same as any other
// process-lifetime singleton; the OS reclaims it). Raw pointer, not a
// smart pointer or thread_local object-by-value, so that a thread which
// never enters a compile scope never pays for constructing one.
thread_local CompileArena *tls_arena = nullptr;
thread_local int tls_scopeDepth = 0;

// SystemHeapScope's nesting depth on the current thread. While > 0,
// arenaAllocate() always falls through to the system heap regardless of
// tls_scopeDepth - see the file comment and CompileArena.h for why this
// exists (the general escape hatch for magic statics).
thread_local int tls_forceSystemHeap = 0;

// Fixed-capacity registry of reset callbacks (see registerArenaResetCallback
// in the header). A plain array, not a std::vector: registrations happen
// from namespace-scope static initializers, i.e. before main() and
// certainly before any ScopedCompileArena exists, but using any container
// that itself allocates would reintroduce exactly the kind of
// static-initialization-order/bootstrapping hazard this file already works
// around elsewhere (see currentThreadArena()'s comment). The known
// registrants (three, as of TODO-5235) are far below this capacity.
constexpr int kMaxArenaResetCallbacks = 32;
void (*g_arenaResetCallbacks[kMaxArenaResetCallbacks])() = {};
int g_arenaResetCallbackCount = 0;

void runArenaResetCallbacks() {
  for (int i = 0; i < g_arenaResetCallbackCount; ++i) {
    g_arenaResetCallbacks[i]();
  }
}

CompileArena &currentThreadArena() {
  if (tls_arena == nullptr) {
    // Deliberately bypass the overridden global operator new here (plain
    // std::malloc + placement new instead of `new CompileArena()`):
    // constructing the arena is itself an allocation, and routing that
    // allocation back through arenaAllocate() before tls_arena is assigned
    // would recurse into currentThreadArena() forever (tls_arena reads as
    // still-null on every nested call until the outermost one returns,
    // which never happens).
    void *raw = std::malloc(sizeof(CompileArena));
    if (raw == nullptr) {
      throw std::bad_alloc();
    }
    tls_arena = new (raw) CompileArena();
  }
  return *tls_arena;
}

int classIndexForPayload(std::size_t size) {
  for (std::size_t i = 0; i < kNumSizeClasses; ++i) {
    if (size <= kSizeClasses[i]) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void *systemAllocate(std::size_t size) {
  void *raw = std::malloc(kHeaderSize + size);
  if (raw == nullptr) {
    throw std::bad_alloc();
  }
  auto *header = static_cast<BlockHeader *>(raw);
  header->owner = nullptr;
  header->classIndex = 0;
  return static_cast<unsigned char *>(raw) + kHeaderSize;
}

void *arenaAllocate(std::size_t size) {
  const int classIndex = classIndexForPayload(size);
  if (classIndex < 0 || tls_scopeDepth == 0 || tls_forceSystemHeap > 0) {
    return systemAllocate(size);
  }
  CompileArena &arena = currentThreadArena();
  void *block = arena.allocate(static_cast<std::size_t>(classIndex));
  auto *header = static_cast<BlockHeader *>(block);
  header->owner = &arena;
  header->classIndex = static_cast<std::uint32_t>(classIndex);
  return static_cast<unsigned char *>(block) + kHeaderSize;
}

void arenaDeallocate(void *ptr) noexcept {
  if (ptr == nullptr) {
    return;
  }
  auto *block = static_cast<unsigned char *>(ptr) - kHeaderSize;
  auto *header = reinterpret_cast<BlockHeader *>(block);
  if (header->owner == nullptr) {
    std::free(block);
    return;
  }
  // Only ever push back onto the free list of the arena that owns this
  // block *and* only if that arena is this thread's own arena instance -
  // never a foreign thread's. A block freed on a different thread than the
  // one that allocated it (possible if ownership of an object is
  // transferred across a std::async boundary) is deliberately leaked
  // rather than recycled: it remains perfectly valid, never-unmapped memory
  // (chunks are never freed for the life of the process), so leaking it
  // costs at most some unreclaimed capacity in the *originating* arena, but
  // handing it out again through a *different* arena's free list would
  // risk two live objects aliasing the same bytes. See
  // docs/CompilerArenaAllocator.md's cross-thread-free section.
  if (header->owner == tls_arena) {
    tls_arena->deallocate(block, header->classIndex);
  }
}

} // namespace
} // namespace primec

// ---------------------------------------------------------------------------
// Global replaceable operator new/delete overrides.
//
// Deliberately NOT overriding the aligned-allocation overloads
// (operator new/delete taking std::align_val_t): the compiler calls those
// consistently for both the allocation and the matching deallocation of any
// over-aligned type, so leaving them at their default (system-allocator)
// implementation is both simpler and safe - those allocations just never
// participate in the arena. Over-aligned types are rare in this codebase's
// hot paths (the DHAT/callgrind survey in docs/CompilerArenaAllocator.md
// found none among the dominant allocation sites).
// ---------------------------------------------------------------------------

void *operator new(std::size_t size) {
  return primec::arenaAllocate(size);
}

void *operator new[](std::size_t size) {
  return primec::arenaAllocate(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
  try {
    return primec::arenaAllocate(size);
  } catch (...) {
    return nullptr;
  }
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
  try {
    return primec::arenaAllocate(size);
  } catch (...) {
    return nullptr;
  }
}

void operator delete(void *ptr) noexcept {
  primec::arenaDeallocate(ptr);
}

void operator delete[](void *ptr) noexcept {
  primec::arenaDeallocate(ptr);
}

void operator delete(void *ptr, std::size_t) noexcept {
  primec::arenaDeallocate(ptr);
}

void operator delete[](void *ptr, std::size_t) noexcept {
  primec::arenaDeallocate(ptr);
}

void operator delete(void *ptr, const std::nothrow_t &) noexcept {
  primec::arenaDeallocate(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept {
  primec::arenaDeallocate(ptr);
}

namespace primec {

ScopedCompileArena::ScopedCompileArena() {
  ++tls_scopeDepth;
}

ScopedCompileArena::~ScopedCompileArena() {
  --tls_scopeDepth;
  if (tls_scopeDepth == 0 && tls_arena != nullptr) {
    // Order matters: clear the registered thread_local caches first (their
    // destructors/clear() calls may still touch arena memory from the scope
    // that's ending), then rewind the arena itself. Nothing else may still
    // reference arena memory from this scope at this point - magic statics
    // never lived in arena memory to begin with (SystemHeapScope), and
    // these are the only other known class of cross-scope-persistent state.
    runArenaResetCallbacks();
    tls_arena->reset();
  }
}

SystemHeapScope::SystemHeapScope() {
  ++tls_forceSystemHeap;
}

SystemHeapScope::~SystemHeapScope() {
  --tls_forceSystemHeap;
}

void registerArenaResetCallback(void (*callback)()) {
  if (g_arenaResetCallbackCount < kMaxArenaResetCallbacks) {
    g_arenaResetCallbacks[g_arenaResetCallbackCount++] = callback;
  }
}

} // namespace primec

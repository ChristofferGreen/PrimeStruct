#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHeapArenas.h"
#include "primec/runtime/VmHeapCore.h"
#include "primec/runtime/VmTextHeap.h"

#include "third_party/doctest.h"

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

using namespace primec;

TEST_SUITE_BEGIN("primestruct.runtime.text_heap");

namespace {

struct Fixture {
  VmHeapCore core;
  VmTextHeap heap;

  uint64_t alloc(uint64_t size) {
    uint64_t address = 0;
    std::string error;
    REQUIRE_MESSAGE(heap.allocate(core, size, address, error), error);
    return address;
  }
  void free(uint64_t address) {
    std::string error;
    REQUIRE_MESSAGE(heap.release(core, address, error), error);
  }
};

} // namespace

TEST_CASE("block sizes are four per doubling from 32 bytes to 64 KiB") {
  CHECK(VmTextHeap::blockSizeFor(0) == 0);
  CHECK(VmTextHeap::blockSizeFor(1) == 32);
  CHECK(VmTextHeap::blockSizeFor(32) == 32);
  CHECK(VmTextHeap::blockSizeFor(33) == 40);
  CHECK(VmTextHeap::blockSizeFor(48) == 48);
  CHECK(VmTextHeap::blockSizeFor(56) == 56);
  CHECK(VmTextHeap::blockSizeFor(57) == 64);
  CHECK(VmTextHeap::blockSizeFor(65) == 80);
  CHECK(VmTextHeap::blockSizeFor(113) == 128);
  CHECK(VmTextHeap::blockSizeFor(32769) == 40960);
  CHECK(VmTextHeap::blockSizeFor(65536) == 65536);
  // Above 64 KiB the block is exactly the request.
  CHECK(VmTextHeap::blockSizeFor(65537) == 65537);
  // From the smallest class up, every class wastes at most a quarter of its block.
  for (uint64_t size = 32; size <= 65536; size += 7) {
    const uint64_t block = VmTextHeap::blockSizeFor(size);
    REQUIRE(block >= size);
    CHECK(block - size <= block / 4 + 1);
  }
}

TEST_CASE("an empty request allocates nothing") {
  Fixture f;
  CHECK(f.alloc(0) == 0);
  f.free(0);
  CHECK(f.heap.stats().chunks == 0);
}

TEST_CASE("blocks are zeroed, byte addressed and reused after being freed") {
  Fixture f;
  const uint64_t first = f.alloc(10);
  REQUIRE(first != 0);
  for (uint64_t i = 0; i < 32; ++i) {
    uint8_t *byte = f.core.byteAt(first + i);
    REQUIRE(byte != nullptr);
    CHECK(*byte == 0);
    *byte = static_cast<uint8_t>(i + 1);
  }
  CHECK(f.heap.stats().bytesInUse == 32);
  CHECK(f.heap.stats().chunks == 1);
  f.free(first);
  CHECK(f.heap.stats().bytesInUse == 0);
  const uint64_t second = f.alloc(20);
  CHECK(second == first);
  for (uint64_t i = 0; i < 32; ++i) {
    CHECK(*f.core.byteAt(second + i) == 0);
  }
}

TEST_CASE("blocks of different classes live in different chunks") {
  Fixture f;
  const uint64_t small = f.alloc(8);
  const uint64_t medium = f.alloc(100);
  const uint64_t other = f.alloc(1000);
  CHECK(f.heap.stats().chunks == 3);
  uint64_t size = 0;
  std::string error;
  REQUIRE(f.heap.blockSize(small, size, error));
  CHECK(size == 32);
  REQUIRE(f.heap.blockSize(medium, size, error));
  CHECK(size == 112);
  REQUIRE(f.heap.blockSize(other, size, error));
  CHECK(size == 1024);
  f.free(small);
  f.free(medium);
  f.free(other);
}

TEST_CASE("a size class's chunks start at 4 KiB and double up to 256 KiB") {
  Fixture f;
  REQUIRE(VmTextHeap::FirstChunkBytes == 4096);
  REQUIRE(VmTextHeap::ChunkBytes == 256 * 1024);
  // Each new chunk is as large as the class's chunks together: 4, 4, 8, 16, ... 128, 256, 256 KiB.
  const std::vector<uint64_t> expected = {
      4096, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 262144};
  std::vector<uint64_t> blocks;
  uint64_t held = 0;
  for (const uint64_t chunkBytes : expected) {
    for (uint64_t i = 0; i < chunkBytes / 32; ++i) {
      blocks.push_back(f.alloc(32));
    }
    held += chunkBytes;
    const TextHeapStats stats = f.heap.stats();
    CHECK(stats.chunkBytes == held);
    CHECK(stats.bytesFree == 0);
    CHECK(f.core.liveByteCount() == held);
  }
  CHECK(f.heap.stats().chunks == expected.size());
  CHECK(f.heap.stats().chunkRegions == expected.size());
  // A class with blocks over 4 KiB starts with a chunk of one block.
  const uint64_t big = f.alloc(40000);
  CHECK(f.heap.stats().chunkBytes == held + VmTextHeap::blockSizeFor(40000));
  f.free(big);
  for (const uint64_t block : blocks) {
    f.free(block);
  }
  CHECK(f.heap.stats().bytesInUse == 0);
}

TEST_CASE("a size class keeps one free chunk, the smaller of two and the lower of two alike") {
  Fixture f;
  // Three chunks of 32-byte blocks: 4 KiB (128 blocks), 4 KiB and 8 KiB (256 blocks).
  std::vector<uint64_t> blocks;
  for (uint64_t i = 0; i < 512; ++i) {
    blocks.push_back(f.alloc(32));
  }
  REQUIRE(f.heap.stats().chunks == 3);
  REQUIRE(f.heap.stats().chunkBytes == 16384);
  // The 8 KiB chunk empties first and is kept: it is the class's only free chunk.
  for (uint64_t i = 256; i < 512; ++i) {
    f.free(blocks[i]);
  }
  CHECK(f.heap.stats().chunks == 3);
  CHECK(f.heap.stats().freeChunkBytes == 8192);
  // The second 4 KiB chunk empties: it is the smaller, so the 8 KiB chunk is returned.
  for (uint64_t i = 128; i < 256; ++i) {
    f.free(blocks[i]);
  }
  CHECK(f.heap.stats().chunks == 2);
  CHECK(f.heap.stats().freeChunkBytes == 4096);
  CHECK(f.core.byteAt(blocks[256]) == nullptr);
  CHECK(f.core.byteAt(blocks[128]) != nullptr);
  // The first chunk empties: the two are alike, the lower one stays.
  for (uint64_t i = 0; i < 128; ++i) {
    f.free(blocks[i]);
  }
  const TextHeapStats stats = f.heap.stats();
  CHECK(stats.chunks == 1);
  CHECK(stats.bytesInUse == 0);
  CHECK(stats.bytesFree == 4096);
  CHECK(stats.chunkBytes == 4096);
  CHECK(stats.freeChunkBytes == 4096);
  CHECK(f.core.liveByteCount() == 4096);
  CHECK(f.core.byteAt(blocks[128]) == nullptr);
  CHECK(f.core.byteAt(blocks[0]) != nullptr);
  // The kept chunk serves the next block without a new region.
  const uint64_t regions = stats.chunkRegions;
  const uint64_t again = f.alloc(32);
  CHECK(again >= blocks[0]);
  CHECK(again < blocks[0] + VmTextHeap::FirstChunkBytes);
  CHECK(f.heap.stats().chunkRegions == regions);
  CHECK(f.heap.stats().freeChunkBytes == 0);
}

TEST_CASE("a class at a chunk boundary keeps the chunk it empties again and again") {
  Fixture f;
  std::vector<uint64_t> blocks;
  for (uint64_t i = 0; i < 128; ++i) {
    blocks.push_back(f.alloc(32));
  }
  for (int round = 0; round < 100; ++round) {
    const uint64_t extra = f.alloc(32); // the first block of the second chunk
    f.free(extra);
  }
  CHECK(f.heap.stats().chunks == 2);
  CHECK(f.heap.stats().chunkRegions == 2);
  for (const uint64_t block : blocks) {
    f.free(block);
  }
  CHECK(f.heap.stats().chunks == 1);
}

TEST_CASE("the first chunks of every size class are kept free together") {
  Fixture f;
  std::vector<uint64_t> blocks;
  uint64_t classes = 0;
  uint64_t firstChunkBytes = 0;
  for (uint64_t size = 32; size <= VmTextHeap::LargeThreshold;
       size = VmTextHeap::blockSizeFor(size + 1)) {
    blocks.push_back(f.alloc(size));
    ++classes;
    firstChunkBytes += std::max(VmTextHeap::FirstChunkBytes, size);
  }
  CHECK(classes == 45);
  CHECK(firstChunkBytes == 518144); // 506 KiB
  CHECK(firstChunkBytes <= VmTextHeap::FreeChunkLimit);
  for (const uint64_t block : blocks) {
    f.free(block);
  }
  TextHeapStats stats = f.heap.stats();
  CHECK(stats.chunks == classes);
  CHECK(stats.chunkBytes == firstChunkBytes);
  CHECK(stats.freeChunkBytes == firstChunkBytes);
  // Every class allocates again without a new region.
  for (uint64_t size = 32; size <= VmTextHeap::LargeThreshold;
       size = VmTextHeap::blockSizeFor(size + 1)) {
    f.free(f.alloc(size));
  }
  stats = f.heap.stats();
  CHECK(stats.chunkRegions == classes);
  CHECK(stats.chunks == classes);
}

TEST_CASE("classes with large blocks end with their one-block first chunks") {
  Fixture f;
  // Five classes with 64 KiB blocks or nearly: fill each until its last chunk is a full 256 KiB one, then free
  // everything but one block in each of those last chunks.
  const std::vector<uint64_t> sizes = {65536, 57344, 49152, 40960, 32768};
  std::vector<std::vector<uint64_t>> blocks(sizes.size());
  std::vector<uint64_t> last(sizes.size(), 0);
  for (size_t c = 0; c < sizes.size(); ++c) {
    uint64_t chunkBytes = 0;
    while (chunkBytes < VmTextHeap::ChunkBytes) {
      const uint64_t before = f.heap.stats().chunkBytes;
      blocks[c].push_back(f.alloc(sizes[c]));
      const uint64_t grown = f.heap.stats().chunkBytes - before;
      chunkBytes = grown != 0 ? grown : chunkBytes;
    }
    last[c] = blocks[c].back();
    blocks[c].pop_back();
  }
  for (const std::vector<uint64_t> &ofClass : blocks) {
    for (const uint64_t block : ofClass) {
      f.free(block);
    }
  }
  // Each class kept its smallest chunk (one block) free: well under the limit.
  TextHeapStats stats = f.heap.stats();
  CHECK(stats.chunks == 2 * sizes.size());
  CHECK(stats.freeChunkBytes == 65536 + 57344 + 49152 + 40960 + 32768);
  // The 256 KiB chunks empty: the classes' small free chunks are the smaller ones and stay, the big ones go.
  for (const uint64_t block : last) {
    f.free(block);
  }
  stats = f.heap.stats();
  CHECK(stats.chunks == sizes.size());
  CHECK(stats.freeChunkBytes == 65536 + 57344 + 49152 + 40960 + 32768);
  CHECK(stats.bytesInUse == 0);
}

TEST_CASE("free chunks beyond 1 MiB are returned, the largest first") {
  Fixture f;
  REQUIRE(VmTextHeap::FreeChunkLimit == 1024 * 1024);
  // Five classes with blocks of 32 to 64 KiB, each grown until it takes a 256 KiB chunk.
  const std::vector<uint64_t> sizes = {65536, 57344, 49152, 40960, 32768};
  std::vector<uint64_t> lastBlocks;
  for (const uint64_t size : sizes) {
    std::vector<uint64_t> ofClass;
    uint64_t chunkBytes = 0;
    while (chunkBytes < VmTextHeap::ChunkBytes) {
      const uint64_t before = f.heap.stats().chunkBytes;
      ofClass.push_back(f.alloc(size));
      const uint64_t grown = f.heap.stats().chunkBytes - before;
      chunkBytes = grown != 0 ? grown : chunkBytes;
    }
    // Free the class's other blocks while the big chunk holds one.
    const uint64_t inBig = ofClass.back();
    ofClass.pop_back();
    for (const uint64_t block : ofClass) {
      f.free(block);
    }
    lastBlocks.push_back(inBig);
  }
  // Now each class holds its one-block first chunk (free) and its 256 KiB chunk (one live block). Use the first
  // chunks, so the big chunks become the classes' only free chunks when their blocks go.
  std::vector<uint64_t> inFirst;
  for (const uint64_t size : sizes) {
    inFirst.push_back(f.alloc(size));
  }
  REQUIRE(f.heap.stats().freeChunkBytes == 0);
  for (size_t c = 0; c < 4; ++c) {
    f.free(lastBlocks[c]);
  }
  CHECK(f.heap.stats().freeChunkBytes == 4 * VmTextHeap::ChunkBytes);
  CHECK(f.heap.stats().chunks == 10);
  f.free(lastBlocks[4]);
  CHECK(f.heap.stats().freeChunkBytes == 4 * VmTextHeap::ChunkBytes);
  CHECK(f.heap.stats().chunks == 9);
  // A smaller chunk that becomes free then is kept as well only if it fits: a 4 KiB one does not, and the largest
  // free chunk (the highest of those alike) makes room for it.
  const uint64_t small = f.alloc(10);
  f.free(small);
  CHECK(f.heap.stats().freeChunkBytes == 3 * VmTextHeap::ChunkBytes + 4096);
  CHECK(f.heap.stats().chunks == 9);
  for (const uint64_t block : inFirst) {
    f.free(block);
  }
  CHECK(f.heap.stats().bytesInUse == 0);
  CHECK(f.heap.stats().freeChunkBytes <= VmTextHeap::FreeChunkLimit);
}

TEST_CASE(
    "a thread's heap that builds and drops strings through three classes takes no new region") {
  // What a String with a few appends does: 32 bytes, then 64, then 128, each time copying and freeing the smaller
  // block, then the drop. On a thread that holds no other text every chunk involved empties once per string.
  const auto run = [](VmHeapCore &core, VmTextHeap &heap) {
    std::string error;
    uint64_t regionsAfterFirst = 0;
    for (int i = 0; i < 1000; ++i) {
      uint64_t block = 0;
      REQUIRE_MESSAGE(heap.allocate(core, 32, block, error), error);
      for (const uint64_t size : {uint64_t{64}, uint64_t{128}}) {
        uint64_t larger = 0;
        REQUIRE_MESSAGE(heap.allocate(core, size, larger, error), error);
        REQUIRE_MESSAGE(heap.release(core, block, error), error);
        block = larger;
      }
      REQUIRE_MESSAGE(heap.release(core, block, error), error);
      if (i == 0) {
        regionsAfterFirst = heap.stats().chunkRegions;
      }
    }
    const TextHeapStats stats = heap.stats();
    CHECK(regionsAfterFirst == 3);
    CHECK(stats.chunkRegions == regionsAfterFirst);
    CHECK(stats.bytesInUse == 0);
    // What the heap holds with everything freed: three first chunks (256 KiB before chunks started small).
    CHECK(stats.chunkBytes == 3 * VmTextHeap::FirstChunkBytes);
    CHECK(core.liveByteCount() == 3 * VmTextHeap::FirstChunkBytes);
  };
  SUBCASE("the main thread's heap") {
    Fixture f;
    run(f.core, f.heap);
  }
  SUBCASE("a thread arena's heap, behind its lock") {
    VmHeapArenas arenas(uint64_t{1} << 20, uint64_t{1} << 24);
    std::string error;
    VmHeapArena *arena = arenas.acquire(error);
    REQUIRE_MESSAGE(arena != nullptr, error);
    run(arena->core, arena->text);
    arenas.release(arena);
  }
}

TEST_CASE("a build-and-drop loop keeps the chunk count constant") {
  Fixture f;
  uint64_t chunksAfterWarmup = 0;
  for (int round = 0; round < 200; ++round) {
    std::vector<uint64_t> live;
    for (int i = 0; i < 300; ++i) {
      live.push_back(f.alloc(static_cast<uint64_t>(20 + (i % 90))));
    }
    for (const uint64_t block : live) {
      f.free(block);
    }
    if (round == 3) {
      chunksAfterWarmup = f.heap.stats().chunks;
    }
  }
  CHECK(f.heap.stats().chunks == chunksAfterWarmup);
  CHECK(f.heap.stats().bytesInUse == 0);
}

TEST_CASE("large blocks have their own allocation and return as soon as they are freed") {
  Fixture f;
  const uint64_t before = f.core.liveByteCount();
  const uint64_t big = f.alloc(100000);
  TextHeapStats stats = f.heap.stats();
  CHECK(stats.largeBlocks == 1);
  CHECK(stats.largeBytes == 100000);
  CHECK(stats.chunks == 0);
  CHECK(f.core.liveByteCount() == before + 100000);
  *f.core.byteAt(big + 99999) = 7;
  f.free(big);
  stats = f.heap.stats();
  CHECK(stats.largeBlocks == 0);
  CHECK(stats.largeBytes == 0);
  CHECK(f.core.liveByteCount() == before);
}

TEST_CASE("the memory limit caps chunks and large blocks") {
  Fixture f;
  f.heap.setLimit(VmTextHeap::FirstChunkBytes + 1000);
  const uint64_t small = f.alloc(10);
  uint64_t address = 0;
  std::string error;
  CHECK_FALSE(f.heap.allocate(f.core, 100, address, error));
  CHECK(error == "string memory limit exceeded");
  CHECK_FALSE(f.heap.allocate(f.core, 70000, address, error));
  // The same class still allocates from its chunk.
  CHECK(f.alloc(20) != 0);
  f.heap.setLimit(0);
  CHECK(f.alloc(100) != 0);
  f.free(small);
}

TEST_CASE("the memory limit counts a class's growing chunks") {
  Fixture f;
  // 4 KiB and 4 KiB fit, the 8 KiB chunk the class takes third does not.
  f.heap.setLimit(3 * VmTextHeap::FirstChunkBytes);
  std::vector<uint64_t> blocks;
  for (uint64_t i = 0; i < 256; ++i) {
    blocks.push_back(f.alloc(32));
  }
  uint64_t address = 0;
  std::string error;
  CHECK_FALSE(f.heap.allocate(f.core, 32, address, error));
  CHECK(error == "string memory limit exceeded");
  CHECK(f.heap.stats().chunkBytes == 2 * VmTextHeap::FirstChunkBytes);
  for (const uint64_t block : blocks) {
    f.free(block);
  }
}

TEST_CASE("the memory limit returns free chunks before it refuses") {
  Fixture f;
  f.heap.setLimit(2 * VmTextHeap::FirstChunkBytes);
  // Two classes leave a free chunk each; a third class and a large block need their room.
  f.free(f.alloc(10));
  f.free(f.alloc(100));
  CHECK(f.heap.stats().chunkBytes == 2 * VmTextHeap::FirstChunkBytes);
  const uint64_t third = f.alloc(1000);
  CHECK(f.heap.stats().chunks == 2);
  const uint64_t fourth = f.alloc(2000);
  CHECK(f.heap.stats().chunks == 2);
  CHECK(f.heap.stats().freeChunkBytes == 0);
  // Chunks with a live block are not free: the limit holds.
  uint64_t address = 0;
  std::string error;
  CHECK_FALSE(f.heap.allocate(f.core, 10, address, error));
  CHECK(error == "string memory limit exceeded");
  f.free(third);
  f.free(fourth);
  f.heap.setLimit(100000);
  const uint64_t large = f.alloc(95000);
  CHECK(f.heap.stats().chunks == 1);
  CHECK(f.heap.stats().chunkBytes + f.heap.stats().largeBytes <= 100000);
  f.free(large);
}

TEST_CASE("freeing an invalid or already freed block is rejected") {
  Fixture f;
  const uint64_t block = f.alloc(10);
  const uint64_t keepAlive = f.alloc(10);
  std::string error;
  CHECK_FALSE(f.heap.release(f.core, 12345, error));
  CHECK_FALSE(f.heap.release(f.core, block + 1, error));
  f.free(block);
  error.clear();
  CHECK_FALSE(f.heap.release(f.core, block, error));
  CHECK(error.find("freed twice") != std::string::npos);
  f.free(keepAlive);
}

TEST_CASE("a free in the unused tail of a chunk is rejected") {
  // 40-byte blocks do not divide a 4 KiB chunk: the last 16 bytes belong to no block.
  Fixture f;
  const uint64_t first = f.alloc(33);
  REQUIRE(VmTextHeap::blockSizeFor(33) == 40);
  const uint64_t blocks = VmTextHeap::FirstChunkBytes / 40;
  REQUIRE(blocks * 40 + 16 == VmTextHeap::FirstChunkBytes);
  std::string error;
  CHECK_FALSE(f.heap.release(f.core, first + blocks * 40, error));
  CHECK(error.find("invalid string heap free address") != std::string::npos);
  f.free(first);
}

TEST_CASE("allocation takes the lowest chunk with a free block and chunk accounting stays exact") {
  Fixture f;
  // Fill three chunks of 32-byte blocks (4, 4 and 8 KiB) and free one block in the middle chunk and one in the last.
  const uint64_t perChunk = VmTextHeap::FirstChunkBytes / 32;
  std::vector<uint64_t> blocks;
  for (uint64_t i = 0; i < perChunk * 4; ++i) {
    blocks.push_back(f.alloc(10));
  }
  CHECK(f.heap.stats().chunks == 3);
  const uint64_t inMiddle = blocks[perChunk + 5];
  const uint64_t inLast = blocks[perChunk * 2 + 9];
  f.free(inLast);
  f.free(inMiddle);
  // The lower chunk gets the next block, whatever was freed last.
  CHECK(f.alloc(10) == inMiddle);
  CHECK(f.alloc(10) == inLast);
  CHECK(f.heap.stats().chunks == 3);
  for (const uint64_t block : blocks) {
    f.free(block);
  }
  // Everything is free: the class's first chunk stays, the others return to the VM heap.
  CHECK(f.heap.stats().chunks == 1);
  CHECK(f.heap.stats().chunkBytes == VmTextHeap::FirstChunkBytes);
  CHECK(f.heap.stats().bytesInUse == 0);
}

TEST_CASE("random allocate and free cycles keep the figures and reject every double free") {
  Fixture f;
  uint32_t seed = 12345;
  const auto next = [&seed](uint32_t bound) {
    seed = seed * 1664525u + 1013904223u;
    return (seed >> 8) % bound;
  };
  std::vector<std::pair<uint64_t, uint64_t>> live;  // address, block size
  uint64_t expectedInUse = 0;
  for (int step = 0; step < 4000; ++step) {
    if (live.empty() || next(100) < 55) {
      const uint64_t size = 1 + next(next(10) == 0 ? 90000 : 700);
      const uint64_t address = f.alloc(size);
      live.emplace_back(address, VmTextHeap::blockSizeFor(size));
      expectedInUse += live.back().second;
    } else {
      const size_t pick = next(static_cast<uint32_t>(live.size()));
      const auto block = live[pick];
      live.erase(live.begin() + static_cast<std::ptrdiff_t>(pick));
      f.free(block.first);
      expectedInUse -= block.second;
      std::string error;
      CHECK_FALSE(f.heap.release(f.core, block.first, error));
    }
    if (step % 250 == 0) {
      CHECK(f.heap.stats().bytesInUse == expectedInUse);
    }
  }
  for (const auto &block : live) {
    f.free(block.first);
  }
  const TextHeapStats stats = f.heap.stats();
  CHECK(stats.bytesInUse == 0);
  CHECK(stats.largeBlocks == 0);
  // What is left is free chunks: at most one per size class, within the limit, and nothing else in the VM heap.
  CHECK(stats.chunks <= 45);
  CHECK(stats.chunkBytes == stats.freeChunkBytes);
  CHECK(stats.freeChunkBytes <= VmTextHeap::FreeChunkLimit);
  CHECK(f.core.liveByteCount() == stats.chunkBytes);
}

namespace {
// Calls one of the bulk byte host functions on the fixture's heap.
bool bulk(Fixture &f, const char *name, std::vector<uint64_t> args, uint64_t &result, std::string &error) {
  bool known = false;
  result = 0;
  const bool ok = callTextHostFunction(f.heap, f.core, name, args.data(), result, known, error);
  CHECK(known);
  return ok;
}
} // namespace

TEST_CASE("the bulk byte host functions move, fill, compare and find inside a region") {
  Fixture f;
  const uint64_t block = f.alloc(40);
  uint8_t *bytes = f.core.byteAt(block);
  for (int i = 0; i < 40; ++i) {
    bytes[i] = static_cast<uint8_t>('a' + i % 26);
  }
  uint64_t result = 0;
  std::string error;
  // Overlapping moves in both directions give what memmove gives.
  std::vector<uint8_t> expected(bytes, bytes + 40);
  REQUIRE(bulk(f, "__pstext_move", {block + 2, block, 10}, result, error));
  std::memmove(&expected[2], &expected[0], 10);
  CHECK(std::vector<uint8_t>(bytes, bytes + 40) == expected);
  REQUIRE(bulk(f, "__pstext_move", {block + 1, block + 9, 20}, result, error));
  std::memmove(&expected[1], &expected[9], 20);
  CHECK(std::vector<uint8_t>(bytes, bytes + 40) == expected);
  // fill, compare and find
  REQUIRE(bulk(f, "__pstext_fill", {block + 5, 0x1FF, 4}, result, error));
  CHECK(bytes[5] == 0xFF);
  CHECK(bytes[8] == 0xFF);
  CHECK(bytes[9] != 0xFF);
  REQUIRE(bulk(f, "__pstext_compare", {block, block, 40}, result, error));
  CHECK(result == 0);
  REQUIRE(bulk(f, "__pstext_compare", {block + 4, block + 5, 3}, result, error));
  CHECK(static_cast<int64_t>(result) == (bytes[4] < bytes[5] ? -1 : 1));
  REQUIRE(bulk(f, "__pstext_compare", {block + 5, block + 4, 3}, result, error));
  CHECK(static_cast<int64_t>(result) == (bytes[5] < bytes[4] ? -1 : 1));
  REQUIRE(bulk(f, "__pstext_find", {block, 40, 0xFF}, result, error));
  CHECK(result == 5);
  REQUIRE(bulk(f, "__pstext_find", {block, 5, 0xFF}, result, error));
  CHECK(static_cast<int64_t>(result) == -1);
  f.free(block);
}

TEST_CASE("the bulk byte host functions check their ranges once and ignore empty ones") {
  Fixture f;
  const uint64_t block = f.alloc(40);
  uint64_t result = 0;
  std::string error;
  // A count of 0 touches nothing, whatever the addresses; find reports -1.
  CHECK(bulk(f, "__pstext_move", {12345, 67890, 0}, result, error));
  CHECK(bulk(f, "__pstext_fill", {12345, 1, 0}, result, error));
  CHECK(bulk(f, "__pstext_find", {12345, 0, 1}, result, error));
  CHECK(static_cast<int64_t>(result) == -1);
  // A range that leaves the chunk, or starts at an address that is no byte region, is rejected.
  CHECK_FALSE(bulk(f, "__pstext_fill", {block, 1, VmTextHeap::ChunkBytes + 1}, result, error));
  CHECK(error.find("invalid text range") != std::string::npos);
  CHECK_FALSE(bulk(f, "__pstext_move", {block, 12345, 4}, result, error));
  CHECK_FALSE(bulk(f, "__pstext_compare", {12345, block, 4}, result, error));
  CHECK_FALSE(bulk(f, "__pstext_find", {12345, 4, 1}, result, error));
  f.free(block);
}

TEST_CASE("the VM answers the string heap host functions without bindings") {
  IrModule module;
  module.hostImports.push_back({"__pstext_alloc", {IrHostValueKind::U64}, IrHostValueKind::U64});
  module.hostImports.push_back({"__pstext_free", {IrHostValueKind::U64}, IrHostValueKind::Void});
  module.hostImports.push_back({"__pstext_stat", {IrHostValueKind::U64}, IrHostValueKind::U64});
  IrFunction main;
  main.name = "/main";
  main.instructions = {
      {IrOpcode::PushI64, 40},      {IrOpcode::CallHost, 0},      {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0},     {IrOpcode::PushI32, 9},       {IrOpcode::StoreU8, 0},
      {IrOpcode::Pop, 0},           {IrOpcode::LoadLocal, 0},     {IrOpcode::PushI64, 3},
      {IrOpcode::AddI64, 0},        {IrOpcode::PushI32, 5},       {IrOpcode::StoreU8, 0},
      {IrOpcode::Pop, 0},           {IrOpcode::PushI64, 1},       {IrOpcode::CallHost, 2},
      {IrOpcode::StoreLocal, 1},    {IrOpcode::LoadLocal, 0},     {IrOpcode::LoadU8, 0},
      {IrOpcode::LoadLocal, 0},     {IrOpcode::PushI64, 3},       {IrOpcode::AddI64, 0},
      {IrOpcode::LoadU8, 0},        {IrOpcode::AddI32, 0},        {IrOpcode::LoadLocal, 1},
      {IrOpcode::AddI32, 0},        {IrOpcode::LoadLocal, 0},     {IrOpcode::CallHost, 1},
      {IrOpcode::ReturnI32, 0},
  };
  module.functions.push_back(main);
  module.entryIndex = 0;
  uint64_t result = 0;
  std::string error;
  REQUIRE_MESSAGE(Vm{}.execute(module, result, error, std::vector<std::string_view>{"prog"}), error);
  // 9 + 5 + bytesInUse (a 40-byte block).
  CHECK(static_cast<int32_t>(result) == 9 + 5 + 40);
}

TEST_CASE("a string arena takes blocks and gives them all back at its end") {
  // A request-shaped pattern: 400 blocks of five size classes, all released together at the end of the request.
  constexpr int Requests = 400;
  constexpr int BlocksPerRequest = 400;
  using Clock = std::chrono::steady_clock;
  Fixture f;
  std::string error;
  uint64_t address = 0;
  std::vector<uint64_t> blocks;
  const auto heapStart = Clock::now();
  for (int r = 0; r < Requests; ++r) {
    blocks.clear();
    for (int k = 0; k < BlocksPerRequest; ++k) {
      REQUIRE(f.heap.allocate(f.core, 20 + (k % 5) * 20, address, error));
      blocks.push_back(address);
    }
    for (const uint64_t block : blocks) {
      REQUIRE(f.heap.release(f.core, block, error));
    }
  }
  const auto heapTime = Clock::now() - heapStart;
  const auto arenaStart = Clock::now();
  for (int r = 0; r < Requests; ++r) {
    f.heap.beginArena();
    for (int k = 0; k < BlocksPerRequest; ++k) {
      REQUIRE(f.heap.allocate(f.core, 20 + (k % 5) * 20, address, error));
    }
    REQUIRE_MESSAGE(f.heap.endArena(f.core, error), error);
  }
  const auto arenaTime = Clock::now() - arenaStart;
  MESSAGE("heap " << std::chrono::duration<double, std::milli>(heapTime).count() << " ms, arena "
                  << std::chrono::duration<double, std::milli>(arenaTime).count() << " ms");
  // Measured at about 18x on an idle machine. The times are reported, not compared: a wall-clock comparison failed
  // in a release gate at load 70, and a test must not depend on timing (AGENTS.md, No flaky tests).
  CHECK(f.heap.stats().arenaDepth == 0);
  CHECK(f.heap.stats().bytesInUse == 0);
}

TEST_CASE("arena blocks are not freed one by one, survive nesting, and are invalid after the arena ends") {
  Fixture f;
  std::string error;
  const uint64_t outside = f.alloc(40);
  f.heap.beginArena();
  const uint64_t first = f.alloc(40);
  f.heap.beginArena();
  const uint64_t second = f.alloc(40);
  f.free(second);  // a no-op inside an arena
  CHECK(f.heap.stats().arenaDepth == 2);
  REQUIRE(f.heap.endArena(f.core, error));
  CHECK(f.core.byteAt(first) != nullptr);
  CHECK(f.core.byteAt(second) == nullptr);
  CHECK(f.heap.stats().arenaDepth == 1);
  REQUIRE(f.heap.endArena(f.core, error));
  CHECK(f.core.byteAt(first) == nullptr);
  CHECK(f.core.byteAt(outside) != nullptr);
  CHECK_FALSE(f.heap.endArena(f.core, error));
  f.free(outside);
  CHECK(f.heap.stats().bytesInUse == 0);
}

TEST_CASE("heap statistic 8 counts the live slot allocations") {
  Fixture f;
  uint64_t result = 0;
  std::string error;
  REQUIRE(bulk(f, "__pstext_stat", {8}, result, error));
  CHECK(result == 0);
  uint64_t first = 0;
  uint64_t second = 0;
  REQUIRE(f.core.allocate(3, first, error));
  REQUIRE(f.core.allocate(5, second, error));
  REQUIRE(bulk(f, "__pstext_stat", {8}, result, error));
  CHECK(result == 2);
  REQUIRE(f.core.release(first, error));
  REQUIRE(bulk(f, "__pstext_stat", {8}, result, error));
  CHECK(result == 1);
  REQUIRE(f.core.release(second, error));
  REQUIRE(bulk(f, "__pstext_stat", {8}, result, error));
  CHECK(result == 0);
}

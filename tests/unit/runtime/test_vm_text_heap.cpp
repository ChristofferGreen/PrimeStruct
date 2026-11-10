#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
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

TEST_CASE("an emptied chunk is returned except one kept for reuse") {
  Fixture f;
  const uint64_t perChunk = VmTextHeap::ChunkBytes / 32;
  std::vector<uint64_t> blocks;
  for (uint64_t i = 0; i < perChunk + 1; ++i) {
    blocks.push_back(f.alloc(32));
  }
  CHECK(f.heap.stats().chunks == 2);
  const uint64_t heldWithTwo = f.core.liveByteCount();
  for (const uint64_t block : blocks) {
    f.free(block);
  }
  const TextHeapStats stats = f.heap.stats();
  CHECK(stats.chunks == 1);
  CHECK(stats.bytesInUse == 0);
  CHECK(stats.bytesFree == perChunk * 32);
  CHECK(f.core.liveByteCount() < heldWithTwo);
  CHECK(f.core.liveByteCount() == VmTextHeap::ChunkBytes);
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
  f.heap.setLimit(VmTextHeap::ChunkBytes + 1000);
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
  // 40-byte blocks do not divide a 256 KiB chunk: the last 24 bytes belong to no block.
  Fixture f;
  const uint64_t first = f.alloc(33);
  REQUIRE(VmTextHeap::blockSizeFor(33) == 40);
  const uint64_t blocks = VmTextHeap::ChunkBytes / 40;
  std::string error;
  CHECK_FALSE(f.heap.release(f.core, first + blocks * 40, error));
  CHECK(error.find("invalid string heap free address") != std::string::npos);
  f.free(first);
}

TEST_CASE("allocation takes the lowest chunk with a free block and chunk accounting stays exact") {
  Fixture f;
  // Fill three chunks of 32-byte blocks and free one block in the middle chunk and one in the last.
  const uint64_t perChunk = VmTextHeap::ChunkBytes / 32;
  std::vector<uint64_t> blocks;
  for (uint64_t i = 0; i < perChunk * 3; ++i) {
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
  // Everything is free: one chunk stays, the others return to the VM heap.
  CHECK(f.heap.stats().chunks == 1);
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
  CHECK(f.heap.stats().bytesInUse == 0);
  CHECK(f.heap.stats().largeBlocks == 0);
  CHECK(f.heap.stats().chunks <= 6);
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

#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHeapCore.h"
#include "primec/runtime/VmTextHeap.h"

#include "third_party/doctest.h"

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

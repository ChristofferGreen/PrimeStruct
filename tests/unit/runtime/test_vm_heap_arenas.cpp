#include "third_party/doctest.h"

#include "primec/runtime/VmHeapArenas.h"

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// The per-thread heap arenas of generated programs (docs/MultithreadingPrototype.md, Heaps). The threads
// here meet only at joins, so every outcome is the same on every run; the unit also ran once under ThreadSanitizer
// (recorded with the finished task in docs/todo_archive/).

using primec::callTextHostFunction;
using primec::drainRemoteFrees;
using primec::VmHeapArena;
using primec::VmHeapArenas;
using primec::VmHeapCore;

namespace {

constexpr uint64_t ArenaMask = uint64_t{63} << 41;
// Small reservations for the tests that start threads; the first case uses the real ones.
constexpr uint64_t TestSlotCapacity = uint64_t{1} << 22;
constexpr uint64_t TestByteCapacity = uint64_t{1} << 27;

uint64_t arenaOfAddress(uint64_t address) {
  return (address & ArenaMask) >> 41;
}

// What a generated program does around an allocation: blocks other threads freed are freed first.
struct Owner {
  explicit Owner(VmHeapArena &held) : arena(held) {
  }

  VmHeapArena &arena;
  std::string error;
  bool ok = true;

  void drain() {
    if (arena.core.hasRemoteFrees()) {
      ok = drainRemoteFrees(arena.text, arena.core, error) && ok;
    }
  }
  uint64_t slots(uint64_t count) {
    drain();
    uint64_t address = 0;
    ok = arena.core.allocate(count, address, error) && ok;
    return address;
  }
  uint64_t bytes(uint64_t count) {
    drain();
    uint64_t address = 0;
    ok = arena.core.allocateBytes(count, address, error) && ok;
    return address;
  }
  uint64_t text(uint64_t count) {
    drain();
    uint64_t address = 0;
    ok = arena.text.allocate(arena.core, count, address, error) && ok;
    return address;
  }
  void free(uint64_t address) {
    ok = arena.core.release(address, error) && ok;
  }
  void freeText(uint64_t address) {
    ok = arena.text.release(arena.core, address, error) && ok;
  }
};

} // namespace

TEST_SUITE_BEGIN("primestruct.runtime.heap_arenas");

TEST_CASE("arena 0 keeps the VM's addresses and a second arena has its number in bits 41 to 46") {
  VmHeapCore plain;
  VmHeapArenas arenas;
  VmHeapCore &main = arenas.main().core;
  std::string error;
  uint64_t expectedSlots = 0;
  uint64_t expectedBytes = 0;
  uint64_t slots = 0;
  uint64_t bytes = 0;
  REQUIRE(plain.allocate(3, expectedSlots, error));
  REQUIRE(plain.allocateBytes(100, expectedBytes, error));
  REQUIRE(main.allocate(3, slots, error));
  REQUIRE(main.allocateBytes(100, bytes, error));
  CHECK(slots == expectedSlots);
  CHECK(bytes == expectedBytes);
  *main.slotAt(slots + 2 * VmHeapCore::SlotBytes) = 77;
  *main.byteAt(bytes + 99) = 9;

  // The first other arena moves arena 0's storage into its reservation: addresses and contents stay.
  VmHeapArena *second = arenas.acquire(error);
  REQUIRE(second != nullptr);
  CHECK(second->index == 1);
  REQUIRE(main.slotAt(slots + 2 * VmHeapCore::SlotBytes) != nullptr);
  CHECK(*main.slotAt(slots + 2 * VmHeapCore::SlotBytes) == 77);
  REQUIRE(main.byteAt(bytes + 99) != nullptr);
  CHECK(*main.byteAt(bytes + 99) == 9);
  const uint64_t *storage = main.slots.data();

  // Arena 0 goes on as the VM would.
  REQUIRE(plain.allocate(5, expectedSlots, error));
  REQUIRE(plain.allocateBytes(7, expectedBytes, error));
  uint64_t moreSlots = 0;
  uint64_t moreBytes = 0;
  REQUIRE(main.allocate(5, moreSlots, error));
  REQUIRE(main.allocateBytes(7, moreBytes, error));
  CHECK(moreSlots == expectedSlots);
  CHECK(moreBytes == expectedBytes);
  uint64_t large = 0;
  REQUIRE(main.allocate(uint64_t{1} << 21, large, error));
  CHECK(main.slots.data() == storage); // it no longer moves

  uint64_t otherSlots = 0;
  uint64_t otherBytes = 0;
  REQUIRE(second->core.allocate(2, otherSlots, error));
  REQUIRE(second->core.allocateBytes(70, otherBytes, error));
  CHECK(arenaOfAddress(otherSlots) == 1);
  CHECK(arenaOfAddress(otherBytes) == 1);
  CHECK((otherSlots & ~ArenaMask) == VmHeapCore::Tag);
  CHECK(VmHeapCore::isByteAddress(otherBytes));
  CHECK_FALSE(VmHeapCore::isByteAddress(otherSlots));
  *second->core.slotAt(otherSlots + VmHeapCore::SlotBytes) = 5;
  *second->core.byteAt(otherBytes + 69) = 6;

  // Either arena reads the other's blocks, and only inside them.
  REQUIRE(main.slotAt(otherSlots + VmHeapCore::SlotBytes) != nullptr);
  CHECK(*main.slotAt(otherSlots + VmHeapCore::SlotBytes) == 5);
  CHECK(main.slotAt(otherSlots + 2 * VmHeapCore::SlotBytes) == nullptr);
  REQUIRE(main.byteAt(otherBytes + 69) != nullptr);
  CHECK(*main.byteAt(otherBytes + 69) == 6);
  CHECK(main.byteAt(otherBytes + 70) == nullptr);
  CHECK(main.byteSpan(otherBytes, 70) != nullptr);
  CHECK(main.byteSpan(otherBytes, 71) == nullptr);
  REQUIRE(second->core.slotAt(slots + 2 * VmHeapCore::SlotBytes) != nullptr);
  CHECK(*second->core.slotAt(slots + 2 * VmHeapCore::SlotBytes) == 77);
  CHECK(second->core.byteSpan(bytes, 100) != nullptr);
  CHECK(second->core.byteSpan(bytes, 101) == nullptr);
  // An arena that does not exist.
  CHECK(main.slotAt(otherSlots + (uint64_t{1} << 41)) == nullptr);
  CHECK(main.byteAt(otherBytes + (uint64_t{1} << 41)) == nullptr);
  CHECK_FALSE(main.release(otherSlots + (uint64_t{1} << 41), error));
  CHECK(error.find("invalid heap free address in IR") != std::string::npos);
}

TEST_CASE(
    "four threads allocate, read and free 100000 blocks each and free a quarter of a neighbour's") {
  constexpr int Threads = 4;
  constexpr uint64_t Blocks = 100000;
  VmHeapArenas arenas(TestSlotCapacity, TestByteCapacity);
  std::string error;
  std::vector<VmHeapArena *> held;
  for (int t = 0; t < Threads; ++t) {
    held.push_back(arenas.acquire(error));
    REQUIRE(held.back() != nullptr);
  }
  // Per thread: the blocks it keeps for its neighbour to free (every fourth), and whether every step worked.
  struct Kept {
    std::vector<uint64_t> slots;
    std::vector<uint64_t> bytes;
    std::vector<uint64_t> text;
    bool ok = false;
  };
  std::vector<Kept> kept(Threads);
  const auto slotCount = [](uint64_t i) { return 1 + i % 3; };
  const auto byteCount = [](uint64_t i) { return 1 + i % 90; };
  const auto textCount = [](uint64_t i) { return 8 + i % 120; };

  {
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t) {
      threads.emplace_back([&, t] {
        Owner owner{*held[static_cast<size_t>(t)]};
        Kept &mine = kept[static_cast<size_t>(t)];
        bool ok = true;
        for (uint64_t i = 0; i < Blocks; ++i) {
          const uint64_t slots = owner.slots(slotCount(i));
          const uint64_t bytes = owner.bytes(byteCount(i));
          const uint64_t text = owner.text(textCount(i));
          if (!owner.ok) {
            break;
          }
          uint64_t *slot =
              owner.arena.core.slotAt(slots + (slotCount(i) - 1) * VmHeapCore::SlotBytes);
          uint8_t *byte = owner.arena.core.byteAt(bytes + byteCount(i) - 1);
          uint8_t *letter = owner.arena.core.byteAt(text + textCount(i) - 1);
          if (slot == nullptr || byte == nullptr || letter == nullptr) {
            ok = false;
            break;
          }
          *slot = i * 4 + static_cast<uint64_t>(t);
          *byte = static_cast<uint8_t>(i + static_cast<uint64_t>(t));
          *letter = static_cast<uint8_t>(i * 3 + static_cast<uint64_t>(t));
          ok = ok && *slot == i * 4 + static_cast<uint64_t>(t) &&
               arenaOfAddress(slots) == owner.arena.index &&
               arenaOfAddress(bytes) == owner.arena.index;
          if (i % 4 == 0) {
            mine.slots.push_back(slots);
            mine.bytes.push_back(bytes);
            mine.text.push_back(text);
          } else {
            owner.free(slots);
            owner.free(bytes);
            owner.freeText(text);
          }
        }
        mine.ok = ok && owner.ok;
      });
    }
    for (std::thread &thread : threads) {
      thread.join();
    }
  }
  for (int t = 0; t < Threads; ++t) {
    CHECK(kept[static_cast<size_t>(t)].ok);
    CHECK(held[static_cast<size_t>(t)]->core.liveSlotAllocationCount() == Blocks / 4);
  }

  // Each thread reads and frees its neighbour's kept blocks while the neighbour allocates (and so frees what was
  // freed for it so far) and frees blocks of its own.
  std::vector<int> passed(Threads, 0);
  {
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t) {
      threads.emplace_back([&, t] {
        const int neighbour = (t + 1) % Threads;
        Owner owner{*held[static_cast<size_t>(t)]};
        const Kept &theirs = kept[static_cast<size_t>(neighbour)];
        bool ok = true;
        for (size_t k = 0; k < theirs.slots.size(); ++k) {
          const uint64_t i = k * 4;
          const uint64_t *slot =
              owner.arena.core.slotAt(theirs.slots[k] + (slotCount(i) - 1) * VmHeapCore::SlotBytes);
          const uint8_t *bytes = owner.arena.core.byteSpan(theirs.bytes[k], byteCount(i));
          const uint8_t *letter = owner.arena.core.byteAt(theirs.text[k] + textCount(i) - 1);
          ok = ok && slot != nullptr && *slot == i * 4 + static_cast<uint64_t>(neighbour) &&
               bytes != nullptr &&
               bytes[byteCount(i) - 1] ==
                   static_cast<uint8_t>(i + static_cast<uint64_t>(neighbour)) &&
               letter != nullptr &&
               *letter == static_cast<uint8_t>(i * 3 + static_cast<uint64_t>(neighbour));
          owner.free(theirs.slots[k]);
          owner.free(theirs.bytes[k]);
          owner.freeText(theirs.text[k]);
          const uint64_t own = owner.slots(2);
          const uint64_t ownText = owner.text(40);
          owner.free(own);
          owner.freeText(ownText);
        }
        passed[static_cast<size_t>(t)] = ok && owner.ok ? 1 : 0;
      });
    }
    for (std::thread &thread : threads) {
      thread.join();
    }
  }
  for (int t = 0; t < Threads; ++t) {
    CHECK(passed[static_cast<size_t>(t)] == 1);
    Owner owner{*held[static_cast<size_t>(t)]};
    owner.drain();
    CHECK(owner.ok);
    CHECK(owner.error.empty());
    CHECK(owner.arena.core.liveSlotAllocationCount() == 0);
    CHECK(owner.arena.text.stats().bytesInUse == 0);
    // What is left is the free chunks the string heap keeps for reuse: at most one for each of the nine size
    // classes that text of 8 to 127 bytes uses.
    const primec::TextHeapStats textStats = owner.arena.text.stats();
    CHECK(owner.arena.core.liveByteCount() == textStats.chunkBytes);
    CHECK(textStats.chunkBytes == textStats.freeChunkBytes);
    CHECK(textStats.chunks <= 9);
    CHECK(textStats.freeChunkBytes <= primec::VmTextHeap::FreeChunkLimit);
    // The freed blocks no longer answer, from any arena.
    CHECK(owner.arena.core.slotAt(kept[static_cast<size_t>(t)].slots[0]) == nullptr);
    CHECK(arenas.main().core.slotAt(kept[static_cast<size_t>(t)].slots[0]) == nullptr);
    CHECK(arenas.main().core.byteAt(kept[static_cast<size_t>(t)].bytes[0]) == nullptr);
  }
  CHECK(arenas.main().core.liveSlotAllocationCount() == 0);
}

TEST_CASE("a block freed by two threads faults when its owner frees what was freed for it") {
  VmHeapArenas arenas(TestSlotCapacity, TestByteCapacity);
  std::string error;
  VmHeapArena *first = arenas.acquire(error);
  VmHeapArena *second = arenas.acquire(error);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  Owner main{arenas.main()};
  const uint64_t block = main.slots(4);
  const uint64_t region = main.bytes(48);
  int accepted[2] = {0, 0};
  {
    std::thread one([&] {
      Owner owner{*first};
      owner.free(block);
      owner.free(region);
      accepted[0] = owner.ok ? 1 : 0;
    });
    std::thread two([&] {
      Owner owner{*second};
      owner.free(block);
      accepted[1] = owner.ok ? 1 : 0;
    });
    one.join();
    two.join();
  }
  // A thread cannot tell that another one freed the block too; the owner finds out.
  CHECK(accepted[0] == 1);
  CHECK(accepted[1] == 1);
  CHECK(arenas.main().core.hasRemoteFrees());
  CHECK(arenas.main().core.slotAt(block) != nullptr);
  main.slots(1);
  CHECK_FALSE(main.ok);
  CHECK(main.error.find("invalid heap free address in IR: " + std::to_string(block)) !=
        std::string::npos);
}

TEST_CASE("a freed block's address read from another thread is no block") {
  VmHeapArenas arenas(TestSlotCapacity, TestByteCapacity);
  std::string error;
  VmHeapArena *other = arenas.acquire(error);
  REQUIRE(other != nullptr);
  Owner main{arenas.main()};
  const uint64_t block = main.slots(2);
  const uint64_t region = main.bytes(200);
  const uint64_t edge = main.bytes(10); // shares a granule with `region`
  const uint64_t text = main.text(30);
  int before = 0;
  std::thread([&] {
    VmHeapCore &core = other->core;
    before = core.slotAt(block) != nullptr && core.byteAt(region + 199) != nullptr &&
                     core.byteAt(edge + 9) != nullptr && core.byteAt(edge + 10) == nullptr &&
                     core.byteSpan(region, 200) != nullptr && core.byteAt(text) != nullptr
                 ? 1
                 : 0;
  }).join();
  CHECK(before == 1);
  main.free(block);
  main.free(region);
  main.free(edge);
  main.freeText(text);
  REQUIRE(main.ok);
  int after = 0;
  std::thread([&] {
    VmHeapCore &core = other->core;
    after = core.slotAt(block) == nullptr && core.byteAt(region) == nullptr &&
                    core.byteAt(region + 199) == nullptr && core.byteAt(edge + 9) == nullptr &&
                    core.byteSpan(region, 200) == nullptr
                ? 1
                : 0;
    // Freeing it again from here is accepted, and faults when the owner gets to it.
    std::string ignored;
    after = after == 1 && core.release(region, ignored) ? 1 : 0;
  }).join();
  CHECK(after == 1);
  main.bytes(1);
  CHECK_FALSE(main.ok);
  CHECK(main.error.find("invalid heap free address in IR") != std::string::npos);
}

TEST_CASE(
    "a block grown or measured from another arena moves to it and frees the old one remotely") {
  VmHeapArenas arenas(TestSlotCapacity, TestByteCapacity);
  std::string error;
  VmHeapArena *other = arenas.acquire(error);
  REQUIRE(other != nullptr);
  Owner main{arenas.main()};
  Owner second{*other};
  const uint64_t block = main.slots(2);
  *main.arena.core.slotAt(block + VmHeapCore::SlotBytes) = 41;
  const uint64_t region = main.bytes(10);
  *main.arena.core.byteAt(region + 9) = 42;
  const uint64_t text = main.text(100);
  *main.arena.core.byteAt(text) = 'a';

  uint64_t grown = 0;
  REQUIRE(other->core.reallocate(block, 6, grown, error));
  CHECK(arenaOfAddress(grown) == 1);
  CHECK(*other->core.slotAt(grown + VmHeapCore::SlotBytes) == 41);
  CHECK(*other->core.slotAt(grown + 5 * VmHeapCore::SlotBytes) == 0);
  uint64_t grownBytes = 0;
  REQUIRE(other->core.reallocate(region, 20, grownBytes, error));
  CHECK(arenaOfAddress(grownBytes) == 1);
  CHECK(*other->core.byteAt(grownBytes + 9) == 42);
  uint64_t gone = 0;
  CHECK_FALSE(other->core.reallocate(block + VmHeapCore::SlotBytes, 6, gone, error));
  CHECK(error.find("invalid heap realloc address in IR") != std::string::npos);

  // The string heap answers for a block of another arena, and frees it through its owner.
  uint64_t result = 0;
  bool known = false;
  uint64_t args[1] = {text};
  REQUIRE(callTextHostFunction(
      other->text, other->core, "__pstext_capacity", args, result, known, error));
  CHECK(result == primec::VmTextHeap::blockSizeFor(100));
  REQUIRE(callTextHostFunction(
      other->text, other->core, "__pstext_length", args, result, known, error));
  CHECK(result == 1);
  REQUIRE(
      callTextHostFunction(other->text, other->core, "__pstext_free", args, result, known, error));
  CHECK(main.arena.text.stats().bytesInUse == primec::VmTextHeap::blockSizeFor(100));

  // The old blocks are live until their owner allocates.
  CHECK(main.arena.core.slotAt(block) != nullptr);
  CHECK(main.arena.core.liveSlotAllocationCount() == 1);
  main.slots(1);
  CHECK(main.ok);
  CHECK(main.arena.core.slotAt(block) == nullptr);
  CHECK(main.arena.core.byteAt(region) == nullptr);
  CHECK(main.arena.core.liveSlotAllocationCount() == 1);
  CHECK(main.arena.text.stats().bytesInUse == 0);

  // An arena whose thread ended goes to the next thread, with what was freed for it meanwhile.
  second.free(grown);
  uint64_t stays = second.slots(1);
  arenas.release(other);
  main.free(stays);
  VmHeapArena *again = arenas.acquire(error);
  CHECK(again == other);
  Owner next{*again};
  CHECK(next.arena.core.liveSlotAllocationCount() == 1);
  next.slots(1);
  CHECK(next.ok);
  CHECK(next.arena.core.liveSlotAllocationCount() == 1);
  CHECK(arenas.arenaCount() == 2);
}

TEST_CASE("threads past the last arena share it behind its locks") {
  constexpr uint64_t Blocks = 20000;
  VmHeapArenas arenas(TestSlotCapacity, TestByteCapacity, 2);
  std::string error;
  VmHeapArena *first = arenas.acquire(error);
  VmHeapArena *second = arenas.acquire(error);
  VmHeapArena *third = arenas.acquire(error);
  REQUIRE(first != nullptr);
  CHECK(first->index == 1);
  CHECK(second == first);
  CHECK(third == first);
  CHECK(arenas.arenaCount() == 2);
  int passed[3] = {0, 0, 0};
  std::vector<uint64_t> kept[3];
  {
    std::vector<std::thread> threads;
    for (int t = 0; t < 3; ++t) {
      threads.emplace_back([&, t] {
        Owner owner{*first};
        bool ok = true;
        uint64_t result = 0;
        bool known = false;
        std::string callError;
        for (uint64_t i = 0; i < Blocks; ++i) {
          // A string arena on the shared heap is not opened: another thread's text must not land in it.
          if (i % 1000 == 0) {
            ok = callTextHostFunction(owner.arena.text,
                                      owner.arena.core,
                                      "__pstext_arena_begin",
                                      nullptr,
                                      result,
                                      known,
                                      callError) &&
                 ok;
          }
          const uint64_t slots = owner.slots(1 + i % 3);
          const uint64_t bytes = owner.bytes(1 + i % 90);
          const uint64_t text = owner.text(8 + i % 120);
          if (!owner.ok) {
            break;
          }
          uint64_t *slot = owner.arena.core.slotAt(slots);
          uint8_t *byte = owner.arena.core.byteAt(bytes + i % 90);
          uint8_t *letter = owner.arena.core.byteAt(text);
          if (slot == nullptr || byte == nullptr || letter == nullptr) {
            ok = false;
            break;
          }
          *slot = i;
          *byte = static_cast<uint8_t>(i);
          *letter = static_cast<uint8_t>(t);
          ok = ok && *slot == i && *byte == static_cast<uint8_t>(i) &&
               *letter == static_cast<uint8_t>(t);
          owner.free(slots);
          owner.free(bytes);
          if (i % 1000 == 999) {
            ok = callTextHostFunction(owner.arena.text,
                                      owner.arena.core,
                                      "__pstext_arena_end",
                                      nullptr,
                                      result,
                                      known,
                                      callError) &&
                 ok;
          }
          if (i % 100 == 0) {
            kept[t].push_back(text);
          } else {
            owner.freeText(text);
          }
        }
        passed[t] = ok && owner.ok ? 1 : 0;
      });
    }
    for (std::thread &thread : threads) {
      thread.join();
    }
  }
  Owner owner{*first};
  for (int t = 0; t < 3; ++t) {
    CHECK(passed[t] == 1);
    // Text kept across the arena calls is still there.
    for (const uint64_t text : kept[t]) {
      const uint8_t *letter = owner.arena.core.byteAt(text);
      REQUIRE(letter != nullptr);
      CHECK(*letter == static_cast<uint8_t>(t));
      owner.freeText(text);
    }
  }
  CHECK(owner.ok);
  CHECK(first->core.liveSlotAllocationCount() == 0);
  CHECK(first->text.stats().bytesInUse == 0);
  CHECK(first->text.stats().arenaDepth == 0);
  std::string unbalanced;
  CHECK_FALSE(first->text.endArena(first->core, unbalanced));
  CHECK(unbalanced == "string arena ended without one being open");
}

TEST_CASE("a thread reads the bytes of another thread's long block without the slow path") {
  VmHeapArenas arenas(TestSlotCapacity, TestByteCapacity);
  std::string error;
  VmHeapArena *other = arenas.acquire(error);
  REQUIRE(other != nullptr);
  Owner main{arenas.main()};
  constexpr uint64_t Bytes = 100000;
  const uint64_t region = main.bytes(Bytes);
  REQUIRE(main.ok);
  uint64_t reads = 0;
  uint64_t slow = 0;
  bool allFound = true;
  std::thread([&] {
    VmHeapCore &core = other->core;
    const uint64_t before = core.slowByteReads();
    for (uint64_t i = 0; i < Bytes; ++i) {
      allFound = core.byteAt(region + i) != nullptr && allFound;
      ++reads;
    }
    slow = core.slowByteReads() - before;
    allFound = core.byteAt(region + Bytes) == nullptr && allFound;
  }).join();
  CHECK(allFound);
  CHECK(reads == Bytes);
  // Only the bytes of the two granules the block shares with its neighbours (and the first read, which finds the
  // arena) take the slow path: a count that does not grow with the length of the block.
  CHECK(slow <= 2 * VmHeapCore::GranuleBytes + 1);
  main.free(region);
  REQUIRE(main.ok);
  bool gone = false;
  std::thread([&] {
    gone =
        other->core.byteAt(region) == nullptr && other->core.byteAt(region + Bytes - 1) == nullptr;
  }).join();
  CHECK(gone);
}

TEST_SUITE_END();

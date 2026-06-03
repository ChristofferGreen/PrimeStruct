#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <deque>
#include <vector>

#include <iostream>

static bool psEnsureStack(std::size_t sp, std::size_t required, const char *operation) {
  if (sp < required) {
    std::cerr << "IR stack underflow on " << operation << '\n';
    return false;
  }
  return true;
}

static const char *ps_string_table[] = {
  "",
};

static constexpr std::size_t ps_string_table_count = 0u;

static constexpr std::size_t ps_string_table_lengths[] = {
  0u,
};

static constexpr uint64_t ps_heap_address_tag = 1ull << 63;

struct PsHeapAllocation {
  std::size_t baseIndex = 0;
  std::size_t slotCount = 0;
  bool live = false;
};

static bool psResolveHeapSlot(uint64_t address,
                              const std::vector<uint64_t> &heapSlots,
                              const std::vector<PsHeapAllocation> &heapAllocations,
                              std::size_t &slotIndexOut) {
  if ((address & ps_heap_address_tag) == 0ull || (address % 16ull) != 0ull) {
    return false;
  }
  uint64_t heapAddress = address & ~ps_heap_address_tag;
  uint64_t heapIndex = heapAddress / 16ull;
  if (heapIndex >= heapSlots.size()) {
    return false;
  }
  for (const auto &allocation : heapAllocations) {
    if (!allocation.live) {
      continue;
    }
    uint64_t baseIndex = static_cast<uint64_t>(allocation.baseIndex);
    uint64_t endIndex = baseIndex + static_cast<uint64_t>(allocation.slotCount);
    if (heapIndex >= baseIndex && heapIndex < endIndex) {
      slotIndexOut = static_cast<std::size_t>(heapIndex);
      return true;
    }
  }
  return false;
}

static bool psHeapAlloc(uint64_t slotCount,
                        std::vector<uint64_t> &heapSlots,
                        std::vector<PsHeapAllocation> &heapAllocations,
                        uint64_t &addressOut) {
  if (slotCount == 0ull) {
    addressOut = 0ull;
    return true;
  }
  if (slotCount > static_cast<uint64_t>(heapSlots.max_size() - heapSlots.size())) {
    return false;
  }
  std::size_t baseIndex = heapSlots.size();
  uint64_t maxAddressableIndex = (std::numeric_limits<uint64_t>::max() - ps_heap_address_tag) / 16ull;
  if (baseIndex > maxAddressableIndex) {
    return false;
  }
  heapSlots.resize(baseIndex + static_cast<std::size_t>(slotCount), 0ull);
  heapAllocations.push_back({baseIndex, static_cast<std::size_t>(slotCount), true});
  addressOut = ps_heap_address_tag + static_cast<uint64_t>(baseIndex) * 16ull;
  return true;
}

static bool psHeapFree(uint64_t address,
                       std::vector<uint64_t> &heapSlots,
                       std::vector<PsHeapAllocation> &heapAllocations) {
  if (address == 0ull) {
    return true;
  }
  if ((address & ps_heap_address_tag) == 0ull || (address % 16ull) != 0ull) {
    return false;
  }
  uint64_t heapAddress = address & ~ps_heap_address_tag;
  std::size_t baseIndex = static_cast<std::size_t>(heapAddress / 16ull);
  for (auto &allocation : heapAllocations) {
    if (allocation.baseIndex != baseIndex) {
      continue;
    }
    if (!allocation.live || allocation.baseIndex + allocation.slotCount > heapSlots.size()) {
      return false;
    }
    for (std::size_t i = 0; i < allocation.slotCount; ++i) {
      heapSlots[allocation.baseIndex + i] = 0ull;
    }
    allocation.live = false;
    return true;
  }
  return false;
}

static bool psHeapRealloc(uint64_t address,
                          uint64_t slotCount,
                          std::vector<uint64_t> &heapSlots,
                          std::vector<PsHeapAllocation> &heapAllocations,
                          uint64_t &addressOut) {
  if (address == 0ull) {
    return psHeapAlloc(slotCount, heapSlots, heapAllocations, addressOut);
  }
  if (slotCount == 0ull) {
    if (!psHeapFree(address, heapSlots, heapAllocations)) {
      return false;
    }
    addressOut = 0ull;
    return true;
  }
  if ((address & ps_heap_address_tag) == 0ull || (address % 16ull) != 0ull) {
    return false;
  }
  uint64_t heapAddress = address & ~ps_heap_address_tag;
  std::size_t baseIndex = static_cast<std::size_t>(heapAddress / 16ull);
  for (auto &allocation : heapAllocations) {
    if (allocation.baseIndex != baseIndex) {
      continue;
    }
    if (!allocation.live || allocation.baseIndex + allocation.slotCount > heapSlots.size()) {
      return false;
    }
    std::size_t oldBaseIndex = allocation.baseIndex;
    std::size_t oldSlotCount = allocation.slotCount;
    uint64_t newAddress = 0ull;
    if (!psHeapAlloc(slotCount, heapSlots, heapAllocations, newAddress)) {
      addressOut = 0ull;
      return true;
    }
    std::size_t newBaseIndex = static_cast<std::size_t>((newAddress & ~ps_heap_address_tag) / 16ull);
    std::size_t copySlots = std::min(oldSlotCount, static_cast<std::size_t>(slotCount));
    for (std::size_t i = 0; i < copySlots; ++i) {
      heapSlots[newBaseIndex + i] = heapSlots[oldBaseIndex + i];
    }
    for (std::size_t i = 0; i < oldSlotCount; ++i) {
      heapSlots[oldBaseIndex + i] = 0ull;
    }
    for (auto &candidate : heapAllocations) {
      if (candidate.baseIndex == oldBaseIndex) {
        candidate.live = false;
        break;
      }
    }
    addressOut = newAddress;
    return true;
  }
  return false;
}

struct PsStack {
  explicit PsStack(std::size_t initialSize) : slots(initialSize, 0ull) {}
  uint64_t &operator[](std::size_t index) {
    if (index >= slots.size()) {
      std::size_t newSize = slots.empty() ? 1024ull : slots.size();
      while (index >= newSize) {
        newSize *= 2ull;
      }
      slots.resize(newSize, 0ull);
    }
    return slots[index];
  }
  const uint64_t &operator[](std::size_t index) const {
    return slots[index];
  }
  std::deque<uint64_t> slots;
};

static int64_t ps_fn_0(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);

static int64_t ps_fn_0_chunk_0(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &locals, std::vector<uint64_t> &heapSlots, std::vector<PsHeapAllocation> &heapAllocations, std::size_t &pc, int argc, char **argv, bool &transfer) {
  while (true) {
    switch (pc) {
      case 0: {
        stack[sp++] = 5;
        pc = 1;
        break;
      }
      case 1: {
        if (!psEnsureStack(sp, 1ull, "return")) {
          return 1;
        }
        return static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));
      }
      default:
        transfer = true;
        return 0;
    }
  }
}

static int64_t ps_fn_0(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv) {
  std::vector<uint64_t> locals(0ull, 0ull);
  std::size_t pc = 0;
  while (true) {
    bool transfer = false;
    if (pc < 2ull) {
      int64_t chunkResult = ps_fn_0_chunk_0(stack, sp, locals, heapSlots, heapAllocations, pc, argc, argv, transfer);
      if (!transfer) {
        return chunkResult;
      }
      continue;
    }
    return 0;
  }
}

static int64_t ps_entry_0(int argc, char **argv) {
  PsStack stack(1024ull);
  std::size_t sp = 0;
  std::vector<uint64_t> heapSlots;
  std::vector<PsHeapAllocation> heapAllocations;
  return ps_fn_0(stack, sp, heapSlots, heapAllocations, argc, argv);
}

int main(int argc, char **argv) {
  return static_cast<int>(ps_entry_0(argc, argv));
}

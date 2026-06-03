#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <deque>
#include <vector>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>

static bool psEnsureStack(std::size_t sp, std::size_t required, const char *operation) {
  if (sp < required) {
    std::cerr << "IR stack underflow on " << operation << '\n';
    return false;
  }
  return true;
}

static float psBitsToF32(uint64_t raw) {
  uint32_t bits = static_cast<uint32_t>(raw);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

static uint64_t psF32ToBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return static_cast<uint64_t>(bits);
}

static double psBitsToF64(uint64_t raw) {
  double value = 0.0;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

static uint64_t psF64ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static int32_t psConvertF32ToI32(float value) {
  if (std::isnan(value)) {
    return 0;
  }
  if (value <= static_cast<float>(std::numeric_limits<int32_t>::min())) {
    return std::numeric_limits<int32_t>::min();
  }
  if (value >= static_cast<float>(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
  }
  return static_cast<int32_t>(value);
}

static int32_t psConvertF64ToI32(double value) {
  if (std::isnan(value)) {
    return 0;
  }
  if (value <= static_cast<double>(std::numeric_limits<int32_t>::min())) {
    return std::numeric_limits<int32_t>::min();
  }
  if (value >= static_cast<double>(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
  }
  return static_cast<int32_t>(value);
}

static int64_t psConvertF32ToI64(float value) {
  if (std::isnan(value)) {
    return 0;
  }
  if (value <= static_cast<float>(std::numeric_limits<int64_t>::min())) {
    return std::numeric_limits<int64_t>::min();
  }
  if (value >= static_cast<float>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }
  return static_cast<int64_t>(value);
}

static int64_t psConvertF64ToI64(double value) {
  if (std::isnan(value)) {
    return 0;
  }
  if (value <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
    return std::numeric_limits<int64_t>::min();
  }
  if (value >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }
  return static_cast<int64_t>(value);
}

static uint32_t psWriteAll(int fd, const void *data, std::size_t size) {
  const char *cursor = static_cast<const char *>(data);
  std::size_t remaining = size;
  while (remaining > 0) {
    ssize_t wrote = ::write(fd, cursor, remaining);
    if (wrote < 0) {
      return errno == 0 ? 1u : static_cast<uint32_t>(errno);
    }
    if (wrote == 0) {
      return 5u;
    }
    remaining -= static_cast<std::size_t>(wrote);
    cursor += wrote;
  }
  return 0u;
}

static const char *ps_string_table[] = {
  "payload",
};

static constexpr std::size_t ps_string_table_count = 1u;

static constexpr std::size_t ps_string_table_lengths[] = {
  7u,
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
        if (!psEnsureStack(sp, 2ull, "add")) {
          return 1;
        }
        int32_t right = static_cast<int32_t>(stack[--sp]);
        int32_t left = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = left + right;
        pc = 1;
        break;
      }
      case 1: {
        if (!psEnsureStack(sp, 2ull, "sub")) {
          return 1;
        }
        int32_t right = static_cast<int32_t>(stack[--sp]);
        int32_t left = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = left - right;
        pc = 2;
        break;
      }
      case 2: {
        if (!psEnsureStack(sp, 2ull, "mul")) {
          return 1;
        }
        int32_t right = static_cast<int32_t>(stack[--sp]);
        int32_t left = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = left * right;
        pc = 3;
        break;
      }
      case 3: {
        if (!psEnsureStack(sp, 2ull, "div")) {
          return 1;
        }
        int32_t right = static_cast<int32_t>(stack[--sp]);
        int32_t left = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = left / right;
        pc = 4;
        break;
      }
      case 4: {
        if (!psEnsureStack(sp, 2ull, "add")) {
          return 1;
        }
        int64_t right = static_cast<int64_t>(stack[--sp]);
        int64_t left = static_cast<int64_t>(stack[--sp]);
        stack[sp++] = static_cast<uint64_t>(left + right);
        pc = 5;
        break;
      }
      case 5: {
        if (!psEnsureStack(sp, 2ull, "sub")) {
          return 1;
        }
        int64_t right = static_cast<int64_t>(stack[--sp]);
        int64_t left = static_cast<int64_t>(stack[--sp]);
        stack[sp++] = static_cast<uint64_t>(left - right);
        pc = 6;
        break;
      }
      case 6: {
        if (!psEnsureStack(sp, 2ull, "mul")) {
          return 1;
        }
        int64_t right = static_cast<int64_t>(stack[--sp]);
        int64_t left = static_cast<int64_t>(stack[--sp]);
        stack[sp++] = static_cast<uint64_t>(left * right);
        pc = 7;
        break;
      }
      case 7: {
        if (!psEnsureStack(sp, 2ull, "div")) {
          return 1;
        }
        int64_t right = static_cast<int64_t>(stack[--sp]);
        int64_t left = static_cast<int64_t>(stack[--sp]);
        stack[sp++] = static_cast<uint64_t>(left / right);
        pc = 8;
        break;
      }
      case 8: {
        if (!psEnsureStack(sp, 2ull, "div")) {
          return 1;
        }
        uint64_t right = stack[--sp];
        uint64_t left = stack[--sp];
        stack[sp++] = left / right;
        pc = 9;
        break;
      }
      case 9: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        float right = psBitsToF32(stack[--sp]);
        float left = psBitsToF32(stack[--sp]);
        stack[sp++] = psF32ToBits(left + right);
        pc = 10;
        break;
      }
      case 10: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        float right = psBitsToF32(stack[--sp]);
        float left = psBitsToF32(stack[--sp]);
        stack[sp++] = psF32ToBits(left - right);
        pc = 11;
        break;
      }
      case 11: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        float right = psBitsToF32(stack[--sp]);
        float left = psBitsToF32(stack[--sp]);
        stack[sp++] = psF32ToBits(left * right);
        pc = 12;
        break;
      }
      case 12: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        float right = psBitsToF32(stack[--sp]);
        float left = psBitsToF32(stack[--sp]);
        stack[sp++] = psF32ToBits(left / right);
        pc = 13;
        break;
      }
      case 13: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        double right = psBitsToF64(stack[--sp]);
        double left = psBitsToF64(stack[--sp]);
        stack[sp++] = psF64ToBits(left + right);
        pc = 14;
        break;
      }
      case 14: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        double right = psBitsToF64(stack[--sp]);
        double left = psBitsToF64(stack[--sp]);
        stack[sp++] = psF64ToBits(left - right);
        pc = 15;
        break;
      }
      case 15: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        double right = psBitsToF64(stack[--sp]);
        double left = psBitsToF64(stack[--sp]);
        stack[sp++] = psF64ToBits(left * right);
        pc = 16;
        break;
      }
      case 16: {
        if (!psEnsureStack(sp, 2ull, "float op")) {
          return 1;
        }
        double right = psBitsToF64(stack[--sp]);
        double left = psBitsToF64(stack[--sp]);
        stack[sp++] = psF64ToBits(left / right);
        pc = 17;
        break;
      }
      case 17: {
        if (!psEnsureStack(sp, 2ull, "compare")) {
          return 1;
        }
        int32_t right = static_cast<int32_t>(stack[--sp]);
        int32_t left = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = (left == right) ? 1u : 0u;
        pc = 18;
        break;
      }
      case 18: {
        if (!psEnsureStack(sp, 2ull, "compare")) {
          return 1;
        }
        int64_t right = static_cast<int64_t>(stack[--sp]);
        int64_t left = static_cast<int64_t>(stack[--sp]);
        stack[sp++] = (left == right) ? 1u : 0u;
        pc = 19;
        break;
      }
      case 19: {
        if (!psEnsureStack(sp, 2ull, "compare")) {
          return 1;
        }
        float right = psBitsToF32(stack[--sp]);
        float left = psBitsToF32(stack[--sp]);
        stack[sp++] = (left == right) ? 1u : 0u;
        pc = 20;
        break;
      }
      case 20: {
        if (!psEnsureStack(sp, 2ull, "compare")) {
          return 1;
        }
        double right = psBitsToF64(stack[--sp]);
        double left = psBitsToF64(stack[--sp]);
        stack[sp++] = (left == right) ? 1u : 0u;
        pc = 21;
        break;
      }
      case 21: {
        if (!psEnsureStack(sp, 2ull, "compare")) {
          return 1;
        }
        int32_t right = static_cast<int32_t>(stack[--sp]);
        int32_t left = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = (left == right) ? 1u : 0u;
        pc = 22;
        break;
      }
      case 22: {
        if (!psEnsureStack(sp, 1ull, "negate")) {
          return 1;
        }
        int32_t value = static_cast<int32_t>(stack[sp - 1]);
        stack[sp - 1] = static_cast<uint64_t>(-value);
        pc = 23;
        break;
      }
      case 23: {
        if (!psEnsureStack(sp, 1ull, "negate")) {
          return 1;
        }
        int64_t value = static_cast<int64_t>(stack[sp - 1]);
        stack[sp - 1] = static_cast<uint64_t>(-value);
        pc = 24;
        break;
      }
      case 24: {
        if (!psEnsureStack(sp, 1ull, "negate")) {
          return 1;
        }
        float value = psBitsToF32(stack[sp - 1]);
        stack[sp - 1] = psF32ToBits(-value);
        pc = 25;
        break;
      }
      case 25: {
        if (!psEnsureStack(sp, 1ull, "negate")) {
          return 1;
        }
        double value = psBitsToF64(stack[sp - 1]);
        stack[sp - 1] = psF64ToBits(-value);
        pc = 26;
        break;
      }
      case 26: {
        if (!psEnsureStack(sp, 1ull, "convert")) {
          return 1;
        }
        int32_t value = static_cast<int32_t>(stack[--sp]);
        stack[sp++] = psF32ToBits(static_cast<float>(value));
        pc = 27;
        break;
      }
      case 27: {
        if (!psEnsureStack(sp, 1ull, "convert")) {
          return 1;
        }
        float value = psBitsToF32(stack[--sp]);
        int32_t converted = psConvertF32ToI32(value);
        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(converted));
        pc = 28;
        break;
      }
      case 28: {
        if (!psEnsureStack(sp, 1ull, "convert")) {
          return 1;
        }
        int64_t value = static_cast<int64_t>(stack[--sp]);
        stack[sp++] = psF64ToBits(static_cast<double>(value));
        pc = 29;
        break;
      }
      case 29: {
        if (!psEnsureStack(sp, 1ull, "convert")) {
          return 1;
        }
        double value = psBitsToF64(stack[--sp]);
        int64_t converted = psConvertF64ToI64(value);
        stack[sp++] = static_cast<uint64_t>(converted);
        pc = 30;
        break;
      }
      case 30: {
        if (!psEnsureStack(sp, 1ull, "store")) {
          return 1;
        }
        locals[0] = stack[--sp];
        pc = 31;
        break;
      }
      case 31: {
        if (!psEnsureStack(sp, 1ull, "load indirect")) {
          return 1;
        }
        uint64_t loadIndirectAddress = stack[--sp];
        if ((loadIndirectAddress % 16ull) != 0ull) {
          std::cerr << "unaligned indirect address in IR\n";
          return 1;
        }
        if ((loadIndirectAddress & ps_heap_address_tag) != 0ull) {
          std::size_t loadHeapIndex = 0;
          if (!psResolveHeapSlot(loadIndirectAddress, heapSlots, heapAllocations, loadHeapIndex)) {
            std::cerr << "invalid indirect address in IR\n";
            return 1;
          }
          stack[sp++] = heapSlots[loadHeapIndex];
        } else {
          uint64_t loadIndirectIndex = loadIndirectAddress / 16ull;
          if (loadIndirectIndex >= 1ull) {
            std::cerr << "invalid indirect address in IR\n";
            return 1;
          }
          stack[sp++] = locals[loadIndirectIndex];
        }
        pc = 32;
        break;
      }
      case 32: {
        if (!psEnsureStack(sp, 2ull, "store indirect")) {
          return 1;
        }
        uint64_t storeIndirectValue = stack[--sp];
        uint64_t storeIndirectAddress = stack[--sp];
        if ((storeIndirectAddress % 16ull) != 0ull) {
          std::cerr << "unaligned indirect address in IR\n";
          return 1;
        }
        if ((storeIndirectAddress & ps_heap_address_tag) != 0ull) {
          std::size_t storeHeapIndex = 0;
          if (!psResolveHeapSlot(storeIndirectAddress, heapSlots, heapAllocations, storeHeapIndex)) {
            std::cerr << "invalid indirect address in IR\n";
            return 1;
          }
          heapSlots[storeHeapIndex] = storeIndirectValue;
        } else {
          uint64_t storeIndirectIndex = storeIndirectAddress / 16ull;
          if (storeIndirectIndex >= 1ull) {
            std::cerr << "invalid indirect address in IR\n";
            return 1;
          }
          locals[storeIndirectIndex] = storeIndirectValue;
        }
        stack[sp++] = storeIndirectValue;
        pc = 33;
        break;
      }
      case 33: {
        if (!psEnsureStack(sp, 1ull, "heap alloc")) {
          return 1;
        }
        uint64_t heapAllocSlotCount = stack[--sp];
        uint64_t heapAllocAddress = 0ull;
        if (!psHeapAlloc(heapAllocSlotCount, heapSlots, heapAllocations, heapAllocAddress)) {
          std::cerr << "VM heap allocation overflow\n";
          return 1;
        }
        stack[sp++] = heapAllocAddress;
        pc = 34;
        break;
      }
      case 34: {
        if (!psEnsureStack(sp, 1ull, "heap free")) {
          return 1;
        }
        if (!psHeapFree(stack[--sp], heapSlots, heapAllocations)) {
          std::cerr << "invalid heap free address in IR\n";
          return 1;
        }
        pc = 35;
        break;
      }
      case 35: {
        if (!psEnsureStack(sp, 2ull, "heap realloc")) {
          return 1;
        }
        uint64_t heapReallocSlotCount = stack[--sp];
        uint64_t heapReallocAddress = stack[--sp];
        uint64_t heapReallocResult = 0ull;
        if (!psHeapRealloc(heapReallocAddress, heapReallocSlotCount, heapSlots, heapAllocations, heapReallocResult)) {
          std::cerr << "invalid heap realloc address in IR\n";
          return 1;
        }
        stack[sp++] = heapReallocResult;
        pc = 36;
        break;
      }
      case 36: {
        if (!psEnsureStack(sp, 1ull, "dup")) {
          return 1;
        }
        uint64_t dupValue = stack[sp - 1];
        stack[sp++] = dupValue;
        pc = 37;
        break;
      }
      case 37: {
        if (!psEnsureStack(sp, 1ull, "jump")) {
          return 1;
        }
        int64_t cond = static_cast<int64_t>(stack[--sp]);
        pc = (cond == 0) ? 0 : 38;
        break;
      }
      case 38: {
        if (!psEnsureStack(sp, 1ull, "print")) {
          return 1;
        }
        std::cout << static_cast<int32_t>(stack[--sp]);
        pc = 39;
        break;
      }
      case 39: {
        if (!psEnsureStack(sp, 1ull, "print")) {
          return 1;
        }
        std::cout << static_cast<int64_t>(stack[--sp]);
        pc = 40;
        break;
      }
      case 40: {
        if (!psEnsureStack(sp, 1ull, "print")) {
          return 1;
        }
        std::cout << stack[--sp];
        pc = 41;
        break;
      }
      case 41: {
        if (!psEnsureStack(sp, 1ull, "print")) {
          return 1;
        }
        uint64_t stringIndex = stack[--sp];
        if (stringIndex >= ps_string_table_count) {
          std::cerr << "invalid string index in IR\n";
          return 1;
        }
        std::cout << ps_string_table[stringIndex];
        pc = 42;
        break;
      }
      case 42: {
        if (!psEnsureStack(sp, 1ull, "print")) {
          return 1;
        }
        int64_t indexValue = static_cast<int64_t>(stack[--sp]);
        if (indexValue < 0 || indexValue >= static_cast<int64_t>(argc)) {
          std::cerr << "invalid argv index in IR\n";
          return 1;
        }
        std::cout << argv[indexValue];
        pc = 43;
        break;
      }
      case 43: {
        if (!psEnsureStack(sp, 1ull, "print")) {
          return 1;
        }
        int64_t indexValue = static_cast<int64_t>(stack[--sp]);
        if (indexValue >= 0 && indexValue < static_cast<int64_t>(argc)) {
          std::cout << argv[indexValue];
        }
        pc = 44;
        break;
      }
      case 44: {
        if (!psEnsureStack(sp, 1ull, "file close")) {
          return 1;
        }
        uint64_t closeHandle = stack[--sp];
        int closeFd = static_cast<int>(closeHandle & 0xffffffffu);
        int closeRc = ::close(closeFd);
        uint32_t closeErr = (closeRc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;
        stack[sp++] = static_cast<uint64_t>(closeErr);
        pc = 45;
        break;
      }
      case 45: {
        if (!psEnsureStack(sp, 1ull, "file flush")) {
          return 1;
        }
        uint64_t flushHandle = stack[--sp];
        int flushFd = static_cast<int>(flushHandle & 0xffffffffu);
        int flushRc = ::fsync(flushFd);
        uint32_t flushErr = (flushRc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;
        stack[sp++] = static_cast<uint64_t>(flushErr);
        pc = 46;
        break;
      }
      case 46: {
        if (!psEnsureStack(sp, 2ull, "file write")) {
          return 1;
        }
        int64_t fileI32Value = static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));
        uint64_t fileI32Handle = stack[--sp];
        int fileI32Fd = static_cast<int>(fileI32Handle & 0xffffffffu);
        std::string fileI32Text = std::to_string(fileI32Value);
        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileI32Fd, fileI32Text.data(), fileI32Text.size()));
        pc = 47;
        break;
      }
      case 47: {
        if (!psEnsureStack(sp, 2ull, "file write")) {
          return 1;
        }
        int64_t fileI64Value = static_cast<int64_t>(stack[--sp]);
        uint64_t fileI64Handle = stack[--sp];
        int fileI64Fd = static_cast<int>(fileI64Handle & 0xffffffffu);
        std::string fileI64Text = std::to_string(fileI64Value);
        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileI64Fd, fileI64Text.data(), fileI64Text.size()));
        pc = 48;
        break;
      }
      case 48: {
        if (!psEnsureStack(sp, 2ull, "file write")) {
          return 1;
        }
        uint64_t fileU64Value = stack[--sp];
        uint64_t fileU64Handle = stack[--sp];
        int fileU64Fd = static_cast<int>(fileU64Handle & 0xffffffffu);
        std::string fileU64Text = std::to_string(static_cast<unsigned long long>(fileU64Value));
        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileU64Fd, fileU64Text.data(), fileU64Text.size()));
        pc = 49;
        break;
      }
      case 49: {
        if (!psEnsureStack(sp, 1ull, "file write")) {
          return 1;
        }
        uint64_t fileStringHandle = stack[--sp];
        int fileStringFd = static_cast<int>(fileStringHandle & 0xffffffffu);
        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileStringFd, ps_string_table[0], 7ull));
        pc = 50;
        break;
      }
      case 50: {
        if (!psEnsureStack(sp, 2ull, "file write")) {
          return 1;
        }
        uint8_t fileByteValue = static_cast<uint8_t>(stack[--sp] & 0xffu);
        uint64_t fileByteHandle = stack[--sp];
        int fileByteFd = static_cast<int>(fileByteHandle & 0xffffffffu);
        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileByteFd, &fileByteValue, 1));
        pc = 51;
        break;
      }
      case 51: {
        if (!psEnsureStack(sp, 1ull, "file write")) {
          return 1;
        }
        uint64_t fileLineHandle = stack[--sp];
        int fileLineFd = static_cast<int>(fileLineHandle & 0xffffffffu);
        char fileLineValue = '\n';
        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileLineFd, &fileLineValue, 1));
        pc = 52;
        break;
      }
      case 52: {
        if (!psEnsureStack(sp, 1ull, "string byte")) {
          return 1;
        }
        uint64_t stringByteIndex = stack[--sp];
        if (stringByteIndex >= 7ull) {
          std::cerr << "string index out of bounds in IR\n";
          return 1;
        }
        uint8_t byte = static_cast<uint8_t>(ps_string_table[0][stringByteIndex]);
        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte)));
        pc = 53;
        break;
      }
      case 53: {
        if (!psEnsureStack(sp, 1ull, "return")) {
          return 1;
        }
        return static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));
      }
      case 54: {
        if (!psEnsureStack(sp, 1ull, "return")) {
          return 1;
        }
        return static_cast<int64_t>(stack[--sp]);
      }
      case 55: {
        if (!psEnsureStack(sp, 1ull, "return")) {
          return 1;
        }
        return static_cast<int64_t>(static_cast<uint32_t>(stack[--sp]));
      }
      case 56: {
        if (!psEnsureStack(sp, 1ull, "return")) {
          return 1;
        }
        return static_cast<int64_t>(stack[--sp]);
      }
      case 57: {
        return 0;
      }
      default:
        transfer = true;
        return 0;
    }
  }
}

static int64_t ps_fn_0(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv) {
  std::vector<uint64_t> locals(1ull, 0ull);
  std::size_t pc = 0;
  while (true) {
    bool transfer = false;
    if (pc < 58ull) {
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

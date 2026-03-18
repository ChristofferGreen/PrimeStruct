#include "VmHeapHelpers.h"

#include <algorithm>
#include <limits>

namespace primec::vm_detail {
namespace {

constexpr uint64_t kVmHeapAddressTag = 1ull << 63;

bool isVmHeapAddress(uint64_t address) {
  return (address & kVmHeapAddressTag) != 0;
}

} // namespace

bool resolveIndirectAddress(uint64_t address,
                            uint64_t slotBytes,
                            std::vector<uint64_t> &locals,
                            std::vector<uint64_t> &heapSlots,
                            std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                            uint64_t *&slotOut,
                            std::string &error) {
  if (address % slotBytes != 0) {
    error = "unaligned indirect address in IR: " + std::to_string(address);
    return false;
  }
  if (isVmHeapAddress(address)) {
    const uint64_t heapAddress = address & ~kVmHeapAddressTag;
    const uint64_t index = heapAddress / slotBytes;
    if (index >= heapSlots.size()) {
      error = "invalid indirect address in IR: " + std::to_string(address);
      return false;
    }
    for (const auto &allocation : heapAllocations) {
      if (!allocation.live) {
        continue;
      }
      const uint64_t baseIndex = static_cast<uint64_t>(allocation.baseIndex);
      const uint64_t endIndex = baseIndex + static_cast<uint64_t>(allocation.slotCount);
      if (index >= baseIndex && index < endIndex) {
        slotOut = &heapSlots[static_cast<size_t>(index)];
        return true;
      }
    }
    error = "invalid indirect address in IR: " + std::to_string(address);
    return false;
  }
  const uint64_t index = address / slotBytes;
  if (index >= locals.size()) {
    error = "invalid indirect address in IR: " + std::to_string(address);
    return false;
  }
  slotOut = &locals[static_cast<size_t>(index)];
  return true;
}

bool allocateVmHeapSlots(uint64_t slotCount,
                         uint64_t slotBytes,
                         std::vector<uint64_t> &heapSlots,
                         std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                         uint64_t &addressOut,
                         std::string &error) {
  if (slotCount == 0) {
    addressOut = 0;
    return true;
  }
  if (slotCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
    error = "VM heap allocation overflow";
    return false;
  }
  const size_t baseIndex = heapSlots.size();
  const size_t allocationSlots = static_cast<size_t>(slotCount);
  if (allocationSlots > heapSlots.max_size() - baseIndex) {
    error = "VM heap allocation overflow";
    return false;
  }
  const uint64_t maxAddressableIndex = (std::numeric_limits<uint64_t>::max() - kVmHeapAddressTag) / slotBytes;
  if (baseIndex > maxAddressableIndex) {
    error = "VM heap allocation overflow";
    return false;
  }
  heapSlots.resize(baseIndex + allocationSlots, 0);
  heapAllocations.push_back({baseIndex, allocationSlots, true});
  addressOut = kVmHeapAddressTag + static_cast<uint64_t>(baseIndex) * slotBytes;
  return true;
}

bool freeVmHeapSlots(uint64_t address,
                     uint64_t slotBytes,
                     std::vector<uint64_t> &heapSlots,
                     std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                     std::string &error) {
  if (address == 0) {
    return true;
  }
  if (!isVmHeapAddress(address) || address % slotBytes != 0) {
    error = "invalid heap free address in IR: " + std::to_string(address);
    return false;
  }
  const uint64_t heapAddress = address & ~kVmHeapAddressTag;
  const uint64_t baseIndex = heapAddress / slotBytes;
  for (auto &allocation : heapAllocations) {
    if (allocation.baseIndex != baseIndex) {
      continue;
    }
    if (!allocation.live) {
      error = "invalid heap free address in IR: " + std::to_string(address);
      return false;
    }
    const size_t endIndex = allocation.baseIndex + allocation.slotCount;
    if (endIndex > heapSlots.size()) {
      error = "invalid heap free address in IR: " + std::to_string(address);
      return false;
    }
    for (size_t index = allocation.baseIndex; index < endIndex; ++index) {
      heapSlots[index] = 0;
    }
    allocation.live = false;
    return true;
  }
  error = "invalid heap free address in IR: " + std::to_string(address);
  return false;
}

bool reallocVmHeapSlots(uint64_t address,
                        uint64_t slotCount,
                        uint64_t slotBytes,
                        std::vector<uint64_t> &heapSlots,
                        std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                        uint64_t &addressOut,
                        std::string &error) {
  if (address == 0) {
    return allocateVmHeapSlots(slotCount, slotBytes, heapSlots, heapAllocations, addressOut, error);
  }
  if (slotCount == 0) {
    if (!freeVmHeapSlots(address, slotBytes, heapSlots, heapAllocations, error)) {
      return false;
    }
    addressOut = 0;
    return true;
  }
  if (!isVmHeapAddress(address) || address % slotBytes != 0) {
    error = "invalid heap realloc address in IR: " + std::to_string(address);
    return false;
  }
  const uint64_t heapAddress = address & ~kVmHeapAddressTag;
  const uint64_t baseIndex = heapAddress / slotBytes;
  for (auto &allocation : heapAllocations) {
    if (allocation.baseIndex != baseIndex) {
      continue;
    }
    if (!allocation.live) {
      error = "invalid heap realloc address in IR: " + std::to_string(address);
      return false;
    }
    const size_t endIndex = allocation.baseIndex + allocation.slotCount;
    if (endIndex > heapSlots.size()) {
      error = "invalid heap realloc address in IR: " + std::to_string(address);
      return false;
    }
    const size_t oldBaseIndex = allocation.baseIndex;
    const size_t oldSlotCount = allocation.slotCount;

    uint64_t newAddress = 0;
    if (!allocateVmHeapSlots(slotCount, slotBytes, heapSlots, heapAllocations, newAddress, error)) {
      return false;
    }
    const uint64_t newBaseIndex = (newAddress & ~kVmHeapAddressTag) / slotBytes;
    const size_t copySlots = std::min(oldSlotCount, static_cast<size_t>(slotCount));
    for (size_t index = 0; index < copySlots; ++index) {
      heapSlots[static_cast<size_t>(newBaseIndex) + index] = heapSlots[oldBaseIndex + index];
    }
    for (size_t index = oldBaseIndex; index < endIndex; ++index) {
      heapSlots[index] = 0;
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
  error = "invalid heap realloc address in IR: " + std::to_string(address);
  return false;
}

} // namespace primec::vm_detail

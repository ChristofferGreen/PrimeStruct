#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::vm_detail {

// Run-time strings of the VM (TODO-5364; design in docs/PrimeStruct.md,
// "VM-owned dynamic strings"). A dynamic string index has bit 63 set:
// DynamicStringTag | generation << 32 | slot. Module-table indices (bit 63
// clear) are unchanged.
inline constexpr uint64_t DynamicStringTag = 1ull << 63;

class VmStringHeap {
public:
  // Stores `text` and returns its dynamic index.
  uint64_t create(std::string text);
  // Invalidates `index` and frees its slot; later lookups of that index fail.
  bool release(uint64_t index);
  // nullptr when `index` is not a live dynamic index of this heap.
  const std::string *find(uint64_t index) const;
  size_t liveCount() const { return liveCount_; }

private:
  struct Slot {
    std::string text;
    uint32_t generation = 0;
    bool live = false;
  };
  std::vector<Slot> slots_;
  std::vector<uint32_t> freeSlots_;
  size_t liveCount_ = 0;
};

// The one place VM code turns a string index into text: module-table indices
// and dynamic indices (when `heap` is given). Faults with a diagnostic otherwise.
bool resolveVmString(const IrModule &module,
                     const VmStringHeap *heap,
                     uint64_t index,
                     const std::string *&textOut,
                     std::string &error);

} // namespace primec::vm_detail

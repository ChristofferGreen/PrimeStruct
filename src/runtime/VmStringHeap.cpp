#include "primec/runtime/VmStringHeap.h"

#include <utility>

namespace primec::vm_detail {

namespace {
constexpr uint32_t GenerationMask = 0x7fffffffu;

uint64_t makeIndex(uint32_t generation, uint32_t slot) {
  return DynamicStringTag | (static_cast<uint64_t>(generation & GenerationMask) << 32) | slot;
}
} // namespace

uint64_t VmStringHeap::create(std::string text) {
  uint32_t slot = 0;
  if (!freeSlots_.empty()) {
    slot = freeSlots_.back();
    freeSlots_.pop_back();
  } else {
    slot = static_cast<uint32_t>(slots_.size());
    slots_.emplace_back();
  }
  Slot &entry = slots_[slot];
  entry.text = std::move(text);
  entry.live = true;
  ++liveCount_;
  return makeIndex(entry.generation, slot);
}

const std::string *VmStringHeap::find(uint64_t index) const {
  if ((index & DynamicStringTag) == 0) {
    return nullptr;
  }
  const uint32_t slot = static_cast<uint32_t>(index & 0xffffffffull);
  const uint32_t generation = static_cast<uint32_t>((index >> 32) & GenerationMask);
  if (slot >= slots_.size()) {
    return nullptr;
  }
  const Slot &entry = slots_[slot];
  if (!entry.live || (entry.generation & GenerationMask) != generation) {
    return nullptr;
  }
  return &entry.text;
}

bool VmStringHeap::release(uint64_t index) {
  if (find(index) == nullptr) {
    return false;
  }
  const uint32_t slot = static_cast<uint32_t>(index & 0xffffffffull);
  Slot &entry = slots_[slot];
  entry.live = false;
  entry.text.clear();
  entry.text.shrink_to_fit();
  entry.generation = (entry.generation + 1) & GenerationMask;
  freeSlots_.push_back(slot);
  --liveCount_;
  return true;
}

bool resolveVmStringSlow(const IrModule &module,
                         const VmStringHeap *heap,
                         uint64_t index,
                         const std::string *&textOut,
                         std::string &error) {
  if ((index & DynamicStringTag) != 0) {
    const std::string *text = heap != nullptr ? heap->find(index) : nullptr;
    if (text == nullptr) {
      error = "invalid dynamic string index in IR";
      return false;
    }
    textOut = text;
    return true;
  }
  if (index >= module.stringTable.size()) {
    error = "invalid string index in IR";
    return false;
  }
  textOut = &module.stringTable[static_cast<size_t>(index)];
  return true;
}

} // namespace primec::vm_detail

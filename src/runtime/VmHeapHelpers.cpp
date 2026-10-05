#include "VmHeapHelpers.h"

namespace primec::vm_detail {

bool resolveIndirectAddress(uint64_t address,
                            uint64_t slotBytes,
                            std::vector<uint64_t> &locals,
                            VmHeapCore &heap,
                            uint64_t *&slotOut,
                            std::string &error) {
  if (address % slotBytes != 0) {
    error = "unaligned indirect address in IR: " + std::to_string(address);
    return false;
  }
  if ((address & VmHeapCore::Tag) != 0) {
    slotOut = heap.slotAt(address);
    if (slotOut == nullptr) {
      error = "invalid indirect address in IR: " + std::to_string(address);
      return false;
    }
    return true;
  }
  const uint64_t index = address / slotBytes;
  if (index >= locals.size()) {
    error = "invalid indirect address in IR: " + std::to_string(address);
    return false;
  }
  slotOut = &locals[static_cast<size_t>(index)];
  return true;
}

} // namespace primec::vm_detail

#include "VmHeapHelpers.h"

namespace primec::vm_detail {

bool resolveIndirectAddress(uint64_t address,
                            uint64_t slotBytes,
                            std::vector<uint64_t> &locals,
                            VmHeapCore &heap,
                            uint64_t *&slotOut,
                            std::string &error) {
  if (VmHeapCore::isByteAddress(address)) {
    error = "slot access to a byte region in IR: " + std::to_string(address);
    return false;
  }
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

namespace {

bool byteAddressFault(uint64_t address, std::string &error) {
  if ((address & VmHeapCore::Tag) != 0 && !VmHeapCore::isByteAddress(address)) {
    error = "byte access to a slot region in IR: " + std::to_string(address);
  } else {
    error = "invalid byte address in IR: " + std::to_string(address);
  }
  return false;
}

} // namespace

bool loadHeapByte(uint64_t address,
                  uint64_t slotBytes,
                  std::vector<uint64_t> &locals,
                  VmHeapCore &heap,
                  uint8_t &value,
                  std::string &error) {
  if (!VmHeapCore::isByteAddress(address)) {
    // A pointer to a u8 local or a u8 slot field: the byte is the slot's low byte.
    uint64_t *slot = nullptr;
    if (!resolveIndirectAddress(address, slotBytes, locals, heap, slot, error)) {
      return false;
    }
    value = static_cast<uint8_t>(*slot & 0xFFull);
    return true;
  }
  const uint8_t *byte = heap.byteAt(address);
  if (byte == nullptr) {
    return byteAddressFault(address, error);
  }
  value = *byte;
  return true;
}

bool storeHeapByte(uint64_t address,
                   uint64_t slotBytes,
                   std::vector<uint64_t> &locals,
                   VmHeapCore &heap,
                   uint8_t value,
                   std::string &error) {
  if (!VmHeapCore::isByteAddress(address)) {
    uint64_t *slot = nullptr;
    if (!resolveIndirectAddress(address, slotBytes, locals, heap, slot, error)) {
      return false;
    }
    *slot = value;
    return true;
  }
  uint8_t *byte = heap.byteAt(address);
  if (byte == nullptr) {
    return byteAddressFault(address, error);
  }
  *byte = value;
  return true;
}

} // namespace primec::vm_detail

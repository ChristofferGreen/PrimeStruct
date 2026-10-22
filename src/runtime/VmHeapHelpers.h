#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/runtime/VmHeapCore.h"

namespace primec::vm_detail {

// The slot an indirect address names: a heap slot (tagged address, see VmHeapCore) or a local
// of the current frame (byte offset into `locals`), or the VM's fault message.
bool resolveIndirectAddress(uint64_t address,
                            uint64_t slotBytes,
                            std::vector<uint64_t> &locals,
                            VmHeapCore &heap,
                            uint64_t *&slotOut,
                            std::string &error);

// The byte a byte-region address names (HeapAllocBytes, LoadU8, StoreU8), or the VM's fault
// message: a slot address, a frame address and a stale or out-of-range byte address all fault.
// Byte access: a byte-region address reads or writes that byte; any other address must be an aligned
// slot (a local or heap slot holding a u8) and reads or writes the slot's low byte.
bool loadHeapByte(uint64_t address,
                  uint64_t slotBytes,
                  std::vector<uint64_t> &locals,
                  VmHeapCore &heap,
                  uint8_t &value,
                  std::string &error);
bool storeHeapByte(uint64_t address,
                   uint64_t slotBytes,
                   std::vector<uint64_t> &locals,
                   VmHeapCore &heap,
                   uint8_t value,
                   std::string &error);

} // namespace primec::vm_detail

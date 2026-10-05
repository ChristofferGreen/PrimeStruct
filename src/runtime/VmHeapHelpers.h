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

} // namespace primec::vm_detail

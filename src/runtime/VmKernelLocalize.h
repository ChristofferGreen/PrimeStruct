#pragma once

#include <cstdint>
#include <vector>

#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

// A local address names a slot of some live frame (docs/spec/vm-design.md, Real calls). Rewrites it to an address
// relative to that frame and selects its locals; a heap address, or a local address outside every live frame, is
// left as it is against the current frame's locals (the host then faults on the latter).
inline void localizeVmAddress(std::vector<VmKernelFrame> &frames,
                              uint64_t slotBytes,
                              uint64_t &address,
                              std::vector<uint64_t> *&locals) {
  VmKernelFrame &top = frames.back();
  locals = &top.locals;
  if ((address & (uint64_t{1} << 63)) != 0) {
    return;
  }
  const uint64_t index = address / slotBytes;
  for (size_t i = frames.size(); i-- > 0;) {
    VmKernelFrame &frame = frames[i];
    if (index >= frame.localsBase) {
      if (index - frame.localsBase < frame.locals.size()) {
        address -= static_cast<uint64_t>(frame.localsBase) * slotBytes;
        locals = &frame.locals;
      }
      return;
    }
  }
}

} // namespace primec::vm_detail

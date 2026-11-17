#pragma once

#include <cstddef>
#include <cstdint>

namespace primec::vm_detail {

// A task handle (docs/spec/vm-design.md, Tasks): a tag in bits 48-62 (bit 63 clear, so it is never a heap address),
// the slot's generation in bits 32-47 and the task slot in bits 0-31. A slot is reused once its task was waited, with
// the next generation. The deterministic scheduler and the threads mode share the encoding.
constexpr uint64_t TaskHandleTag = uint64_t{0x7A5C} << 48;
constexpr uint64_t TaskHandleTagMask = uint64_t{0xFFFF} << 48;

constexpr uint64_t makeTaskHandle(uint16_t generation, size_t slot) {
  return TaskHandleTag | (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(slot);
}

} // namespace primec::vm_detail

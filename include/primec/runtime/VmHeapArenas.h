#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "primec/runtime/VmHeapCore.h"
#include "primec/runtime/VmTextHeap.h"

namespace primec {

// The heaps of a program whose tasks run on threads (docs/MultithreadingPrototype.md, Heaps): one arena per
// thread, each a VmHeapCore and a VmTextHeap, found by the arena number in address bits 41 to 46. The VM does not
// use it (one arena, no locks); the optimized C++ backend pastes it into generated programs after VmTextHeapSource.
// The body lives in VmHeapArenasBody.inc and is expanded twice, like VmHeapCore: here as code (for the unit tests)
// and as the source text primec::VmHeapArenasSource.
#define PRIMEC_VM_HEAP_ARENAS_BODY(...) __VA_ARGS__
#include "primec/runtime/VmHeapArenasBody.inc"
#undef PRIMEC_VM_HEAP_ARENAS_BODY

#define PRIMEC_VM_HEAP_ARENAS_BODY(...)                                                            \
  inline constexpr const char *VmHeapArenasSource = #__VA_ARGS__;
#include "primec/runtime/VmHeapArenasBody.inc"
#undef PRIMEC_VM_HEAP_ARENAS_BODY

} // namespace primec

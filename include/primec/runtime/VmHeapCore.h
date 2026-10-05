#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// The VM heap (docs/spec/vm-design.md): heap addresses, allocation with reuse of freed memory,
// and the checks every runtime that mirrors the VM's heap makes. The class body lives in
// VmHeapCoreBody.inc and is expanded twice: here as code (primec::VmHeapCore), and as the
// source text primec::VmHeapCoreSource, which the C++ backends paste into generated programs
// (after including the headers above) so their heaps behave exactly like the VM's.

namespace primec {

#define PRIMEC_VM_HEAP_CORE_BODY(...) __VA_ARGS__
#include "primec/runtime/VmHeapCoreBody.inc"
#undef PRIMEC_VM_HEAP_CORE_BODY

#define PRIMEC_VM_HEAP_CORE_BODY(...) inline constexpr const char *VmHeapCoreSource = #__VA_ARGS__;
#include "primec/runtime/VmHeapCoreBody.inc"
#undef PRIMEC_VM_HEAP_CORE_BODY

} // namespace primec

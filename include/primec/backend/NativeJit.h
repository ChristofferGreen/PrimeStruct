#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// In-process native execution for the VM (Linux x86_64): the module is compiled by the native
// backend and run as machine code instead of being interpreted, observably the same. Integer, f64
// and f32 arithmetic, comparisons, branches, calls, returns, prints of numbers and module strings,
// argc, string bytes and indirect loads and stores (frame and heap addresses, with the VM's
// address values) are machine code; the heap's allocation, files, prints of argv and dynamic
// strings, dynamic string bytes and the float conversions the code leaves out
// call into the runtime, which runs them with the VM's own handlers. The VM's runtime faults
// (division by zero, string, indirect-address and heap errors, more than 4096 frames, a missing
// return, I/O handler errors) are reported with the VM's messages. Modules importing host
// functions stay on the interpreter.
struct NativeJitResult {
  bool executed = false; // false: nothing ran, `reason` says why; run the interpreter instead
  bool ok = false;       // when executed: true with `result`, false with the fault in `error`
  uint64_t result = 0;
  std::string error;
  std::string reason;
};

// Whether `module` is in the subset above; `reason` names the first obstacle.
bool nativeJitAccepts(const IrModule &module, std::string &reason);

// Compiles and runs `module` with `args` (argv[0] first, as the VM receives them). Program output
// goes straight to file descriptors 1 and 2.
NativeJitResult runNativeJit(const IrModule &module, const std::vector<std::string_view> &args);

} // namespace primec

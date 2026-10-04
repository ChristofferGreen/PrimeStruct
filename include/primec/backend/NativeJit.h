#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// In-process native execution for the VM (Linux x86_64): the module is compiled by the native
// backend and run as machine code instead of being interpreted, when that is observably the
// same. Only modules whose every opcode has the VM's exact semantics natively are taken
// (integer and f64 arithmetic, comparisons, branches, calls, returns, prints of numbers and
// module strings, argc, module string bytes, frame addresses with the VM's address values); the
// VM's runtime faults (division by zero, string and indirect-address errors, more than 4096
// frames, a missing return) are checked in the code and reported with the VM's messages. The
// heap, files, host calls, argv strings, f32 values and float-to-i32/u64 conversions stay on the
// interpreter.
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

#pragma once

#include <string>
#include <unordered_map>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

// An entry that returns a `Result` ends the process with an exit code, not with the value's address or its packed
// bits. `ok` exits with its payload when that is an integer (the operating system keeps the low 8 bits) and with 0
// otherwise. `error` exits with its error code when that is an integer in 1..255, and with 1 otherwise (a struct,
// float or string error, an integer of 0 that must not look like `ok`, or an integer outside that range).
// The declared return type tells the two representations apart: a stdlib `Result` sum (`/std/result/Result__...`,
// every return hands over the sum's address) or the builtin packed `Result` (the error code in the high 32 bits, the
// ok payload in the low 32 bits). Rewrites every return of `entryFunction` into a jump to an appended block that
// decodes the value the entry would have returned. Does nothing when the entry does not return a `Result`.
void applyEntryResultExitCode(const Definition &entryDef,
                              const std::unordered_map<std::string, const Definition *> &defMap,
                              IrFunction &entryFunction);

// The same exit codes for a target that takes structured control flow only (Wasm), which cannot jump to an appended
// block: each return of the entry is replaced in place by a decode that parks the value in a fresh local and ends
// every branch in its own return, and the jumps of the entry are renumbered. Handles the builtin packed `Result`;
// a stdlib `Result` sum is read through its address, which such a target does not support, so it is left alone.
void applyEntryResultExitCodeStructured(
    const Definition &entryDef,
    const std::unordered_map<std::string, const Definition *> &defMap,
    IrFunction &entryFunction);

} // namespace primec::ir_lowerer

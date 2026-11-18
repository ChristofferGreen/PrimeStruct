#pragma once

#include <string>
#include <unordered_map>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

// An entry that returns a stdlib `Result` sum (`/std/result/Result`) ends the process with an exit code, not with
// the sum's address: 0 for `ok`; for `error`, the error payload when it is an integer in 1..255, and 1 otherwise
// (a struct, float or string payload, or an integer outside that range). Rewrites every `ReturnI64` of
// `entryFunction` into a jump to one appended block that decodes the sum the entry would have returned. Does
// nothing when the entry's return type is not a stdlib Result sum.
void applyEntryResultExitCode(const Definition &entryDef,
                              const std::unordered_map<std::string, const Definition *> &defMap,
                              IrFunction &entryFunction);

} // namespace primec::ir_lowerer

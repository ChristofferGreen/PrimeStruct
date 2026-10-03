#pragma once

#include <cstddef>
#include <string>

#include "primec/ir/Ir.h"

namespace primec {

// Deterministic text listing of a lowered IR module: header, string table, host
// imports, struct layouts, and every function with one instruction per line
// (index, opcode name, decoded immediate). Used by `--dump-stage=ir-lowered`
// and `--dump-stage=ir-optimized`, and by golden tests of optimization passes.
// The listing carries no timings or addresses, so equal modules print equal
// text. Not a serialization format: use IrSerializer for that.
std::string formatIrModule(const IrModule &module);

// Listing of one function (the same lines formatIrModule prints for it).
std::string formatIrFunction(const IrModule &module, size_t functionIndex);

// One instruction without its index, for example `AddI32` or `Jump -> 12`.
std::string formatIrInstruction(const IrModule &module, const IrInstruction &instruction);

} // namespace primec

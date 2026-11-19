#pragma once

#include "primec/ir/Ir.h"

namespace primec {

// True when a function of `module` has a FmaF32 or FmaF64.
bool irModuleHasFusedMultiplyAdd(const IrModule &module);

// Replaces each FmaF32 and FmaF64 by the rounding-twice sequence `StoreLocal t; Mul; LoadLocal t; Add` (t is a local of
// its own, one past the highest local the function uses), for a backend that has no fused instruction. The stack
// holds a, b, c before the opcode and a * b + c after the sequence; jump targets are renumbered. The result differs
// from the fused opcode where the product does not fit the type (docs/spec/vm-design.md, Fused multiply-add).
void expandFusedMultiplyAdd(IrModule &module);

} // namespace primec

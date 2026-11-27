#pragma once

#include "primec/ir/Ir.h"

namespace primec {

// True when a function of `module` has a FunctionAddress, CallIndirect or CallIndirectVoid.
bool irModuleHasIndirectCalls(const IrModule &module);

// Rewrites the function address opcodes for a target that does not run them (docs/spec/vm-design.md, Function
// addresses). `FunctionAddress f` becomes `PushI64 f + 1`. A `CallIndirect p` stores its address and p parameters to
// locals of its own (one past the highest local the function uses), then compares the address with the address of
// each function whose address the module takes and that has p parameters (and, for `CallIndirect`, returns a value),
// and calls the one that matches with a direct `Call` (`CallVoid` for the void form) whose result goes to a local of
// its own that the arms' join loads, so every arm leaves the operand stack as it found it; an address that matches
// none, a null one included, ends the program with exit code 3 as a runtime error does. Jump targets are renumbered.
// Run it on a linked module: the functions of a library whose address the program takes are known only after the
// link.
void expandIndirectCalls(IrModule &module);

} // namespace primec

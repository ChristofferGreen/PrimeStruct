#pragma once

#include "primec/ir/Ir.h"

#include <string>

namespace primec {

// Emits structured C++ for an IR module, for the `optcpp` and `optexe` emit
// kinds (docs/OptimizingBackendsPlan.md, section 9). Unlike IrToCppEmitter,
// which re-implements the stack machine with a `switch (pc)` dispatch loop and
// a heap-allocated operand stack, every function becomes ordinary C++:
//   * each operand-stack depth is a plain variable (s0, s1, ...), so a stack
//     operation is an assignment between variables;
//   * each local is a variable (l0, l1, ...);
//   * each basic block is a label and jumps are `goto`;
//   * calls are C++ calls whose parameters are the callee's initial stack.
// Block depths are proven consistent by the shared CFG (IrCfg.h), so no moves
// are needed at joins, and the host C++ compiler performs register allocation
// and instruction selection. Arithmetic follows the shared pure-opcode
// semantics (IrPureSemantics.h), so results match the VM bit for bit.
//
// Opcodes this emitter does not support yet fail with a diagnostic naming the
// opcode and function; there is no per-function fallback to another emitter.
class IrToOptCppEmitter {
public:
  bool emitSource(const IrModule &module, std::string &out, std::string &error) const;
};

} // namespace primec

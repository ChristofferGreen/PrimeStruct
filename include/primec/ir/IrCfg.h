#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// Operand-stack effect of one IR instruction. `readsWithoutPop` counts operands
// that are inspected but left in place (Dup); they must be present on entry.
struct IrStackEffect {
  uint32_t pops = 0;
  uint32_t pushes = 0;
  uint32_t readsWithoutPop = 0;
};

// Single table of stack effects for every IrOpcode. Call/CallVoid read the
// callee's parameterCount and CallHost reads the host import's signature from
// `module`. A call target or host import index that is out of range yields an
// effect of zero here; IR validation is what rejects such modules.
// Returns false only for an opcode value outside the table.
bool computeIrStackEffect(const IrInstruction &instruction, const IrModule &module, IrStackEffect &out);

enum class IrCfgErrorKind : uint8_t {
  None = 0,
  InvalidJumpTarget,
  UnsupportedOpcode,
  StackUnderflow,
  InvalidDup,
  InconsistentDepth,
};

struct IrCfgError {
  IrCfgErrorKind kind = IrCfgErrorKind::None;
  // Instruction the problem is reported at. For InconsistentDepth this is the
  // first instruction of the block whose entry depth disagrees.
  size_t instructionIndex = 0;
  IrOpcode opcode = IrOpcode::PushI32;
};

struct IrCfgBlock {
  size_t start = 0; // first instruction index
  size_t end = 0;   // one past the last instruction
  // Successor block indices: the jump target first, then the fallthrough.
  // A jump whose target is the function end has no successor for that edge.
  std::vector<size_t> successors;
  std::vector<size_t> predecessors;
  bool reachable = false;
  // Operand-stack depth on entry and exit, and the highest depth reached while
  // executing the block. Only meaningful when `reachable`.
  int64_t entryDepth = 0;
  int64_t exitDepth = 0;
  int64_t maxDepth = 0;
};

struct IrCfg {
  std::vector<IrCfgBlock> blocks;
  // Highest operand-stack depth over all reachable blocks.
  int64_t maxStackDepth = 0;
};

// Splits `function` into basic blocks (leaders are instruction 0, jump targets,
// and the instruction after every jump or return), links them, and propagates
// operand-stack depth from the entry block, which starts at
// `function.parameterCount` because a called function receives its arguments
// on the shared stack. Fails on an out-of-range jump target, on stack
// underflow, on a Dup without enough operands, and when two paths reach a
// block with different depths. An empty function yields no blocks.
bool buildIrCfg(const IrFunction &function, const IrModule &module, IrCfg &out, IrCfgError &error);

// Index of the block containing `instructionIndex` (the last block if it is
// past the end).
size_t irCfgBlockIndexForInstruction(const IrCfg &cfg, size_t instructionIndex);

} // namespace primec

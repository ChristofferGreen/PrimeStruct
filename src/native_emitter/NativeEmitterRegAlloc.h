#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::native_emitter {

// Register allocation for the x86_64 native emitter (docs/OptimizingBackendsPlan.md, Phase 3).
//
// A function is lowered to the block register form with promoted locals
// (IrVirtualRegisterLowering.h). Copies are folded away before allocation: a constant push
// becomes an immediate, and LoadLocal/StoreLocal of a promoted local, Dup and Pop name an
// existing value instead of producing a new one. Every value is then defined and used inside
// one block; values cross blocks only through the edge moves, so liveness is a single range
// per value and a linear scan over the function assigns registers, with each edge move hinting
// its two ends towards the same register. Values that do not fit get a spill slot.

enum class RegAllocLocationKind : uint8_t {
  None, // never read (a compare fused into its branch)
  Imm,  // a constant, used as an immediate
  Reg,  // a machine register
  Slot, // a spill slot
};

struct RegAllocLocation {
  RegAllocLocationKind kind = RegAllocLocationKind::None;
  uint8_t reg = 0;
  uint32_t slot = 0;
  uint64_t imm = 0;
};

struct RegAllocInstruction {
  size_t irIndex = 0;
  // Values read and written, after copy folding.
  std::vector<uint32_t> uses;
  std::vector<uint32_t> defs;
  // Emits nothing: a constant push, a promoted local's load or store, Dup or Pop.
  bool folded = false;
  // An integer comparison whose only reader is the JumpIfZero right after it.
  bool fusedIntoBranch = false;
};

struct RegAllocMove {
  uint32_t source = 0;
  uint32_t destination = 0;
};

struct RegAllocEdge {
  size_t successorBlock = 0;
  std::vector<RegAllocMove> moves;
};

struct RegAllocBlock {
  size_t start = 0;
  size_t end = 0;
  bool reachable = false;
  // Block 0: the parameters on the operand stack at entry, deepest first.
  std::vector<uint32_t> entryValues;
  // Block 0: promoted locals read before any store, which start at zero.
  std::vector<uint32_t> zeroValues;
  std::vector<RegAllocInstruction> instructions;
  std::vector<RegAllocEdge> edges;
  // Every value written in this block (its entry values included), the only ones that can be
  // live inside it.
  std::vector<uint32_t> values;
};

struct RegAllocFunctionPlan {
  std::vector<RegAllocBlock> blocks;
  std::vector<RegAllocLocation> locations;
  // Live range of each value in instruction positions (see regAllocUsePosition).
  std::vector<uint32_t> rangeStart;
  std::vector<uint32_t> rangeEnd;
  uint32_t spillSlotCount = 0;
};

// Positions: instruction i reads its operands at 4i+1 and writes its results at 4i+2; a block's
// entry values are written at 4*start and its edge moves read at 4*end-1.
inline uint32_t regAllocUsePosition(size_t index) {
  return static_cast<uint32_t>(index * 4 + 1);
}
inline uint32_t regAllocDefPosition(size_t index) {
  return static_cast<uint32_t>(index * 4 + 2);
}

// Plans `functionIndex` of `module` over the registers in `pool`. Returns false (with a reason in
// `error`) when the function's register form cannot be built, in which case the caller emits the
// function the template way.
bool planNativeRegisterAllocation(const IrModule &module,
                                  size_t functionIndex,
                                  const std::vector<uint8_t> &pool,
                                  RegAllocFunctionPlan &out,
                                  std::string &error);

// The machine registers holding a value that must survive instruction `index` of block
// `blockIndex` (live before and after it, and not written by it).
std::vector<uint8_t>
regAllocRegistersLiveAcross(const RegAllocFunctionPlan &plan, size_t blockIndex, size_t index);

} // namespace primec::native_emitter

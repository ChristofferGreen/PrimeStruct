#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::native_emitter {

// Register allocation for the native emitters (docs/OptimizingBackendsPlan.md, Phase 3 and
// section 12): x86_64 (NativeEmitterInternalsX64RegAlloc.h) and arm64
// (NativeEmitterInternalsArm64RegAlloc.h). The plan is target-independent; the pool names the
// target's registers.
//
// A function is lowered to the block register form with promoted locals
// (IrVirtualRegisterLowering.h). Copies are folded away before allocation: a constant push
// becomes an immediate, and LoadLocal/StoreLocal of a promoted local, Dup and Pop name an
// existing value instead of producing a new one. Every value is then defined and used inside
// one block and crosses blocks only through the edge moves, so liveness is a single range per
// value. Values joined by an edge move, or by a two-address operation whose operand dies there,
// form classes when their ranges do not meet; classes are coloured greedily by estimated
// executions per position held, and a class that finds no register everywhere gets one slot.
// A class that only float operations read or write is coloured from the target's float pool
// instead (x86_64: xmm2-xmm15; arm64: d8-d15, d16-d31, d2-d7).

enum class RegAllocLocationKind : uint8_t {
  None, // never read (a compare fused into its branch)
  Imm,  // a constant, used as an immediate
  Reg,  // a machine register
  Slot, // a spill slot
  Xmm,  // a float register (x86_64 xmm, arm64 v/d), holding the value's bit pattern in its
        // low 64 bits
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
  // A comparison whose only reader is the JumpIfZero right after it (integer, and float where
  // the target fuses them: RegAllocFloatRegisters::fuseCompares).
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

// The float registers of a target: classes only float operations touch are coloured from
// `pool` (none when it is empty), in its order.
struct RegAllocFloatRegisters {
  std::vector<uint8_t> pool;
  // A float comparison whose only reader is the JumpIfZero after it is fused into the branch.
  bool fuseCompares = false;
};
// xmm0 and xmm1 are the float scratch registers of the x86_64 inline code and templates.
inline const RegAllocFloatRegisters X64RegAllocFloatRegisters = {
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, false};

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
// function the template way. With `promoteLocals` false every local stays in its frame slot.
// `floatRegisters` names the float pool (numbered below 32, as the general registers).
bool planNativeRegisterAllocation(
    const IrModule &module,
    size_t functionIndex,
    const std::vector<uint8_t> &pool,
    RegAllocFunctionPlan &out,
    std::string &error,
    bool promoteLocals = true,
    const RegAllocFloatRegisters &floatRegisters = X64RegAllocFloatRegisters);

// The machine registers holding a value that must survive instruction `index` of block
// `blockIndex` (live before and after it, and not written by it): general registers by number,
// float registers as `floatBase` + their number (x86_64: 16, arm64: 32).
std::vector<uint8_t> regAllocRegistersLiveAcross(const RegAllocFunctionPlan &plan,
                                                 size_t blockIndex,
                                                 size_t index,
                                                 uint8_t floatBase = 16);

} // namespace primec::native_emitter

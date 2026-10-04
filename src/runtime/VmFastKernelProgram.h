#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::vm_detail {

// The pre-decoded form the fast VM loop (VmFastKernel.cpp) runs: IR instructions with their stack
// effects, and fused instructions that replace short sequences (VmFastKernelPrepare.cpp).

// The six integer comparisons the fused compare-and-branch forms cover.
#define FAST_CMPS(X) X(Eq, ==) X(Ne, !=) X(Lt, <) X(Le, <=) X(Gt, >) X(Ge, >=)

#define FAST_ENUM_JMP_CMP_LOCAL_IMM(N, O) FastOpJmpCmpLocalImm##N,
#define FAST_ENUM_JMP_CMP_LOCAL_LOCAL(N, O) FastOpJmpCmpLocalLocal##N,
#define FAST_ENUM_JMP_CMP(N, O) FastOpJmpCmp##N,

// Internal opcodes live above the IrOpcode range. Most are fused sequences of
// IR instructions that stay inside one basic block (see fuseInstructions); a
// fused instruction sits in the slot of the first original instruction and
// advances `ip` past the slots it replaced, which stay in the array untouched
// so every jump target keeps its index.
enum FastOp : uint16_t {
  FastOpMissingReturn = 0x100,
  FastOpStoreLocalDupPop,                  // Dup; StoreLocal a; Pop
  FastOpStoreLocalImm,                     // Push c; StoreLocal a
  FastOpCopyLocal,                         // LoadLocal a; StoreLocal b
  FastOpJmpLocalZero,                      // LoadLocal a; JumpIfZero b
  FastOpPushLocalAddImm,                   // LoadLocal a; Push c; Add
  FastOpPushLocalSubImm,                   // LoadLocal a; Push c; Sub
  FastOpPushLocalMulImm,                   // LoadLocal a; Push c; Mul
  FastOpPushLocalAddLocal,                 // LoadLocal a; LoadLocal b; Add
  FastOpPushLocalSubLocal,                 // LoadLocal a; LoadLocal b; Sub
  FastOpPushLocalMulLocal,                 // LoadLocal a; LoadLocal b; Mul
  FastOpLocalAddImmStore,                  // LoadLocal a; Push c; Add; StoreLocal b
  FastOpLocalSubImmStore,                  // LoadLocal a; Push c; Sub; StoreLocal b
  FastOpPushLocalAddImmSext,               // LoadLocal a; Push c; Add; SextI32
  FastOpPushLocalSubImmSext,               // LoadLocal a; Push c; Sub; SextI32
  FastOpPushLocalMulImmSext,               // LoadLocal a; Push c; Mul; SextI32
  FastOpPushLocalAddLocalSext,             // LoadLocal a; LoadLocal b; Add; SextI32
  FastOpPushLocalSubLocalSext,             // LoadLocal a; LoadLocal b; Sub; SextI32
  FastOpPushLocalMulLocalSext,             // LoadLocal a; LoadLocal b; Mul; SextI32
  FastOpLocalAddImmStoreSext,              // LoadLocal a; Push c; Add; SextI32; StoreLocal b
  FastOpLocalSubImmStoreSext,              // LoadLocal a; Push c; Sub; SextI32; StoreLocal b
  FastOpLocalStringByteStore,              // LoadLocal a; LoadStringByte #imm; StoreLocal b
  FastOpPushLocalStringByte,               // LoadLocal a; LoadStringByte #imm
  FastOpPushLocalAddLocalF64,              // LoadLocal a; LoadLocal b; AddF64
  FastOpPushLocalSubLocalF64,              // LoadLocal a; LoadLocal b; SubF64
  FastOpPushLocalMulLocalF64,              // LoadLocal a; LoadLocal b; MulF64
  FastOpPushLocalDivLocalF64,              // LoadLocal a; LoadLocal b; DivF64
  FastOpPushLocalAddImmF64,                // LoadLocal a; PushF64 c; AddF64
  FastOpPushLocalSubImmF64,                // LoadLocal a; PushF64 c; SubF64
  FastOpPushLocalMulImmF64,                // LoadLocal a; PushF64 c; MulF64
  FastOpPushLocalDivImmF64,                // LoadLocal a; PushF64 c; DivF64
  FastOpAddSext,                           // AddI32; SextI32
  FastOpSubSext,                           // SubI32; SextI32
  FastOpMulSext,                           // MulI32; SextI32
  FAST_CMPS(FAST_ENUM_JMP_CMP_LOCAL_IMM)   // LoadLocal a; Push c; Cmp; JumpIfZero b
  FAST_CMPS(FAST_ENUM_JMP_CMP_LOCAL_LOCAL) // LoadLocal a; LoadLocal b; Cmp; JumpIfZero imm
  FAST_CMPS(FAST_ENUM_JMP_CMP)             // Cmp; JumpIfZero b
  FastOpEnd,
};

#undef FAST_ENUM_JMP_CMP_LOCAL_IMM
#undef FAST_ENUM_JMP_CMP_LOCAL_LOCAL
#undef FAST_ENUM_JMP_CMP

struct FastInst {
  uint16_t op = 0;
  // Operands the instruction pops and results it pushes, from the shared stack
  // effect table. Used by the opcodes that go through the host handlers.
  uint16_t pops = 0;
  uint16_t pushes = 0;
  // Local indices and jump targets of the fused forms.
  uint32_t a = 0;
  uint32_t b = 0;
  uint64_t imm = 0;
  const IrInstruction *source = nullptr;
};

struct FastFunction {
  const IrFunction *function = nullptr;
  // The function's instructions followed by a sentinel that faults with the
  // step kernel's "missing return" message, so falling off the end (or jumping
  // to it) needs no bounds check in the loop.
  std::vector<FastInst> code;
  size_t localCount = 0;
  // Operand-stack slots the function needs above its arguments.
  size_t stackHeadroom = 0;
};

// The slot value SextI32 produces: the low 32 bits sign-extended.
inline uint64_t sext32(uint64_t value) {
  return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));
}

// Prepares every function; false when any of them (or the entry) is not eligible for the fast
// loop.
bool prepareModule(const IrModule &module, std::vector<FastFunction> &functions);

} // namespace primec::vm_detail

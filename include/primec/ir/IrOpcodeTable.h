#pragma once

#include "primec/ir/Ir.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace primec {

// What an opcode does, at the granularity the passes need.
enum class IrOpCategory : uint8_t {
  Unclassified,
  ConstInt,   // PushI32, PushI64
  ConstFloat, // PushF32, PushF64
  Local,      // load, store or take the address of a local
  Stack,      // Dup, Pop
  IntArith,   // integer add, sub, mul, div, neg
  FloatArith, // float add, sub, mul, div, neg
  Compare,
  Convert,
  SignExtend,
  ZeroExtend,
  Jump,   // Jump, JumpIfZero
  Return, // returns from the function
  Trap,   // ends the program
  Call,
  Io,     // print and file operations
  Memory, // indirect and byte loads and stores, string byte and length reads
  Heap,
  Sync, // tasks, atomics, mutexes, condition variables
  Other,
};

// The arithmetic operation of an IntArith or FloatArith opcode.
enum class IrOpArith : uint8_t { None, Add, Sub, Mul, Div, Neg };

// The kind of the value an opcode pushes, where the name fixes it; None when it pushes nothing
// or an untyped slot.
enum class IrOpResult : uint8_t { None, Bool, I32, I64, U64, F32, F64 };

// Single table of IR opcodes. One row per IrOpcode enumerator, in
// enum order: X(Name, category, arith, result, glsl, wasm, wasmBrowser). The category,
// arithmetic operation and result kind classify the opcode for every pass that needs to
// recognise a group of opcodes (a test fails for an opcode left Unclassified); the flags say
// whether the validator accepts the opcode for that target (`wasm` is the WASI profile;
// `wasmBrowser` excludes the WASI-only I/O opcodes). The enum in Ir.h keeps
// the opcode numbering; scripts/check_ir_opcode_table.py (ctest) fails when an
// enumerator has no row or the order differs, and the static_asserts below tie
// the table to the enum's first and last values.
#define PRIMEC_IR_OPCODE_TABLE(X)                                                                  \
  X(PushI32, ConstInt, None, I32, 1, 1, 1)                                                         \
  X(PushI64, ConstInt, None, I64, 1, 1, 1)                                                         \
  X(LoadLocal, Local, None, None, 1, 1, 1)                                                         \
  X(StoreLocal, Local, None, None, 1, 1, 1)                                                        \
  X(AddressOfLocal, Local, None, None, 1, 0, 0)                                                    \
  X(LoadIndirect, Memory, None, None, 1, 0, 0)                                                     \
  X(StoreIndirect, Memory, None, None, 1, 0, 0)                                                    \
  X(Dup, Stack, None, None, 1, 1, 1)                                                               \
  X(Pop, Stack, None, None, 1, 1, 1)                                                               \
  X(AddI32, IntArith, Add, I32, 1, 1, 1)                                                           \
  X(SubI32, IntArith, Sub, I32, 1, 1, 1)                                                           \
  X(MulI32, IntArith, Mul, I32, 1, 1, 1)                                                           \
  X(DivI32, IntArith, Div, I32, 1, 1, 1)                                                           \
  X(NegI32, IntArith, Neg, I32, 1, 1, 1)                                                           \
  X(AddI64, IntArith, Add, I64, 1, 1, 1)                                                           \
  X(SubI64, IntArith, Sub, I64, 1, 1, 1)                                                           \
  X(MulI64, IntArith, Mul, I64, 1, 1, 1)                                                           \
  X(DivI64, IntArith, Div, I64, 1, 1, 1)                                                           \
  X(DivU64, IntArith, Div, U64, 1, 1, 1)                                                           \
  X(NegI64, IntArith, Neg, I64, 1, 1, 1)                                                           \
  X(CmpEqI32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpNeI32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLtI32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLeI32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGtI32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGeI32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpEqI64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpNeI64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLtI64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLeI64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGtI64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGeI64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLtU64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLeU64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGtU64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGeU64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(JumpIfZero, Jump, None, None, 1, 1, 1)                                                         \
  X(Jump, Jump, None, None, 1, 1, 1)                                                               \
  X(ReturnVoid, Return, None, None, 1, 1, 1)                                                       \
  X(ReturnI32, Return, None, None, 1, 1, 1)                                                        \
  X(ReturnI64, Return, None, None, 1, 1, 1)                                                        \
  X(PrintI32, Io, None, None, 1, 1, 0)                                                             \
  X(PrintI64, Io, None, None, 1, 1, 0)                                                             \
  X(PrintU64, Io, None, None, 1, 1, 0)                                                             \
  X(PrintString, Io, None, None, 1, 1, 0)                                                          \
  X(PushArgc, Other, None, None, 1, 1, 0)                                                          \
  X(PrintArgv, Io, None, None, 1, 1, 0)                                                            \
  X(PrintArgvUnsafe, Io, None, None, 1, 1, 0)                                                      \
  X(LoadStringByte, Memory, None, None, 1, 0, 0)                                                   \
  X(LoadStringLength, Memory, None, None, 1, 0, 0)                                                 \
  X(FileOpenRead, Io, None, None, 1, 1, 0)                                                         \
  X(FileOpenWrite, Io, None, None, 1, 1, 0)                                                        \
  X(FileOpenAppend, Io, None, None, 1, 1, 0)                                                       \
  X(FileOpenReadDynamic, Io, None, None, 1, 1, 0)                                                  \
  X(FileOpenWriteDynamic, Io, None, None, 1, 1, 0)                                                 \
  X(FileOpenAppendDynamic, Io, None, None, 1, 1, 0)                                                \
  X(FileReadByte, Io, None, None, 0, 1, 0)                                                         \
  X(FileClose, Io, None, None, 1, 1, 0)                                                            \
  X(FileFlush, Io, None, None, 1, 1, 0)                                                            \
  X(FileWriteI32, Io, None, None, 1, 1, 0)                                                         \
  X(FileWriteI64, Io, None, None, 1, 1, 0)                                                         \
  X(FileWriteU64, Io, None, None, 1, 1, 0)                                                         \
  X(FileWriteString, Io, None, None, 1, 1, 0)                                                      \
  X(FileWriteByte, Io, None, None, 1, 1, 0)                                                        \
  X(FileWriteNewline, Io, None, None, 1, 1, 0)                                                     \
  X(PushF32, ConstFloat, None, F32, 1, 1, 1)                                                       \
  X(PushF64, ConstFloat, None, F64, 1, 1, 1)                                                       \
  X(AddF32, FloatArith, Add, F32, 1, 1, 1)                                                         \
  X(SubF32, FloatArith, Sub, F32, 1, 1, 1)                                                         \
  X(MulF32, FloatArith, Mul, F32, 1, 1, 1)                                                         \
  X(DivF32, FloatArith, Div, F32, 1, 1, 1)                                                         \
  X(NegF32, FloatArith, Neg, F32, 1, 1, 1)                                                         \
  X(AddF64, FloatArith, Add, F64, 1, 1, 1)                                                         \
  X(SubF64, FloatArith, Sub, F64, 1, 1, 1)                                                         \
  X(MulF64, FloatArith, Mul, F64, 1, 1, 1)                                                         \
  X(DivF64, FloatArith, Div, F64, 1, 1, 1)                                                         \
  X(NegF64, FloatArith, Neg, F64, 1, 1, 1)                                                         \
  X(CmpEqF32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpNeF32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLtF32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLeF32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGtF32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGeF32, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpEqF64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpNeF64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLtF64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpLeF64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGtF64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(CmpGeF64, Compare, None, Bool, 1, 1, 1)                                                        \
  X(ConvertI32ToF32, Convert, None, F32, 1, 1, 1)                                                  \
  X(ConvertI32ToF64, Convert, None, F64, 1, 1, 1)                                                  \
  X(ConvertI64ToF32, Convert, None, F32, 1, 1, 1)                                                  \
  X(ConvertI64ToF64, Convert, None, F64, 1, 1, 1)                                                  \
  X(ConvertU64ToF32, Convert, None, F32, 1, 1, 1)                                                  \
  X(ConvertU64ToF64, Convert, None, F64, 1, 1, 1)                                                  \
  X(ConvertF32ToI32, Convert, None, I32, 1, 1, 1)                                                  \
  X(ConvertF32ToI64, Convert, None, I64, 1, 1, 1)                                                  \
  X(ConvertF32ToU64, Convert, None, U64, 1, 1, 1)                                                  \
  X(ConvertF64ToI32, Convert, None, I32, 1, 1, 1)                                                  \
  X(ConvertF64ToI64, Convert, None, I64, 1, 1, 1)                                                  \
  X(ConvertF64ToU64, Convert, None, U64, 1, 1, 1)                                                  \
  X(ConvertF32ToF64, Convert, None, F64, 1, 1, 1)                                                  \
  X(ConvertF64ToF32, Convert, None, F32, 1, 1, 1)                                                  \
  X(ReturnF32, Return, None, None, 1, 1, 1)                                                        \
  X(ReturnF64, Return, None, None, 1, 1, 1)                                                        \
  X(PrintStringDynamic, Io, None, None, 1, 1, 0)                                                   \
  X(Call, Call, None, None, 1, 1, 1)                                                               \
  X(CallVoid, Call, None, None, 1, 1, 1)                                                           \
  X(HeapAlloc, Heap, None, None, 0, 0, 0)                                                          \
  X(HeapFree, Heap, None, None, 0, 0, 0)                                                           \
  X(HeapRealloc, Heap, None, None, 0, 0, 0)                                                        \
  X(FileWriteStringDynamic, Io, None, None, 1, 1, 0)                                               \
  X(CallHost, Call, None, None, 0, 0, 0)                                                           \
  X(LoadStringByteDynamic, Memory, None, None, 0, 0, 0)                                            \
  X(SextI32, SignExtend, None, None, 1, 1, 1)                                                      \
  X(HeapAllocBytes, Heap, None, None, 0, 0, 0)                                                     \
  X(LoadU8, Memory, None, None, 0, 0, 0)                                                           \
  X(StoreU8, Memory, None, None, 0, 0, 0)                                                          \
  X(ZextU8, ZeroExtend, None, None, 0, 0, 0)                                                       \
  X(Trap, Trap, None, None, 0, 0, 0)                                                               \
  X(TaskSpawn, Sync, None, None, 0, 0, 0)                                                          \
  X(TaskWait, Sync, None, None, 0, 0, 0)                                                           \
  X(AtomicLoad, Sync, None, None, 0, 0, 0)                                                         \
  X(AtomicStore, Sync, None, None, 0, 0, 0)                                                        \
  X(AtomicAdd, Sync, None, None, 0, 0, 0)                                                          \
  X(AtomicExchange, Sync, None, None, 0, 0, 0)                                                     \
  X(AtomicCompareExchange, Sync, None, None, 0, 0, 0)                                              \
  X(MutexLock, Sync, None, None, 0, 0, 0)                                                          \
  X(MutexUnlock, Sync, None, None, 0, 0, 0)                                                        \
  X(ConditionWait, Sync, None, None, 0, 0, 0)                                                      \
  X(ConditionNotify, Sync, None, None, 0, 0, 0)

struct IrOpcodeInfo {
  IrOpcode op;
  const char *name;
  IrOpCategory category;
  IrOpArith arith;
  IrOpResult result;
  bool glsl;
  bool wasm;
  bool wasmBrowser;
};

inline constexpr std::array IrOpcodeTable = {
#define PRIMEC_IR_OPCODE_ROW(name, category, arith, result, glsl, wasm, wasmBrowser)               \
  IrOpcodeInfo{IrOpcode::name,                                                                     \
               #name,                                                                              \
               IrOpCategory::category,                                                             \
               IrOpArith::arith,                                                                   \
               IrOpResult::result,                                                                 \
               glsl != 0,                                                                          \
               wasm != 0,                                                                          \
               wasmBrowser != 0},
    PRIMEC_IR_OPCODE_TABLE(PRIMEC_IR_OPCODE_ROW)
#undef PRIMEC_IR_OPCODE_ROW
};

inline constexpr uint8_t IrOpcodeMin = static_cast<uint8_t>(IrOpcodeTable.front().op);
inline constexpr uint8_t IrOpcodeMax = static_cast<uint8_t>(IrOpcodeTable.back().op);

static_assert(IrOpcodeMin == 1, "opcode numbering starts at 1");
static_assert(static_cast<size_t>(IrOpcodeMax - IrOpcodeMin) + 1 == IrOpcodeTable.size(),
              "the opcode table must have exactly one row per opcode value");

constexpr bool irOpcodeTableOrderMatchesEnum() {
  for (size_t i = 0; i < IrOpcodeTable.size(); ++i) {
    if (static_cast<size_t>(IrOpcodeTable[i].op) != i + IrOpcodeMin) {
      return false;
    }
  }
  return true;
}
static_assert(irOpcodeTableOrderMatchesEnum(), "opcode table rows must follow the enum order");

// nullptr for a value outside [IrOpcodeMin, IrOpcodeMax].
constexpr const IrOpcodeInfo *irOpcodeInfo(IrOpcode op) {
  const auto value = static_cast<uint8_t>(op);
  if (value < IrOpcodeMin || value > IrOpcodeMax) {
    return nullptr;
  }
  return &IrOpcodeTable[static_cast<size_t>(value - IrOpcodeMin)];
}

inline constexpr IrOpCategory irOpcodeCategory(IrOpcode op) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  return info == nullptr ? IrOpCategory::Unclassified : info->category;
}
inline constexpr IrOpArith irOpcodeArith(IrOpcode op) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  return info == nullptr ? IrOpArith::None : info->arith;
}
inline constexpr IrOpResult irOpcodeResult(IrOpcode op) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  return info == nullptr ? IrOpResult::None : info->result;
}

} // namespace primec

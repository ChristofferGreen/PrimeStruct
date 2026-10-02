#pragma once

#include "primec/ir/Ir.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace primec {

// Single table of IR opcodes (TODO-5361). One row per IrOpcode enumerator, in
// enum order: X(Name, glsl, wasm, wasmBrowser) where the flags say whether the
// validator accepts the opcode for that target (`wasm` is the WASI profile;
// `wasmBrowser` excludes the WASI-only I/O opcodes). The enum in Ir.h keeps
// the opcode numbering; scripts/check_ir_opcode_table.py (ctest) fails when an
// enumerator has no row or the order differs, and the static_asserts below tie
// the table to the enum's first and last values.
#define PRIMEC_IR_OPCODE_TABLE(X) \
  X(PushI32, 1, 1, 1) \
  X(PushI64, 1, 1, 1) \
  X(LoadLocal, 1, 1, 1) \
  X(StoreLocal, 1, 1, 1) \
  X(AddressOfLocal, 1, 0, 0) \
  X(LoadIndirect, 1, 0, 0) \
  X(StoreIndirect, 1, 0, 0) \
  X(Dup, 1, 1, 1) \
  X(Pop, 1, 1, 1) \
  X(AddI32, 1, 1, 1) \
  X(SubI32, 1, 1, 1) \
  X(MulI32, 1, 1, 1) \
  X(DivI32, 1, 1, 1) \
  X(NegI32, 1, 1, 1) \
  X(AddI64, 1, 1, 1) \
  X(SubI64, 1, 1, 1) \
  X(MulI64, 1, 1, 1) \
  X(DivI64, 1, 1, 1) \
  X(DivU64, 1, 1, 1) \
  X(NegI64, 1, 1, 1) \
  X(CmpEqI32, 1, 1, 1) \
  X(CmpNeI32, 1, 1, 1) \
  X(CmpLtI32, 1, 1, 1) \
  X(CmpLeI32, 1, 1, 1) \
  X(CmpGtI32, 1, 1, 1) \
  X(CmpGeI32, 1, 1, 1) \
  X(CmpEqI64, 1, 1, 1) \
  X(CmpNeI64, 1, 1, 1) \
  X(CmpLtI64, 1, 1, 1) \
  X(CmpLeI64, 1, 1, 1) \
  X(CmpGtI64, 1, 1, 1) \
  X(CmpGeI64, 1, 1, 1) \
  X(CmpLtU64, 1, 1, 1) \
  X(CmpLeU64, 1, 1, 1) \
  X(CmpGtU64, 1, 1, 1) \
  X(CmpGeU64, 1, 1, 1) \
  X(JumpIfZero, 1, 1, 1) \
  X(Jump, 1, 1, 1) \
  X(ReturnVoid, 1, 1, 1) \
  X(ReturnI32, 1, 1, 1) \
  X(ReturnI64, 1, 1, 1) \
  X(PrintI32, 1, 1, 0) \
  X(PrintI64, 1, 1, 0) \
  X(PrintU64, 1, 1, 0) \
  X(PrintString, 1, 1, 0) \
  X(PushArgc, 1, 1, 0) \
  X(PrintArgv, 1, 1, 0) \
  X(PrintArgvUnsafe, 1, 1, 0) \
  X(LoadStringByte, 1, 0, 0) \
  X(LoadStringLength, 1, 0, 0) \
  X(FileOpenRead, 1, 1, 0) \
  X(FileOpenWrite, 1, 1, 0) \
  X(FileOpenAppend, 1, 1, 0) \
  X(FileOpenReadDynamic, 1, 1, 0) \
  X(FileOpenWriteDynamic, 1, 1, 0) \
  X(FileOpenAppendDynamic, 1, 1, 0) \
  X(FileReadByte, 0, 1, 0) \
  X(FileClose, 1, 1, 0) \
  X(FileFlush, 1, 1, 0) \
  X(FileWriteI32, 1, 1, 0) \
  X(FileWriteI64, 1, 1, 0) \
  X(FileWriteU64, 1, 1, 0) \
  X(FileWriteString, 1, 1, 0) \
  X(FileWriteByte, 1, 1, 0) \
  X(FileWriteNewline, 1, 1, 0) \
  X(PushF32, 1, 1, 1) \
  X(PushF64, 1, 1, 1) \
  X(AddF32, 1, 1, 1) \
  X(SubF32, 1, 1, 1) \
  X(MulF32, 1, 1, 1) \
  X(DivF32, 1, 1, 1) \
  X(NegF32, 1, 1, 1) \
  X(AddF64, 1, 1, 1) \
  X(SubF64, 1, 1, 1) \
  X(MulF64, 1, 1, 1) \
  X(DivF64, 1, 1, 1) \
  X(NegF64, 1, 1, 1) \
  X(CmpEqF32, 1, 1, 1) \
  X(CmpNeF32, 1, 1, 1) \
  X(CmpLtF32, 1, 1, 1) \
  X(CmpLeF32, 1, 1, 1) \
  X(CmpGtF32, 1, 1, 1) \
  X(CmpGeF32, 1, 1, 1) \
  X(CmpEqF64, 1, 1, 1) \
  X(CmpNeF64, 1, 1, 1) \
  X(CmpLtF64, 1, 1, 1) \
  X(CmpLeF64, 1, 1, 1) \
  X(CmpGtF64, 1, 1, 1) \
  X(CmpGeF64, 1, 1, 1) \
  X(ConvertI32ToF32, 1, 1, 1) \
  X(ConvertI32ToF64, 1, 1, 1) \
  X(ConvertI64ToF32, 1, 1, 1) \
  X(ConvertI64ToF64, 1, 1, 1) \
  X(ConvertU64ToF32, 1, 1, 1) \
  X(ConvertU64ToF64, 1, 1, 1) \
  X(ConvertF32ToI32, 1, 1, 1) \
  X(ConvertF32ToI64, 1, 1, 1) \
  X(ConvertF32ToU64, 1, 1, 1) \
  X(ConvertF64ToI32, 1, 1, 1) \
  X(ConvertF64ToI64, 1, 1, 1) \
  X(ConvertF64ToU64, 1, 1, 1) \
  X(ConvertF32ToF64, 1, 1, 1) \
  X(ConvertF64ToF32, 1, 1, 1) \
  X(ReturnF32, 1, 1, 1) \
  X(ReturnF64, 1, 1, 1) \
  X(PrintStringDynamic, 1, 1, 0) \
  X(Call, 1, 1, 1) \
  X(CallVoid, 1, 1, 1) \
  X(HeapAlloc, 0, 0, 0) \
  X(HeapFree, 0, 0, 0) \
  X(HeapRealloc, 0, 0, 0) \
  X(FileWriteStringDynamic, 1, 1, 0) \
  X(CallHost, 0, 0, 0)

struct IrOpcodeInfo {
  IrOpcode op;
  const char *name;
  bool glsl;
  bool wasm;
  bool wasmBrowser;
};

inline constexpr std::array IrOpcodeTable = {
#define PRIMEC_IR_OPCODE_ROW(name, glsl, wasm, wasmBrowser) \
  IrOpcodeInfo{IrOpcode::name, #name, glsl != 0, wasm != 0, wasmBrowser != 0},
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

} // namespace primec

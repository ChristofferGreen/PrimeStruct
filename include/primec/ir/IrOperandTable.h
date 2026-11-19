#pragma once

#include "primec/ir/Ir.h"
#include "primec/ir/IrOpcodeTable.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace primec {

// What the immediate operand (`IrInstruction::imm`) of an opcode names, which decides how the opcode is rewritten when
// modules are linked (primec/ir/IrLinker.h, docs/ModulesAndLibrariesPlan.md section 9). The classes come from the
// validator (src/ir/IrValidation.cpp) and the VM kernels (src/runtime), which are the ground truth for which opcode
// reads its immediate as what.
enum class IrOperandClass : uint8_t {
  Unclassified, // no class yet: scripts/check_ir_opcode_table.py and a unit test fail
  None,         // the immediate is not read
  Constant, // a value (PushI32, PushI64, PushF32 and PushF64 bits); never rewritten by index, but PushI32 and PushI64
  // also carry the string index of a string literal (see IrLinker.h for how the linker finds those)
  Local,      // a local slot of the function
  Jump,       // an instruction index of the same function
  Function,   // an index into IrModule::functions (Call, CallVoid, TaskSpawn)
  HostImport, // an index into IrModule::hostImports (CallHost)
  String,     // an index into IrModule::stringTable
  PrintString, // a string index in the bits from PrintStringIndexShift up, print flags below (PrintString)
  PrintFlags, // the print flags (PrintFlagMask)
  Width,      // an atomic width, 32 or 64
  ExitCode,   // the exit code of Trap
};

// One row per IrOpcode enumerator, in enum order: X(Name, operandClass). scripts/check_ir_opcode_table.py (ctest)
// fails when an enumerator has no row, the order differs, or a row is Unclassified, so a new opcode cannot be added
// without saying how it links. The static_asserts below tie the table to IrOpcodeTable.h.
#define PRIMEC_IR_OPERAND_TABLE(X)                                                                 \
  X(PushI32, Constant)                                                                             \
  X(PushI64, Constant)                                                                             \
  X(LoadLocal, Local)                                                                              \
  X(StoreLocal, Local)                                                                             \
  X(AddressOfLocal, Local)                                                                         \
  X(LoadIndirect, None)                                                                            \
  X(StoreIndirect, None)                                                                           \
  X(Dup, None)                                                                                     \
  X(Pop, None)                                                                                     \
  X(AddI32, None)                                                                                  \
  X(SubI32, None)                                                                                  \
  X(MulI32, None)                                                                                  \
  X(DivI32, None)                                                                                  \
  X(NegI32, None)                                                                                  \
  X(AddI64, None)                                                                                  \
  X(SubI64, None)                                                                                  \
  X(MulI64, None)                                                                                  \
  X(DivI64, None)                                                                                  \
  X(DivU64, None)                                                                                  \
  X(NegI64, None)                                                                                  \
  X(CmpEqI32, None)                                                                                \
  X(CmpNeI32, None)                                                                                \
  X(CmpLtI32, None)                                                                                \
  X(CmpLeI32, None)                                                                                \
  X(CmpGtI32, None)                                                                                \
  X(CmpGeI32, None)                                                                                \
  X(CmpEqI64, None)                                                                                \
  X(CmpNeI64, None)                                                                                \
  X(CmpLtI64, None)                                                                                \
  X(CmpLeI64, None)                                                                                \
  X(CmpGtI64, None)                                                                                \
  X(CmpGeI64, None)                                                                                \
  X(CmpLtU64, None)                                                                                \
  X(CmpLeU64, None)                                                                                \
  X(CmpGtU64, None)                                                                                \
  X(CmpGeU64, None)                                                                                \
  X(JumpIfZero, Jump)                                                                              \
  X(Jump, Jump)                                                                                    \
  X(ReturnVoid, None)                                                                              \
  X(ReturnI32, None)                                                                               \
  X(ReturnI64, None)                                                                               \
  X(PrintI32, PrintFlags)                                                                          \
  X(PrintI64, PrintFlags)                                                                          \
  X(PrintU64, PrintFlags)                                                                          \
  X(PrintString, PrintString)                                                                      \
  X(PushArgc, None)                                                                                \
  X(PrintArgv, PrintFlags)                                                                         \
  X(PrintArgvUnsafe, PrintFlags)                                                                   \
  X(LoadStringByte, String)                                                                        \
  X(LoadStringLength, None)                                                                        \
  X(FileOpenRead, String)                                                                          \
  X(FileOpenWrite, String)                                                                         \
  X(FileOpenAppend, String)                                                                        \
  X(FileOpenReadDynamic, None)                                                                     \
  X(FileOpenWriteDynamic, None)                                                                    \
  X(FileOpenAppendDynamic, None)                                                                   \
  X(FileReadByte, Local)                                                                           \
  X(FileClose, None)                                                                               \
  X(FileFlush, None)                                                                               \
  X(FileWriteI32, None)                                                                            \
  X(FileWriteI64, None)                                                                            \
  X(FileWriteU64, None)                                                                            \
  X(FileWriteString, String)                                                                       \
  X(FileWriteByte, None)                                                                           \
  X(FileWriteNewline, None)                                                                        \
  X(PushF32, Constant)                                                                             \
  X(PushF64, Constant)                                                                             \
  X(AddF32, None)                                                                                  \
  X(SubF32, None)                                                                                  \
  X(MulF32, None)                                                                                  \
  X(DivF32, None)                                                                                  \
  X(NegF32, None)                                                                                  \
  X(AddF64, None)                                                                                  \
  X(SubF64, None)                                                                                  \
  X(MulF64, None)                                                                                  \
  X(DivF64, None)                                                                                  \
  X(NegF64, None)                                                                                  \
  X(CmpEqF32, None)                                                                                \
  X(CmpNeF32, None)                                                                                \
  X(CmpLtF32, None)                                                                                \
  X(CmpLeF32, None)                                                                                \
  X(CmpGtF32, None)                                                                                \
  X(CmpGeF32, None)                                                                                \
  X(CmpEqF64, None)                                                                                \
  X(CmpNeF64, None)                                                                                \
  X(CmpLtF64, None)                                                                                \
  X(CmpLeF64, None)                                                                                \
  X(CmpGtF64, None)                                                                                \
  X(CmpGeF64, None)                                                                                \
  X(ConvertI32ToF32, None)                                                                         \
  X(ConvertI32ToF64, None)                                                                         \
  X(ConvertI64ToF32, None)                                                                         \
  X(ConvertI64ToF64, None)                                                                         \
  X(ConvertU64ToF32, None)                                                                         \
  X(ConvertU64ToF64, None)                                                                         \
  X(ConvertF32ToI32, None)                                                                         \
  X(ConvertF32ToI64, None)                                                                         \
  X(ConvertF32ToU64, None)                                                                         \
  X(ConvertF64ToI32, None)                                                                         \
  X(ConvertF64ToI64, None)                                                                         \
  X(ConvertF64ToU64, None)                                                                         \
  X(ConvertF32ToF64, None)                                                                         \
  X(ConvertF64ToF32, None)                                                                         \
  X(ReturnF32, None)                                                                               \
  X(ReturnF64, None)                                                                               \
  X(PrintStringDynamic, PrintFlags)                                                                \
  X(Call, Function)                                                                                \
  X(CallVoid, Function)                                                                            \
  X(HeapAlloc, None)                                                                               \
  X(HeapFree, None)                                                                                \
  X(HeapRealloc, None)                                                                             \
  X(FileWriteStringDynamic, None)                                                                  \
  X(CallHost, HostImport)                                                                          \
  X(LoadStringByteDynamic, None)                                                                   \
  X(SextI32, None)                                                                                 \
  X(HeapAllocBytes, None)                                                                          \
  X(LoadU8, None)                                                                                  \
  X(StoreU8, None)                                                                                 \
  X(ZextU8, None)                                                                                  \
  X(Trap, ExitCode)                                                                                \
  X(TaskSpawn, Function)                                                                           \
  X(TaskWait, None)                                                                                \
  X(AtomicLoad, Width)                                                                             \
  X(AtomicStore, Width)                                                                            \
  X(AtomicAdd, Width)                                                                              \
  X(AtomicExchange, Width)                                                                         \
  X(AtomicCompareExchange, Width)                                                                  \
  X(MutexLock, None)                                                                               \
  X(MutexUnlock, None)                                                                             \
  X(ConditionWait, None)                                                                           \
  X(ConditionNotify, None)                                                                         \
  X(FmaF32, None)                                                                                  \
  X(FmaF64, None)

struct IrOperandInfo {
  IrOpcode op;
  IrOperandClass operandClass;
};

inline constexpr std::array IrOperandTable = {
#define PRIMEC_IR_OPERAND_ROW(name, operandClass)                                                  \
  IrOperandInfo{IrOpcode::name, IrOperandClass::operandClass},
    PRIMEC_IR_OPERAND_TABLE(PRIMEC_IR_OPERAND_ROW)
#undef PRIMEC_IR_OPERAND_ROW
};

static_assert(IrOperandTable.size() == IrOpcodeTable.size(),
              "the operand table must have one row per opcode");

constexpr bool irOperandTableMatchesOpcodeTable() {
  for (size_t i = 0; i < IrOperandTable.size(); ++i) {
    if (IrOperandTable[i].op != IrOpcodeTable[i].op ||
        IrOperandTable[i].operandClass == IrOperandClass::Unclassified) {
      return false;
    }
  }
  return true;
}
static_assert(irOperandTableMatchesOpcodeTable(),
              "operand table rows must follow the opcode table, each with a class");

// Unclassified for a value outside [IrOpcodeMin, IrOpcodeMax].
constexpr IrOperandClass irOperandClass(IrOpcode op) {
  const auto value = static_cast<uint8_t>(op);
  if (value < IrOpcodeMin || value > IrOpcodeMax) {
    return IrOperandClass::Unclassified;
  }
  return IrOperandTable[static_cast<size_t>(value - IrOpcodeMin)].operandClass;
}

} // namespace primec

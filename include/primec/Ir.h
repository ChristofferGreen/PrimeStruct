#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

enum class IrOpcode : uint8_t {
  PushI32 = 1,
  PushI64,
  LoadLocal,
  StoreLocal,
  AddressOfLocal,
  LoadIndirect,
  StoreIndirect,
  Dup,
  Pop,
  AddI32,
  SubI32,
  MulI32,
  DivI32,
  NegI32,
  AddI64,
  SubI64,
  MulI64,
  DivI64,
  DivU64,
  NegI64,
  CmpEqI32,
  CmpNeI32,
  CmpLtI32,
  CmpLeI32,
  CmpGtI32,
  CmpGeI32,
  CmpEqI64,
  CmpNeI64,
  CmpLtI64,
  CmpLeI64,
  CmpGtI64,
  CmpGeI64,
  CmpLtU64,
  CmpLeU64,
  CmpGtU64,
  CmpGeU64,
  JumpIfZero,
  Jump,
  ReturnVoid,
  ReturnI32,
  ReturnI64,
  PrintI32,
  PrintI64,
  PrintU64,
  PrintString,
};

struct IrInstruction {
  IrOpcode op = IrOpcode::PushI32;
  uint64_t imm = 0;
};

struct IrFunction {
  std::string name;
  std::vector<IrInstruction> instructions;
};

struct IrModule {
  std::vector<IrFunction> functions;
  int32_t entryIndex = -1;
  std::vector<std::string> stringTable;
};

} // namespace primec

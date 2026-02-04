#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

enum class IrOpcode : uint8_t {
  PushI32 = 1,
  AddI32,
  SubI32,
  MulI32,
  DivI32,
  NegI32,
  ReturnI32,
};

struct IrInstruction {
  IrOpcode op = IrOpcode::PushI32;
  int32_t imm = 0;
};

struct IrFunction {
  std::string name;
  std::vector<IrInstruction> instructions;
};

struct IrModule {
  std::vector<IrFunction> functions;
  int32_t entryIndex = -1;
};

} // namespace primec

#pragma once

#include "primec/ir/Ir.h"
#include "primec/ir/IrModulePrinter.h"
#include "primec/ir/IrOpcodeTable.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// A tiny assembler so optimizer tests read as instruction lists:
//   assemble({"PushI32 2", "PushI32 3", "AddI32", "ReturnI32"})
// An immediate is a decimal (possibly negative) or 0x-prefixed hex integer;
// an opcode with no immediate takes 0.
namespace optimizer_test {

inline primec::IrOpcode opcodeByName(std::string_view name) {
  for (const primec::IrOpcodeInfo &info : primec::IrOpcodeTable) {
    if (name == info.name) {
      return info.op;
    }
  }
  FAIL("unknown opcode name in test: " << std::string(name));
  return primec::IrOpcode::PushI32;
}

inline primec::IrInstruction assembleOne(const std::string &line) {
  std::istringstream stream(line);
  std::string name;
  std::string immText;
  stream >> name >> immText;
  primec::IrInstruction instruction;
  instruction.op = opcodeByName(name);
  if (!immText.empty()) {
    if (immText.rfind("0x", 0) == 0) {
      instruction.imm = std::stoull(immText, nullptr, 16);
    } else if (immText[0] == '-') {
      instruction.imm = static_cast<uint64_t>(std::stoll(immText));
    } else {
      instruction.imm = std::stoull(immText);
    }
  }
  return instruction;
}

inline std::vector<primec::IrInstruction> assemble(std::initializer_list<const char *> lines) {
  std::vector<primec::IrInstruction> out;
  for (const char *line : lines) {
    out.push_back(assembleOne(line));
  }
  return out;
}

inline primec::IrModule moduleOf(std::vector<primec::IrInstruction> instructions, uint32_t parameterCount = 0) {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  module.functions.push_back(std::move(function));
  return module;
}

// Listing of function 0, for comparisons that print a readable diff.
inline std::string listing(const primec::IrModule &module) {
  return primec::formatIrFunction(module, 0);
}

} // namespace optimizer_test

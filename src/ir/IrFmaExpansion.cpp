#include "primec/ir/IrFmaExpansion.h"

#include "primec/ir/IrPureSemantics.h"

#include <algorithm>
#include <vector>

namespace primec {

namespace {

// One past the highest local any instruction names.
uint64_t freeLocalIndex(const IrFunction &function) {
  uint64_t next = 0;
  for (const IrInstruction &instruction : function.instructions) {
    if (instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal ||
        instruction.op == IrOpcode::AddressOfLocal || instruction.op == IrOpcode::FileReadByte) {
      next = std::max(next, instruction.imm + 1);
    }
  }
  return next;
}

IrInstruction makeInstruction(IrOpcode op, uint64_t imm, const IrInstruction &like) {
  IrInstruction instruction;
  instruction.op = op;
  instruction.imm = imm;
  instruction.debugId = like.debugId;
  return instruction;
}

void expandFunction(IrFunction &function) {
  const size_t count = function.instructions.size();
  const uint64_t temp = freeLocalIndex(function);
  // The new index of each original instruction; an Fma becomes four instructions.
  std::vector<size_t> newIndex(count + 1, 0);
  size_t next = 0;
  for (size_t i = 0; i < count; ++i) {
    newIndex[i] = next;
    next += isIrFmaOpcode(function.instructions[i].op) ? 4 : 1;
  }
  newIndex[count] = next;
  std::vector<IrInstruction> out;
  out.reserve(next);
  for (const IrInstruction &instruction : function.instructions) {
    if (isIrFmaOpcode(instruction.op)) {
      const bool isF64 = instruction.op == IrOpcode::FmaF64;
      out.push_back(makeInstruction(IrOpcode::StoreLocal, temp, instruction));
      out.push_back(makeInstruction(isF64 ? IrOpcode::MulF64 : IrOpcode::MulF32, 0, instruction));
      out.push_back(makeInstruction(IrOpcode::LoadLocal, temp, instruction));
      out.push_back(makeInstruction(isF64 ? IrOpcode::AddF64 : IrOpcode::AddF32, 0, instruction));
      continue;
    }
    IrInstruction copy = instruction;
    if ((copy.op == IrOpcode::Jump || copy.op == IrOpcode::JumpIfZero) && copy.imm <= count) {
      copy.imm = newIndex[static_cast<size_t>(copy.imm)];
    }
    out.push_back(copy);
  }
  function.instructions = std::move(out);
}

} // namespace

bool irModuleHasFusedMultiplyAdd(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (isIrFmaOpcode(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

void expandFusedMultiplyAdd(IrModule &module) {
  for (IrFunction &function : module.functions) {
    const bool hasFma =
        std::any_of(function.instructions.begin(),
                    function.instructions.end(),
                    [](const IrInstruction &instruction) { return isIrFmaOpcode(instruction.op); });
    if (hasFma) {
      expandFunction(function);
    }
  }
}

} // namespace primec

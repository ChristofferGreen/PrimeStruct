#include "primec/ir/IrIndirectCallExpansion.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace primec {

namespace {

constexpr uint64_t RuntimeErrorExitCode = 3;

bool isAddressOpcode(IrOpcode op) {
  return op == IrOpcode::FunctionAddress;
}

bool isIndirectCallOpcode(IrOpcode op) {
  return op == IrOpcode::CallIndirect || op == IrOpcode::CallIndirectVoid;
}

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

// Whether a function returns a value: it ends in a ReturnI32, ReturnI64, ReturnF32 or ReturnF64 (a function that
// returns nothing ends in ReturnVoid). A target that types its functions (Wasm) needs the call of a value to name a
// function that returns one.
bool returnsValue(const IrFunction &function) {
  return std::any_of(function.instructions.begin(), function.instructions.end(), [](const IrInstruction &instruction) {
    return instruction.op == IrOpcode::ReturnI32 || instruction.op == IrOpcode::ReturnI64 ||
           instruction.op == IrOpcode::ReturnF32 || instruction.op == IrOpcode::ReturnF64;
  });
}

// The functions whose address the module takes, in index order.
std::vector<uint64_t> addressTakenFunctions(const IrModule &module) {
  std::vector<bool> taken(module.functions.size(), false);
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (isAddressOpcode(instruction.op) && instruction.imm < taken.size()) {
        taken[static_cast<size_t>(instruction.imm)] = true;
      }
    }
  }
  std::vector<uint64_t> out;
  for (size_t i = 0; i < taken.size(); ++i) {
    if (taken[i]) {
      out.push_back(i);
    }
  }
  return out;
}

// The instructions that replace one indirect call.
size_t expandedLength(size_t parameterCount, size_t candidates, bool hasResult) {
  // Stores of the parameters and the address, then per candidate: the compare (4), the loads of the parameters, the
  // call, the store of its result and the jump to the end; the trap after the chain; and the load of the result, where
  // the arms join. The result goes through a local so that every arm leaves the operand stack as it found it, the
  // shape a structured back end (Wasm) recovers.
  const size_t resultSlots = hasResult ? 1 : 0;
  return parameterCount + 1 + candidates * (4 + parameterCount + resultSlots + 2) + 1 + resultSlots;
}

void expandFunction(const IrModule &module, const std::vector<uint64_t> &taken, IrFunction &function) {
  const size_t count = function.instructions.size();
  const uint64_t base = freeLocalIndex(function);
  // The functions an indirect call of p parameters can reach.
  const auto candidatesFor = [&](uint64_t parameterCount, bool wantsValue) {
    std::vector<uint64_t> out;
    for (const uint64_t index : taken) {
      const IrFunction &candidate = module.functions[static_cast<size_t>(index)];
      if (candidate.parameterCount == parameterCount && (!wantsValue || returnsValue(candidate))) {
        out.push_back(index);
      }
    }
    return out;
  };
  std::vector<size_t> newIndex(count + 1, 0);
  size_t next = 0;
  for (size_t i = 0; i < count; ++i) {
    newIndex[i] = next;
    const IrInstruction &instruction = function.instructions[i];
    next += isIndirectCallOpcode(instruction.op)
                ? expandedLength(static_cast<size_t>(instruction.imm),
                                 candidatesFor(instruction.imm, instruction.op == IrOpcode::CallIndirect).size(),
                                 instruction.op == IrOpcode::CallIndirect)
                : 1;
  }
  newIndex[count] = next;
  std::vector<IrInstruction> out;
  out.reserve(next);
  for (const IrInstruction &instruction : function.instructions) {
    if (isAddressOpcode(instruction.op)) {
      out.push_back(makeInstruction(IrOpcode::PushI64, instruction.imm + 1, instruction));
      continue;
    }
    if (!isIndirectCallOpcode(instruction.op)) {
      IrInstruction copy = instruction;
      if ((copy.op == IrOpcode::Jump || copy.op == IrOpcode::JumpIfZero) && copy.imm <= count) {
        copy.imm = newIndex[static_cast<size_t>(copy.imm)];
      }
      out.push_back(copy);
      continue;
    }
    const uint64_t parameterCount = instruction.imm;
    const std::vector<uint64_t> candidates =
        candidatesFor(parameterCount, instruction.op == IrOpcode::CallIndirect);
    const bool hasResult = instruction.op == IrOpcode::CallIndirect;
    const uint64_t resultLocal = base + parameterCount + 1;
    const size_t start = out.size();
    const size_t length = expandedLength(static_cast<size_t>(parameterCount), candidates.size(), hasResult);
    // The arms join where the result is loaded, or after the trap when there is none.
    const size_t end = start + length - (hasResult ? 1 : 0);
    // The address is the deepest slot: store the parameters from the top, then the address.
    for (uint64_t parameter = parameterCount; parameter > 0; --parameter) {
      out.push_back(makeInstruction(IrOpcode::StoreLocal, base + parameter, instruction));
    }
    out.push_back(makeInstruction(IrOpcode::StoreLocal, base, instruction));
    for (const uint64_t candidate : candidates) {
      out.push_back(makeInstruction(IrOpcode::LoadLocal, base, instruction));
      out.push_back(makeInstruction(IrOpcode::PushI64, candidate + 1, instruction));
      out.push_back(makeInstruction(IrOpcode::CmpEqI64, 0, instruction));
      const size_t skip = out.size();
      out.push_back(makeInstruction(IrOpcode::JumpIfZero, 0, instruction));
      for (uint64_t parameter = 1; parameter <= parameterCount; ++parameter) {
        out.push_back(makeInstruction(IrOpcode::LoadLocal, base + parameter, instruction));
      }
      out.push_back(makeInstruction(hasResult ? IrOpcode::Call : IrOpcode::CallVoid, candidate, instruction));
      if (hasResult) {
        out.push_back(makeInstruction(IrOpcode::StoreLocal, resultLocal, instruction));
      }
      out.push_back(makeInstruction(IrOpcode::Jump, end, instruction));
      out[skip].imm = out.size();
    }
    out.push_back(makeInstruction(IrOpcode::Trap, RuntimeErrorExitCode, instruction));
    if (hasResult) {
      out.push_back(makeInstruction(IrOpcode::LoadLocal, resultLocal, instruction));
    }
  }
  function.instructions = std::move(out);
}

} // namespace

bool irModuleHasIndirectCalls(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (isAddressOpcode(instruction.op) || isIndirectCallOpcode(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

void expandIndirectCalls(IrModule &module) {
  const std::vector<uint64_t> taken = addressTakenFunctions(module);
  for (IrFunction &function : module.functions) {
    const bool hasOpcode = std::any_of(function.instructions.begin(),
                                       function.instructions.end(),
                                       [](const IrInstruction &instruction) {
                                         return isAddressOpcode(instruction.op) ||
                                                isIndirectCallOpcode(instruction.op);
                                       });
    if (hasOpcode) {
      expandFunction(module, taken, function);
    }
  }
}

} // namespace primec

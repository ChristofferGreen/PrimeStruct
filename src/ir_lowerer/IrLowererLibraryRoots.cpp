#include "IrLowererLibraryRoots.h"

#include <cstddef>

namespace primec::ir_lowerer {

namespace {

// The opcodes whose operand is an index into IrModule::functions (primec/ir/IrOperandTable.h).
bool carriesFunctionIndex(IrOpcode op) {
  return op == IrOpcode::Call || op == IrOpcode::CallVoid || op == IrOpcode::TaskSpawn;
}

} // namespace

bool removeLibraryEntryStub(IrModule &module, std::string &error) {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "internal error: the library lowering produced no entry stub";
    return false;
  }
  const uint64_t removed = static_cast<uint64_t>(module.entryIndex);
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (carriesFunctionIndex(instruction.op) && instruction.imm == removed) {
        error = "internal error: a library function calls the entry stub";
        return false;
      }
    }
  }
  module.functions.erase(module.functions.begin() + static_cast<std::ptrdiff_t>(removed));
  for (IrFunction &function : module.functions) {
    for (IrInstruction &instruction : function.instructions) {
      if (carriesFunctionIndex(instruction.op) && instruction.imm > removed) {
        instruction.imm -= 1;
      }
    }
  }
  module.entryIndex = -1;
  return true;
}

} // namespace primec::ir_lowerer

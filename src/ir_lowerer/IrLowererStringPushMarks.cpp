#include "primec/ir_lowerer/IrLowererStringPushMarks.h"

namespace primec::ir_lowerer {

bool resolveStringLiteralPushMarks(IrModule &module, std::string &error) {
  for (IrFunction &function : module.functions) {
    for (IrInstruction &instruction : function.instructions) {
      const uint8_t opcode = static_cast<uint8_t>(instruction.op);
      if (opcode != IrStringPushI32Code && opcode != IrStringPushI64Code) {
        continue;
      }
      instruction.op = opcode == IrStringPushI32Code ? IrOpcode::PushI32 : IrOpcode::PushI64;
      instruction.isStringLiteral = true;
      if (instruction.imm >= module.stringTable.size()) {
        error = "native backend emitted a string literal push outside the string table in " +
                function.name;
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::ir_lowerer

#pragma once

#include <cstdint>
#include <string>

#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

// Lowering emits instructions through callbacks that only take an opcode and an immediate, so a string literal push
// is emitted as `stringLiteralPushOp(IrOpcode::PushI32 or PushI64)` and the marks are resolved once, after every
// function is lowered, by resolveStringLiteralPushMarks (the unresolved form is not a valid opcode, so a push that
// escaped the resolution is rejected by validation instead of linking wrongly).
constexpr IrOpcode stringLiteralPushOp(IrOpcode push) {
  return static_cast<IrOpcode>(push == IrOpcode::PushI32 ? IrStringPushI32Code
                                                         : IrStringPushI64Code);
}

// Turns every instruction emitted with stringLiteralPushOp into a PushI32 or PushI64 with IrInstruction::isStringLiteral
// set. Fails when the immediate of one is not an index into `module.stringTable`.
bool resolveStringLiteralPushMarks(IrModule &module, std::string &error);

} // namespace primec::ir_lowerer

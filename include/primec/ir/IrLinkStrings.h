#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// The string-literal pushes of one function: `isStringPush[i]` is true when instruction i is a PushI32 or PushI64
// whose immediate a string-consuming instruction reads as an index into the module's string table. A push is matched
// with its consumer when only single-slot pushes (PushI32, PushI64, LoadLocal) lie between them: PrintStringDynamic,
// LoadStringLength, FileOpen*Dynamic and FileWriteStringDynamic take the index from the top, LoadStringByteDynamic
// from below the byte position, and a CallHost takes each String parameter from its place among the arguments.
// `unprovenConsumer` names the opcode of the first consumer whose string operand is not such a push (empty when
// there is none); a string that reaches it came through a local, a parameter or a return value.
struct IrStringPushes {
  std::vector<bool> isStringPush;
  std::string unprovenConsumer;
};

IrStringPushes findIrStringLiteralPushes(const IrModule &module, const IrFunction &function);

} // namespace primec

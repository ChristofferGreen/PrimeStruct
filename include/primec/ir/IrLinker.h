#pragma once

#include <string>

#include "primec/ir/Ir.h"

namespace primec {

// The IR link of docs/ModulesAndLibrariesPlan.md, section 9: merges `library` into `module`, which keeps
// its entry point. What it does, in order:
//   - functions merge by name. An import (IrFunction::isImport) resolves to the definition in the other module, and
//     two definitions of one name are an error (`duplicate definition of <name>`), as are two declarations of one
//     name with different parameter counts. An import no module defines yet stays an import, so libraries can be
//     linked one after the other in any order; checkIrImportsResolved reports what is left.
//   - struct layouts merge by name; two layouts of one name must be equal.
//   - host imports merge by name and signature.
//   - the string table of `library` is appended and every operand that is a string index is shifted by the size
//     `module`'s table had. The operand classes of primec/ir/IrOperandTable.h say which opcodes carry a function
//     index, a host-import index or a string index. A string literal is pushed with PushI32 or PushI64, which the IR
//     cannot tell from an integer, so the linker finds those pushes by the instruction that consumes them (see
//     primec/ir/IrLinkStrings.h). A string that reaches a consumer any other way (through a local, a parameter or a
//     return value) cannot be followed, so a library that has strings and such a consumer is refused when its
//     strings would have to move (`cannot relocate the string operand of <opcode> in <function>`). Lowering does not
//     mark string literal pushes yet; the plan lists that as the way to lift this limit.
//   - instruction source maps are appended and the debug ids of `library` shifted past the ids of `module`.
// `module` is unchanged when the link fails. The result is not validated here: the caller runs validateIrModule.
bool linkIrModule(IrModule &module, const IrModule &library, std::string &error);

// Fails with `undefined symbol <name> (library <library>)` for the first function that is still an import.
bool checkIrImportsResolved(const IrModule &module, std::string &error);

} // namespace primec

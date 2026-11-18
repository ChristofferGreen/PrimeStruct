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
//     index, a host-import index or a string index. A string literal is pushed with PushI32 or PushI64, and lowering
//     marks each such push (IrInstruction::isStringLiteral, PSIR v39), so the linker shifts exactly the marked pushes
//     wherever the string flows afterwards (a local, a parameter, a return value). A library written before v39
//     (IrModule::stringPushesMarked is false) has no marks and is refused with a message that asks for a rebuild.
//   - instruction source maps are appended and the debug ids of `library` shifted past the ids of `module`.
// `module` is unchanged when the link fails. The result is not validated here: the caller runs validateIrModule.
bool linkIrModule(IrModule &module, const IrModule &library, std::string &error);

// Fails with `undefined symbol <name> (library <library>)` for the first function that is still an import.
bool checkIrImportsResolved(const IrModule &module, std::string &error);

} // namespace primec

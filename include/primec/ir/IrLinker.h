#pragma once

#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

struct IrLinkOptions {
  // Namespace roots whose functions are shared between the modules (`/std`): a function of the library under one of
  // them that `module` also defines is dropped, and the library's callers use the definition of `module`. This is for
  // the stdlib routines a library's code reached as real calls, which the program lowers under the same names
  // (docs/ModulesAndLibrariesPlan.md, section 9; docs/spec/integration-and-tooling.md, Compiling against a library).
  // The two definitions are the same source, so they are not compared. Every other name defined twice is an error.
  std::vector<std::string> sharedRoots;
};

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
bool linkIrModule(IrModule &module,
                  const IrModule &library,
                  std::string &error,
                  const IrLinkOptions *options = nullptr);

// Fails with `undefined symbol <name> (library <library>)` for the first function that is still an import.
bool checkIrImportsResolved(const IrModule &module, std::string &error);

} // namespace primec

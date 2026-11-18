#pragma once

// Where `[imported(<library>)]` may appear (docs/spec/integration-and-tooling.md, Compiling against a library): only in
// the interface text of a library archive, and only naming that library.

#include "primec/ast/Ast.h"
#include "primec/frontend/ExpandedSource.h"

#include <string>

namespace primec {

// Fails, naming the definition, for a definition marked `imported` in a source unit that is not a library interface
// (`imported is only valid in a library interface`), or in the interface of a library other than the one it names.
bool checkImportedDeclarationSites(const Program &program,
                                   const ExpandedSource &source,
                                   std::string &error);

} // namespace primec

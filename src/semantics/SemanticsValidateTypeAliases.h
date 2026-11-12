#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Resolves type aliases (docs/spec/type-system.md, Type aliases). A definition `[alias<Target>] Name() {}` names the
// type `Target`; this pass replaces every spelling of `Name` in a type position (a binding, parameter, return,
// template argument or brace constructor) with the target, so the later passes never see an alias. The alias
// definition stays behind as an empty struct, so an import of its path still finds a definition.
bool rewriteTypeAliases(Program &program, std::string &error);

} // namespace primec::semantics

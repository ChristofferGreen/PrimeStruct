#pragma once

// The definitions one definition refers to, for the closure hash of the symbol table (docs/ModulesAndLibrariesPlan.md,
// section 5). The scan is by name: a call target, a name, a type in a transform or a template argument that resolves to
// a defined path counts. It over-approximates (a local variable that shares a name with a definition counts), which is
// the safe side for a hash that decides whether something is rebuilt.

#include "primec/ast/Ast.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace primec {

// The parent path of `/a/b/c`, which is `/a/b`; empty for `/a` and for an empty path.
std::string parentDefinitionPath(const std::string &path);

// The paths in `definedPaths` that `definition` refers to, sorted and without duplicates; `definition` itself is not
// listed unless it calls itself.
std::vector<std::string>
collectDefinitionReferences(const Definition &definition,
                            const std::unordered_set<std::string> &definedPaths);

} // namespace primec

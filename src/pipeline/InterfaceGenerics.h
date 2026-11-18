#pragma once

// The generic definitions of a module interface (docs/ModulesAndLibrariesPlan.md, section 10). A generic is not lowered
// to code: the consumer instantiates it, so the interface holds it in full, body included, as it was parsed (semantic
// validation replaces a generic by its instances). Every function is a pure function of its inputs.

#include "primec/ast/Ast.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace primec {

// A definition that is a generic as parsed: it has template arguments and is not an instance.
bool isGenericDefinition(const Definition &definition);

// A `Create`, `Destroy`, `Copy` or `Move` helper nested in a struct. The lowerer expands these at each use instead of
// calling them, so a library has no function for one: the interface holds it in full and the consumer expands it.
bool isLifecycleHelper(const Definition &definition);

// The full text of a generic function or struct, or of a lifecycle helper, with its transforms (the `namespace` marker excepted), its template
// arguments and its body. `name` is the name printed (the path as written in the block). `methods` are the definitions
// nested in a generic struct, printed inside it with their name relative to the struct.
std::string genericDefinitionText(const Definition &definition,
                                  const std::string &name,
                                  const std::vector<const Definition *> &methods);

// The paths of the generic definitions of `seedPaths` and of every definition they refer to (private helpers and
// types, and the methods of a struct reached), through `definitions` (parsed definitions by path). A path under a
// reached struct is reached too. `seedPaths` are included.
std::set<std::string>
reachedFromGenerics(const std::unordered_map<std::string, const Definition *> &definitions,
                    const std::vector<std::string> &seedPaths);

} // namespace primec

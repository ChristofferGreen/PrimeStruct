#pragma once

#include <string>
#include <unordered_set>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

// Returns the fullPaths of definitions in `program` that participate in a
// call cycle (self-recursion or mutual recursion), based on each
// definition's already-resolved `Expr::resolvedCallPath` edges (populated by
// template monomorphization, before ir_lowerer runs). A definition is in the
// result set iff it is reachable from itself by following one or more call
// edges. Edges to paths outside `program.definitions` (unresolved/external)
// are ignored.
//
// This is a static call-graph analysis only; it does not consult
// `resolvedCallPath` for anything ir_lowerer treats as a builtin/intrinsic
// dispatch (those never populate `resolvedCallPath` to a `program.definitions`
// entry in the first place, so they never appear as edges here).
std::unordered_set<std::string> findRecursiveDefinitionPaths(const Program &program);

} // namespace primec::ir_lowerer

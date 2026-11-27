#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace primec::ir_lowerer {

// Whether `def` has a `string` result: a one-slot table index that a real call returns and its caller reads as it reads
// the result of an inlined call.
bool hasStringResult(const Definition &def);

// Whether every call site of `def` in the definitions of `reachable` uses its result only as a runtime string index:
// an argument of `print`, `print_line`, `print_error` or `print_line_error`, the operand of `count()`, the path of a
// file open, or an argument that a real call's `string` parameter takes (`stringParameterUsedOnlyAsIndexReads`).
// Indexing a string a call returned needs the dynamic string-byte operation, which only the VM has, so a function
// with any other use stays inline, where a result that is a static string indexes on every target. A call site is a
// call whose resolved path is `def`, and, to cover the calls the lowering writes itself (`Result.why` calls the `why`
// of the error type), any call whose name ends in the leaf of `def`.
bool stringResultUsedOnlyAsIndexReads(
    const std::unordered_map<std::string, const Definition *> &definitionsByPath,
    const std::unordered_set<std::string> &reachable,
    const Definition &def);

} // namespace primec::ir_lowerer

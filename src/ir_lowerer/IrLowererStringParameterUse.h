#pragma once

#include "primec/ast/Ast.h"

#include <string>

namespace primec::ir_lowerer {

// A call that opens the file at its one path argument: `File<Mode>(path)`, or the `/File/open...` helpers of the
// file library that forward to it.
bool isFileOpenWithPath(const Expr &call);

// Whether every use of the `string` parameter `name` in `def` reads the runtime string index the parameter holds:
// `count()` of it, its passing to `__pstext_literal`, or its being the path of a file open (`File<Mode>(name)`, which
// has a dynamic form that takes the index from the stack, or the `/File/open...` helpers that forward to it). Any
// other use (indexing, a comparison, a print, forwarding it) keeps the definition inline, where the string is a table
// index the lowering knows statically.
bool stringParameterUsedOnlyAsIndexReads(const Definition &def, const std::string &name);

} // namespace primec::ir_lowerer

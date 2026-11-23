#pragma once

#include "primec/ast/Ast.h"

#include <string>

namespace primec::ir_lowerer {

// Whether every use of the `string` parameter `name` in `def` is `count()` of it or its passing to
// `__pstext_literal`: the two reads of a runtime string that cost one instruction. Any other use (indexing, a
// comparison, a print, forwarding it) keeps the definition inline, where the string is a table index the lowering
// knows statically.
bool stringParameterUsedOnlyAsCountOrLiteral(const Definition &def, const std::string &name);

} // namespace primec::ir_lowerer

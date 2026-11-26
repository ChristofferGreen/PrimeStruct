#pragma once

#include <functional>
#include <string>
#include <vector>

#include "SemanticsHelpers.h"
#include "primec/ast/Ast.h"

namespace primec::semantics {

// The rules of the function address markers (primec/semantics/FunctionAddressDefinitions.h) as declarations: an empty
// body, an explicit return type, and for `[indirect_call]` the function address as the first parameter, a u64, and for
// `[function_address("/path")]` no parameters, a u64 result and a path that names a definition (`definitionExists`).
// Returns the problem as a sentence (`indirect_call declaration must have an empty body`), empty when the declaration
// is well formed; the caller appends the definition's path.
std::string functionAddressDeclarationProblem(const Definition &def,
                                              const std::vector<ParameterInfo> &params,
                                              const std::function<bool(const std::string &)> &definitionExists);

} // namespace primec::semantics

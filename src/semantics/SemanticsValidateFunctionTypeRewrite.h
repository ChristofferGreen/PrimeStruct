#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// `Function<R, A...>` spellings and conversions of the lambda-closures pass (docs/spec/value-lifecycle.md,
// Function values): the type is spelled as the arity-specific struct `/__FunctionN`, and `Function<R, A...>{x}`
// becomes `/__FunctionN<R, A...>{[invoke] 0u64, [clone] 0u64, [release] 0u64, [env] /__fn_box(x)}`, where the three
// addresses are placeholders the function-values pass replaces with the routines of the closure's type.

// Rewrites the `Function` spellings of one transform (a binding or return type) and records the arities seen.
void rewriteFunctionTransform(Transform &transform, std::set<size_t> &arities);

// Rewrites the `Function` spellings and explicit conversions below `expr`.
bool rewriteFunctionTypes(Expr &expr, std::set<size_t> &arities, std::string &error);

// Makes `construct` (a brace constructor of an instance of `/__FunctionN`) the conversion of `closure`: the placeholder
// addresses and the `env` block that `/__fn_box` fills.
void fillFunctionConstruction(Expr &construct, Expr closure);

// Parses generated source text into definitions appended to `out`.
bool parseGeneratedDefinitions(const std::string &text,
                               std::vector<Definition> &out,
                               std::string &error);

// The placeholder struct of one `Function` arity.
std::string functionShellText(size_t arity);

// The `/__fn_box` helper that moves a closure into its heap block.
const char *functionBoxText();

} // namespace primec::semantics

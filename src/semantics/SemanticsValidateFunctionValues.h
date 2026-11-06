#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Completes `Function<R, A...>` after template monomorphization (docs/spec/value-lifecycle.md, "Function values").
// The lambda-closures pass spelled each `Function<R, A...>` as an instance of `/__FunctionN` (a tag and a heap
// pointer) and each conversion `Function<R, A...>{x}` as a brace constructor whose `env` is `/__fn_box(x)`. Now that
// every converted closure type is concrete, this pass numbers them per Function instance (the tag) and writes the
// instance's `call` (dispatch on the tag), `Copy` (clone the boxed closure) and `Destroy` (drop and free it).
bool rewriteFunctionValues(Program &program, std::string &error);

} // namespace primec::semantics

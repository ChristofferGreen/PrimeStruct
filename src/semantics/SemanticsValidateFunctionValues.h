#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Completes `Function<R, A...>` after template monomorphization (docs/spec/value-lifecycle.md, "Function values").
// The lambda-closures pass spelled each `Function<R, A...>` as an instance of `/__FunctionN` (the addresses of three
// routines of its closure and a heap pointer) and each conversion `Function<R, A...>{x}` as a brace constructor whose
// `env` is `/__fn_box(x)` and whose addresses are placeholders. Now that every converted closure type is concrete,
// this pass writes the routines of each closure type (invoke, clone and release, SemanticsFunctionRoutines.h), puts
// the calls of their address getters in the conversion, and writes the instance's `call` as a call through the address
// of the invoke routine. The `Copy` and `Destroy` of the box are the same for every instance.
bool rewriteFunctionValues(Program &program, std::string &error);

} // namespace primec::semantics

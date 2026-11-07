#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Named definitions as Function values (docs/spec/value-lifecycle.md, "Function values"). A free function named where
// a value is expected (`Function<i32, i32>{/double}`, or `router.get("/", handle)` for a `Function` parameter) becomes
// a closure: a struct without fields, `/double__fnref`, whose `call` forwards to it. Run by the lambda-closures pass
// after it has boxed explicit conversions: the box of `Function<...>{name}` gets the forwarder's constructor here,
// and an argument at a call site is rewritten by template monomorphization once it knows the parameter type, so a
// forwarder is generated for every bare definition name passed as an argument or binding initializer. The generated
// definitions are appended to `generated`; `structPaths` are the program's struct definitions.
void generateFunctionForwarders(Program &program,
                                const std::unordered_set<std::string> &structPaths,
                                std::vector<Definition> &generated);

} // namespace primec::semantics

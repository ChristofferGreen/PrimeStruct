#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Turns every lambda into a closure struct (docs/spec/value-lifecycle.md, "Lambdas and closures"):
// one field per capture, a `call` method holding the body, and a generated factory whose implicit template
// arguments are the capture types. The lambda expression becomes a call of that factory, so template
// monomorphization infers the capture types from the enclosing locals.
bool rewriteLambdaClosures(Program &program, std::string &error);

// Marker in the name of every closure struct the pass generates (`/ns/main__lambda3`, after its enclosing definition).
inline constexpr const char *LambdaClosureStructPrefix = "__lambda";

// Suffix of the closure struct the pass generates for a named definition used as a `Function` value
// (`/double__fnref`): a struct without fields whose `call` forwards to the definition.
inline constexpr const char *FunctionForwarderSuffix = "__fnref";

} // namespace primec::semantics

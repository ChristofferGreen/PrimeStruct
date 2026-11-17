#pragma once

#include <string>
#include <string_view>

namespace primec::semantics {

// The diagnostic for a read-only parameter of an owning type that a definition tries to keep: `how` is the way it
// escapes ("return" or "assignment") and `name` the parameter. The message starts with
// `borrowed parameter escapes via <how>: <name>` and ends with the way out, the one place that says it: take the
// parameter as `[T copy]` or `[T move]` (docs/spec/value-lifecycle.md, Parameter Passing).
std::string borrowedParameterEscapeMessage(std::string_view how, const std::string &name);

} // namespace primec::semantics

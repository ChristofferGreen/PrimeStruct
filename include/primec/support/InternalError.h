#pragma once

#include <string>

namespace primec {

// A message that reports a violated compiler invariant (a `missing semantic-product ...` fact the semantic pass should
// have published) is not a mistake in the user's program: `error` with the prefix
// `internal compiler error (please report it): `. Any other message, and one already marked, comes back unchanged.
// Every stage that builds such a message marks it with this function (the lowerer's helper forwards to it).
std::string markInternalCompilerError(const std::string &error);

} // namespace primec

#pragma once

#include "primec/support/Options.h"
#include "primec/support/OptionsParser.h"

#include <iosfwd>
#include <string>

namespace primec {

// Reports a command-line argument failure for primec or primevm: a JSON diagnostic
// when --emit-diagnostics was given, otherwise "Argument error: ..." plus the usage text
// for `mode` on `err`. Returns the process exit code (2).
int reportArgumentError(std::ostream &err, const Options &options, std::string argError, OptionsParserMode mode);

} // namespace primec

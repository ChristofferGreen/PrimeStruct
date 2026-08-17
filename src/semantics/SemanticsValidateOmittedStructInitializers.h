#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec {

bool rewriteOmittedStructInitializers(Program &program, std::string &error);

} // namespace primec

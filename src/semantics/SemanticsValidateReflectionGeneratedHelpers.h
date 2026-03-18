#pragma once

#include <string>

#include "primec/Semantics.h"

namespace primec::semantics {

bool rewriteReflectionGeneratedHelpers(Program &program, std::string &error);

} // namespace primec::semantics

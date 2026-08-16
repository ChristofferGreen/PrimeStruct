#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

bool rewriteExperimentalGfxConstructors(Program &program, std::string &error);

} // namespace primec::semantics

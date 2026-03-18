#pragma once

#include <string>

#include "primec/Semantics.h"

namespace primec::semantics {

bool rewriteConvertConstructors(Program &program, std::string &error);

} // namespace primec::semantics

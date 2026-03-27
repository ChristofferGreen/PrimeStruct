#pragma once

#include "primec/Ast.h"

#include <string>

namespace primec::semantics {

bool rewriteEnumDefinitions(Program &program, std::string &error);

} // namespace primec::semantics

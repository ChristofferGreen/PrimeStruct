#pragma once

#include "primec/ast/Ast.h"

#include <string>

namespace primec::semantics {

bool rewriteEnumDefinitions(Program &program, std::string &error);

} // namespace primec::semantics

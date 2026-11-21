#pragma once

#include "primec/ast/Ast.h"

#include <string>

namespace primec::ir_lowerer {

// The first parameter a scalar-only call (a target without the address ABI) cannot take, as text that names the
// parameter and its type; empty when every parameter is a scalar value.
std::string scalarOnlyParameterBlocker(const Definition &def);

} // namespace primec::ir_lowerer

#pragma once

#include "IrLowererRecursionAnalysis.h"
#include "primec/ast/Ast.h"

#include <string>

namespace primec::ir_lowerer {

// The first parameter a scalar-only call (a target without the address ABI) cannot take, as text that names the
// parameter and its type; empty when every parameter is a scalar value.
std::string scalarOnlyParameterBlocker(const Definition &def);

// The first definition shape (`host`, `struct`, `sum`, `compute` or an `on_error` handler) that the scalar ABI never
// makes a function, as a code and text; empty when the definition has none of them.
RealCallBlocker scalarDefinitionShapeBlocker(const Definition &def);

} // namespace primec::ir_lowerer

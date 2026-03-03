#pragma once

#include <string>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out);
bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out);

} // namespace primec::ir_lowerer

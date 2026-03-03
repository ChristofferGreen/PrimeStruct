#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

bool isMathImportPath(const std::string &path);
bool hasProgramMathImport(const std::vector<std::string> &imports);
bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out);
bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out);

} // namespace primec::ir_lowerer

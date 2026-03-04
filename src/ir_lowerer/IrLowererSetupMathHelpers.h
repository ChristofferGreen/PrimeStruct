#pragma once

#include <functional>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

using GetSetupMathBuiltinNameFn = std::function<bool(const Expr &, std::string &)>;
using GetSetupMathConstantNameFn = std::function<bool(const std::string &, std::string &)>;

struct SetupMathResolvers {
  GetSetupMathBuiltinNameFn getMathBuiltinName;
  GetSetupMathConstantNameFn getMathConstantName;
};

bool isMathImportPath(const std::string &path);
bool hasProgramMathImport(const std::vector<std::string> &imports);
SetupMathResolvers makeSetupMathResolvers(bool hasMathImport);
GetSetupMathBuiltinNameFn makeGetSetupMathBuiltinName(bool hasMathImport);
GetSetupMathConstantNameFn makeGetSetupMathConstantName(bool hasMathImport);
bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out);
bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out);

} // namespace primec::ir_lowerer

#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {

bool validateVmProgramEffects(const Program &program,
                              const SemanticProgram *semanticProgram,
                              const std::string &entryPath,
                              const std::vector<std::string> &defaultEffects,
                              const std::vector<std::string> &entryDefaultEffects,
                              std::string &error);

} // namespace primec::ir_lowerer

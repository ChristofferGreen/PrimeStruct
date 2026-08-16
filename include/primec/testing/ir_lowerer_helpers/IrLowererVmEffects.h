#pragma once

#include <string>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec::ir_lowerer {

bool validateVmProgramEffects(const ::primec::Program &program,
                              const ::primec::SemanticProgram *semanticProgram,
                              const std::string &entryPath,
                              const std::vector<std::string> &defaultEffects,
                              const std::vector<std::string> &entryDefaultEffects,
                              std::string &error);

} // namespace primec::ir_lowerer

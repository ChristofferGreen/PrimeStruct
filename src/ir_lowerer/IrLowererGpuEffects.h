#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {

bool validateGpuNoSoftwareNumericTypes(const SemanticProgram *semanticProgram, std::string &error);
bool validateGpuNoRuntimeReflectionQueries(const SemanticProgram *semanticProgram, std::string &error);

bool validateGpuProgramEffects(const Program &program,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               std::string &error);

bool validateGpuProgramEffects(const Program &program,
                               const SemanticProgram *semanticProgram,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               std::string &error);

} // namespace primec::ir_lowerer

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {

bool runLowerEntrySetup(const Program &program,
                        const SemanticProgram *semanticProgram,
                        const std::string &entryPath,
                        const std::vector<std::string> &defaultEffects,
                        const std::vector<std::string> &entryDefaultEffects,
                        const Definition *&entryDefOut,
                        uint64_t &entryEffectMaskOut,
                        uint64_t &entryCapabilityMaskOut,
                        std::string &error);

} // namespace primec::ir_lowerer

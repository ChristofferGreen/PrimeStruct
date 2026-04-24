#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/IrValidation.h"

namespace primec {

struct Definition;
struct Program;
struct SemanticProgram;

namespace ir_lowerer {

bool runLowerEntrySetup(const ::primec::Program &program,
                        const ::primec::SemanticProgram *semanticProgram,
                        const std::string &entryPath,
                        const std::vector<std::string> &defaultEffects,
                        const std::vector<std::string> &entryDefaultEffects,
                        ::primec::IrValidationTarget validationTarget,
                        const ::primec::Definition *&entryDefOut,
                        uint64_t &entryEffectMaskOut,
                        uint64_t &entryCapabilityMaskOut,
                        std::string &error);

} // namespace ir_lowerer

} // namespace primec

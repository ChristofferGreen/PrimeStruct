#include "IrLowererVmEffects.h"

#include "IrLowererLowerEffects.h"

namespace primec::ir_lowerer {

bool validateVmProgramEffects(const Program &program,
                              const SemanticProgram *semanticProgram,
                              const std::string &entryPath,
                              const std::vector<std::string> &defaultEffects,
                              const std::vector<std::string> &entryDefaultEffects,
                              std::string &error) {
  return validateProgramEffectsForBackendSurface(
      program,
      semanticProgram,
      entryPath,
      defaultEffects,
      entryDefaultEffects,
      "vm backend",
      error);
}

} // namespace primec::ir_lowerer

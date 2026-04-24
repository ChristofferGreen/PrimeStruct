#include "IrLowererGpuEffects.h"

#include "IrLowererLowerEffects.h"

namespace primec::ir_lowerer {

bool validateGpuNoSoftwareNumericTypes(const SemanticProgram *semanticProgram, std::string &error) {
  return validateNoSoftwareNumericTypesForBackendSurface(semanticProgram, "gpu backend", error);
}

bool validateGpuNoRuntimeReflectionQueries(const SemanticProgram *semanticProgram, std::string &error) {
  return validateNoRuntimeReflectionQueriesForBackendSurface(semanticProgram, "gpu backend", error);
}

bool validateGpuProgramEffects(const Program &program,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               std::string &error) {
  return validateGpuProgramEffects(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, error);
}

bool validateGpuProgramEffects(const Program &program,
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
      "gpu backend",
      error);
}

} // namespace primec::ir_lowerer

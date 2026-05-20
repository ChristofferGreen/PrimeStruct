#include "IrLowererNativeEffects.h"

#include "IrLowererLowerEffects.h"

namespace primec::ir_lowerer {

bool validateNativeNoSoftwareNumericTypes(const SemanticProgram *semanticProgram, std::string &error) {
  return validateNoSoftwareNumericTypesForBackendSurface(semanticProgram, "native backend", error);
}

bool validateNativeNoRuntimeReflectionQueries(const SemanticProgram *semanticProgram, std::string &error) {
  return validateNoRuntimeReflectionQueriesForBackendSurface(semanticProgram, "native backend", error);
}

bool validateNativeProgramEffects(const Program &program,
                                  const std::string &entryPath,
                                  const std::vector<std::string> &defaultEffects,
                                  const std::vector<std::string> &entryDefaultEffects,
                                  std::string &error) {
  return validateNativeProgramEffects(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, error);
}

bool validateNativeProgramEffects(const Program &program,
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
      "native backend",
      error);
}

} // namespace primec::ir_lowerer

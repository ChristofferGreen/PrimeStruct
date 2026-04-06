#include "IrLowererLowerEntrySetup.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererLowerEffects.h"

namespace primec::ir_lowerer {

bool runLowerEntrySetup(const Program &program,
                        const SemanticProgram *semanticProgram,
                        const std::string &entryPath,
                        const std::vector<std::string> &defaultEffects,
                        const std::vector<std::string> &entryDefaultEffects,
                        const Definition *&entryDefOut,
                        uint64_t &entryEffectMaskOut,
                        uint64_t &entryCapabilityMaskOut,
                        std::string &error) {
  entryDefOut = nullptr;
  entryEffectMaskOut = 0;
  entryCapabilityMaskOut = 0;

  if (semanticProgram != nullptr && !semanticProgram->entryPath.empty() &&
      semanticProgram->entryPath != entryPath) {
    error = "semantic product entry path mismatch: expected " + entryPath + ", got " +
            semanticProgram->entryPath;
    return false;
  }

  if (!findEntryDefinition(program, entryPath, entryDefOut, error)) {
    return false;
  }
  if (!validateNoSoftwareNumericTypes(program, error)) {
    return false;
  }
  if (!validateNoRuntimeReflectionQueries(program, error)) {
    return false;
  }
  if (!validateSemanticProductDirectCallCoverage(program, semanticProgram, error)) {
    return false;
  }
  if (!validateProgramEffects(program, semanticProgram, entryPath, defaultEffects, entryDefaultEffects, error)) {
    return false;
  }
  if (!resolveEntryMetadataMasks(*entryDefOut,
                                 semanticProgram,
                                 entryPath,
                                 defaultEffects,
                                 entryDefaultEffects,
                                 entryEffectMaskOut,
                                 entryCapabilityMaskOut,
                                 error)) {
    return false;
  }

  return true;
}

} // namespace primec::ir_lowerer

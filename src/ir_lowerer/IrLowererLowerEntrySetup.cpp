#include "IrLowererLowerEntrySetup.h"

#include "IrLowererLowerEffects.h"

namespace primec::ir_lowerer {

bool runLowerEntrySetup(const Program &program,
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

  if (!findEntryDefinition(program, entryPath, entryDefOut, error)) {
    return false;
  }
  if (!validateNoSoftwareNumericTypes(program, error)) {
    return false;
  }
  if (!validateProgramEffects(program, entryPath, defaultEffects, entryDefaultEffects, error)) {
    return false;
  }
  if (!resolveEntryMetadataMasks(*entryDefOut,
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

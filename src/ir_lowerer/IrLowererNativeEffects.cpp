#include "IrLowererNativeEffects.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

bool validateNativeNoSoftwareNumericTypes(const SemanticProgram *semanticProgram, std::string &error) {
  return validateNoSoftwareNumericTypesForBackendSurface(semanticProgram, "native backend", error);
}

bool validateNativeNoRuntimeReflectionQueries(const SemanticProgram *semanticProgram, std::string &error) {
  return validateNoRuntimeReflectionQueriesForBackendSurface(semanticProgram, "native backend", error);
}

bool validateNativeMapValueKinds(const SemanticProgram *semanticProgram,
                                 std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }
  for (const auto &entry : semanticProgram->collectionSpecializations) {
    const std::string collectionFamily =
        normalizeCollectionBindingTypeName(resolveSemanticProductTypeText(
            semanticProgram, entry.collectionFamily, entry.collectionFamilyId));
    if (collectionFamily != "map") {
      continue;
    }
    const std::string valueTypeText = resolveSemanticProductTypeText(
        semanticProgram, entry.valueTypeText, entry.valueTypeTextId);
    if (valueKindFromTypeName(valueTypeText) != LocalInfo::ValueKind::Unknown) {
      continue;
    }
    error = "native backend only supports numeric/bool/string map values";
    return false;
  }
  return true;
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

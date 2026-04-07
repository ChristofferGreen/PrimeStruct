#include "IrLowererLowerEntrySetup.h"

#include <array>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererResultHelpers.h"

namespace primec::ir_lowerer {

namespace {

struct SemanticProductCompletenessContext {
  const Program &program;
  const SemanticProgram *semanticProgram = nullptr;
};

using SemanticProductCompletenessCheckFn =
    bool (*)(const SemanticProductCompletenessContext &, std::string &error);

struct SemanticProductCompletenessCheck {
  const char *factFamily = "";
  SemanticProductCompletenessCheckFn validate = nullptr;
};

bool validateDirectCallFactFamily(const SemanticProductCompletenessContext &context,
                                  std::string &error) {
  return validateSemanticProductDirectCallCoverage(context.program, context.semanticProgram, error);
}

bool validateBridgePathFactFamily(const SemanticProductCompletenessContext &context,
                                  std::string &error) {
  return validateSemanticProductBridgePathCoverage(context.program, context.semanticProgram, error);
}

bool validateMethodCallFactFamily(const SemanticProductCompletenessContext &context,
                                  std::string &error) {
  return validateSemanticProductMethodCallCoverage(context.program, context.semanticProgram, error);
}

bool validateBindingFactFamily(const SemanticProductCompletenessContext &context,
                               std::string &error) {
  return validateSemanticProductBindingCoverage(context.program, context.semanticProgram, error);
}

bool validateLocalAutoFactFamily(const SemanticProductCompletenessContext &context,
                                 std::string &error) {
  return validateSemanticProductLocalAutoCoverage(context.program, context.semanticProgram, error);
}

bool validateResultMetadataFactFamily(const SemanticProductCompletenessContext &context,
                                      std::string &error) {
  return validateSemanticProductResultMetadataCompleteness(context.semanticProgram, error);
}

const std::array<SemanticProductCompletenessCheck, 6> kSemanticProductCompletenessMatrix = {{
    {"routing.direct-call", validateDirectCallFactFamily},
    {"routing.bridge-path", validateBridgePathFactFamily},
    {"routing.method-call", validateMethodCallFactFamily},
    {"type-shape.binding", validateBindingFactFamily},
    {"type-shape.local-auto", validateLocalAutoFactFamily},
    {"result-control.metadata", validateResultMetadataFactFamily},
}};

bool validateSemanticProductCompletenessMatrix(const Program &program,
                                               const SemanticProgram *semanticProgram,
                                               std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductCompletenessContext context{
      .program = program,
      .semanticProgram = semanticProgram,
  };
  for (const auto &check : kSemanticProductCompletenessMatrix) {
    if (check.validate == nullptr) {
      continue;
    }
    if (!check.validate(context, error)) {
      return false;
    }
  }
  return true;
}

} // namespace

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
  if (!validateSemanticProductCompletenessMatrix(program, semanticProgram, error)) {
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

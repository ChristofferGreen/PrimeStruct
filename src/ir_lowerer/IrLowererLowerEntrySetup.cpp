#include "IrLowererLowerEntrySetup.h"

#include <array>
#include <string_view>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererNativeEffects.h"
#include "IrLowererOnErrorHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererVmEffects.h"

namespace primec::ir_lowerer {

namespace {

struct SemanticProductCompletenessContext {
  const Program &program;
  const Definition *entryDef = nullptr;
  const SemanticProgram *semanticProgram = nullptr;
};

using SemanticProductCompletenessCheckFn =
    bool (*)(const SemanticProductCompletenessContext &, std::string &error);

struct SemanticProductCompletenessCheck {
  const char *factFamily = "";
  const char *requiredField = "";
  SemanticProductCompletenessCheckFn validate = nullptr;
};

struct SemanticProductContractManifest {
  uint32_t version = SemanticProductContractVersionCurrent;
  const std::array<SemanticProductCompletenessCheck, 5> *checks = nullptr;
};

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

bool validateEntryParameterFactFamily(const SemanticProductCompletenessContext &context,
                                      std::string &error) {
  if (context.semanticProgram == nullptr || context.entryDef == nullptr) {
    return true;
  }
  bool hasEntryArgs = false;
  std::string entryArgsName;
  return resolveEntryArgsParameter(
      *context.entryDef, context.semanticProgram, hasEntryArgs, entryArgsName, error);
}

bool validateOnErrorFactFamily(const SemanticProductCompletenessContext &context,
                               std::string &error) {
  if (context.semanticProgram == nullptr) {
    return true;
  }
  const ResolveExprPathFn unusedResolveExprPath;
  const DefinitionExistsFn unusedDefinitionExists;
  OnErrorByDefinition ignoredOnErrorByDefinition;
  return buildOnErrorByDefinition(context.program,
                                  context.semanticProgram,
                                  unusedResolveExprPath,
                                  unusedDefinitionExists,
                                  ignoredOnErrorByDefinition,
                                  error);
}

const std::array<SemanticProductCompletenessCheck, 5> kSemanticProductCompletenessMatrix = {{
    {"type-shape.binding", "bindingFacts[].resolvedPathId", validateBindingFactFamily},
    {"type-shape.local-auto", "localAutoFacts[].bindingTypeText", validateLocalAutoFactFamily},
    {"result-control.entry-args", "entryPath + bindingFacts[]", validateEntryParameterFactFamily},
    {"result-control.on-error", "onErrorFacts[].handlerPathId", validateOnErrorFactFamily},
    {"result-control.metadata", "callableSummaries[].fullPathId", validateResultMetadataFactFamily},
}};

const SemanticProductContractManifest kSemanticProductContractManifestV1 = {
    .version = SemanticProductContractVersionV1,
    .checks = &kSemanticProductCompletenessMatrix,
};

bool validateModuleResolvedArtifactIdentity(const SemanticProgram &semanticProgram, std::string &error) {
  auto validateFamilyIndices = [&](std::string_view factFamily,
                                   std::string_view moduleKey,
                                   const std::vector<std::size_t> &indices,
                                   std::size_t familySize) {
    for (const std::size_t entryIndex : indices) {
      if (entryIndex < familySize) {
        continue;
      }
      const std::string moduleLabel =
          moduleKey.empty() ? "<unknown>" : std::string(moduleKey);
      error = "semantic-product contract module index out of range: family " +
              std::string(factFamily) + ", module " + moduleLabel + ", index " +
              std::to_string(entryIndex);
      return false;
    }
    return true;
  };

  for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
    const std::string_view moduleKey = module.identity.moduleKey;
    if (!validateFamilyIndices("routing.direct-call",
                               moduleKey,
                               module.directCallTargetIndices,
                               semanticProgram.directCallTargets.size()) ||
        !validateFamilyIndices("routing.method-call",
                               moduleKey,
                               module.methodCallTargetIndices,
                               semanticProgram.methodCallTargets.size()) ||
        !validateFamilyIndices("routing.bridge-path",
                               moduleKey,
                               module.bridgePathChoiceIndices,
                               semanticProgram.bridgePathChoices.size()) ||
        !validateFamilyIndices("type-shape.callable-summary",
                               moduleKey,
                               module.callableSummaryIndices,
                               semanticProgram.callableSummaries.size()) ||
        !validateFamilyIndices("type-shape.binding",
                               moduleKey,
                               module.bindingFactIndices,
                               semanticProgram.bindingFacts.size()) ||
        !validateFamilyIndices("type-shape.return",
                               moduleKey,
                               module.returnFactIndices,
                               semanticProgram.returnFacts.size()) ||
        !validateFamilyIndices("type-shape.local-auto",
                               moduleKey,
                               module.localAutoFactIndices,
                               semanticProgram.localAutoFacts.size()) ||
        !validateFamilyIndices("result-control.query",
                               moduleKey,
                               module.queryFactIndices,
                               semanticProgram.queryFacts.size()) ||
        !validateFamilyIndices("result-control.try",
                               moduleKey,
                               module.tryFactIndices,
                               semanticProgram.tryFacts.size()) ||
        !validateFamilyIndices("result-control.on-error",
                               moduleKey,
                               module.onErrorFactIndices,
                               semanticProgram.onErrorFacts.size())) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductCompletenessMatrix(const Program &program,
                                               const Definition &entryDef,
                                               const SemanticProgram *semanticProgram,
                                               std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }
  if (semanticProgram->contractVersion !=
      kSemanticProductContractManifestV1.version) {
    error = "semantic-product contract version mismatch: expected " +
            std::to_string(kSemanticProductContractManifestV1.version) + ", got " +
            std::to_string(semanticProgram->contractVersion);
    return false;
  }
  if (!validateModuleResolvedArtifactIdentity(*semanticProgram, error)) {
    return false;
  }
  if (kSemanticProductContractManifestV1.checks == nullptr) {
    error = "semantic-product contract manifest missing checks";
    return false;
  }

  const SemanticProductCompletenessContext context{
      .program = program,
      .entryDef = &entryDef,
      .semanticProgram = semanticProgram,
  };
  for (const auto &check : *kSemanticProductContractManifestV1.checks) {
    if (check.validate == nullptr) {
      continue;
    }
    if (!check.validate(context, error)) {
      if (error.empty()) {
        error = "semantic-product contract check failed: " +
                std::string(check.factFamily) + " requires " +
                std::string(check.requiredField);
      }
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
                        IrValidationTarget validationTarget,
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
  const bool useNativeEffectSurface = validationTarget == IrValidationTarget::Native;
  if ((useNativeEffectSurface && !validateNativeNoSoftwareNumericTypes(semanticProgram, error)) ||
      (!useNativeEffectSurface &&
       !validateNoSoftwareNumericTypesForBackendSurface(semanticProgram, "native backend", error))) {
    return false;
  }
  if ((useNativeEffectSurface &&
       !validateNativeNoRuntimeReflectionQueries(semanticProgram, error)) ||
      (!useNativeEffectSurface &&
       !validateNoRuntimeReflectionQueriesForBackendSurface(semanticProgram, "native backend", error))) {
    return false;
  }
  if (!validateSemanticProductCompletenessMatrix(
          program, *entryDefOut, semanticProgram, error)) {
    return false;
  }
  if ((validationTarget == IrValidationTarget::Vm &&
       !validateVmProgramEffects(
           program, semanticProgram, entryPath, defaultEffects, entryDefaultEffects, error)) ||
      (useNativeEffectSurface &&
      !validateNativeProgramEffects(
           program, semanticProgram, entryPath, defaultEffects, entryDefaultEffects, error)) ||
      (validationTarget != IrValidationTarget::Vm &&
       !useNativeEffectSurface &&
       !validateProgramEffectsForBackendSurface(
           program,
           semanticProgram,
           entryPath,
           defaultEffects,
           entryDefaultEffects,
           "native backend",
           error))) {
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

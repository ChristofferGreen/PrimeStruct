#include "IrLowererLowerSetupStage.h"

#include "IrLowererHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererLowerEntrySetup.h"
#include "IrLowererLowerImportsStructsSetup.h"
#include "IrLowererLowerLocalsSetup.h"
#include "IrLowererSetupMathHelpers.h"

namespace primec::ir_lowerer {

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut) {
  if (input.program == nullptr || input.entryPath == nullptr ||
      input.defaultEffects == nullptr || input.entryDefaultEffects == nullptr ||
      input.outModule == nullptr) {
    errorOut = "native backend missing lower setup stage input";
    return false;
  }

  stateOut = LowerSetupStageState{};

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  if (!runLowerEntrySetup(*input.program,
                          input.semanticProgram,
                          *input.entryPath,
                          *input.defaultEffects,
                          *input.entryDefaultEffects,
                          input.validationTarget,
                          stateOut.entryDef,
                          entryEffectMask,
                          entryCapabilityMask,
                          errorOut)) {
    return false;
  }

  if (!runLowerImportsStructsSetup(*input.program,
                                   input.semanticProgram,
                                   *input.outModule,
                                   stateOut.defMap,
                                   stateOut.structNames,
                                   stateOut.importAliases,
                                   stateOut.structFieldInfoByName,
                                   errorOut)) {
    return false;
  }

  stateOut.functionSyntaxProvenanceByName.reserve(stateOut.defMap.size());
  for (const auto &[path, definition] : stateOut.defMap) {
    FunctionSyntaxProvenance provenance;
    if (definition != nullptr) {
      if (definition->sourceLine > 0) {
        provenance.line = static_cast<uint32_t>(definition->sourceLine);
      }
      if (definition->sourceColumn > 0) {
        provenance.column = static_cast<uint32_t>(definition->sourceColumn);
      }
    }
    stateOut.functionSyntaxProvenanceByName.insert_or_assign(path, provenance);
  }

  stateOut.function.name = *input.entryPath;
  stateOut.function.metadata.effectMask = entryEffectMask;
  stateOut.function.metadata.capabilityMask = entryCapabilityMask;
  stateOut.function.metadata.schedulingScope = IrSchedulingScope::Default;
  stateOut.function.metadata.instrumentationFlags = 0;
  stateOut.hasMathImport = hasProgramMathImport(input.program->imports);

  if (!runLowerLocalsSetup(stateOut.stringTable,
                           stateOut.function,
                           *input.program,
                           input.semanticProgram,
                           *stateOut.entryDef,
                           *input.entryPath,
                           stateOut.defMap,
                           stateOut.importAliases,
                           stateOut.structNames,
                           stateOut.structFieldInfoByName,
                           stateOut.setupLocalsOrchestration,
                           errorOut)) {
    return false;
  }

  const auto &entryCountAccessSetup = stateOut.setupLocalsOrchestration.entryCountAccessSetup;
  const auto &countAccessClassifiers = entryCountAccessSetup.classifiers;
  const auto &entryCallOnErrorSetup = stateOut.setupLocalsOrchestration.entryCallOnErrorSetup;
  const auto &callResolutionAdapters = entryCallOnErrorSetup.callResolutionAdapters;
  const auto &setupMathResolvers = stateOut.setupLocalsOrchestration.setupMathResolvers;
  const auto &bindingTypeAdapters = stateOut.setupLocalsOrchestration.bindingTypeAdapters;
  auto hasExplicitBindingTypeTransform = bindingTypeAdapters.hasExplicitBindingTypeTransform;
  if (!hasExplicitBindingTypeTransform) {
    hasExplicitBindingTypeTransform = [](const Expr &expr) {
      return ir_lowerer::hasExplicitBindingTypeTransform(expr);
    };
  }
  const auto &setupTypeAndStructTypeAdapters =
      stateOut.setupLocalsOrchestration.setupTypeAndStructTypeAdapters;
  const auto &structTypeResolutionAdapters =
      setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
  const auto &structArrayInfoAdapters = stateOut.setupLocalsOrchestration.structArrayInfoAdapters;
  const auto &structSlotResolutionAdapters =
      stateOut.setupLocalsOrchestration.structSlotResolutionAdapters;
  const auto &uninitializedResolutionAdapters =
      stateOut.setupLocalsOrchestration.uninitializedResolutionAdapters;

  if (!runLowerInferenceSetup(
          {
              .program = input.program,
              .defMap = &stateOut.defMap,
              .importAliases = &stateOut.importAliases,
              .structNames = &stateOut.structNames,
              .semanticProgram = callResolutionAdapters.semanticProgram,
              .semanticIndex = &callResolutionAdapters.semanticProductTargets.semanticIndex,
              .isArrayCountCall = countAccessClassifiers.isArrayCountCall,
              .isStringCountCall = countAccessClassifiers.isStringCountCall,
              .isVectorCapacityCall = countAccessClassifiers.isVectorCapacityCall,
              .isEntryArgsName = countAccessClassifiers.isEntryArgsName,
              .resolveExprPath = callResolutionAdapters.resolveExprPath,
              .getBuiltinOperatorName = setupMathResolvers.getMathBuiltinName,
              .resolveStructArrayInfoFromPath =
                  structArrayInfoAdapters.resolveStructArrayTypeInfoFromPath,
              .inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath,
              .resolveStructFieldSlot = structSlotResolutionAdapters.resolveStructFieldSlot,
              .resolveUninitializedStorage = uninitializedResolutionAdapters.resolveUninitializedStorage,
              .hasMathImport = stateOut.hasMathImport,
              .combineNumericKinds = setupTypeAndStructTypeAdapters.combineNumericKinds,
              .getMathConstantName = setupMathResolvers.getMathConstantName,
              .resolveStructTypeName = structTypeResolutionAdapters.resolveStructTypeName,
              .isBindingMutable = bindingTypeAdapters.isBindingMutable,
              .bindingKind = bindingTypeAdapters.bindingKind,
              .hasExplicitBindingTypeTransform = hasExplicitBindingTypeTransform,
              .bindingValueKind = bindingTypeAdapters.bindingValueKind,
              .isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding,
              .setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo,
              .applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo,
              .applyStructValueInfo = stateOut.setupLocalsOrchestration.applyStructValueInfo,
              .isStringBinding = bindingTypeAdapters.isStringBinding,
              .lowerMatchToIf = lowerMatchToIf,
          },
          stateOut.inferenceSetupBootstrap,
          errorOut)) {
    return false;
  }

  return true;
}

} // namespace primec::ir_lowerer

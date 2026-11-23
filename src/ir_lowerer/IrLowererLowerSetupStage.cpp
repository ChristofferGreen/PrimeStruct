#include "primec/ir_lowerer/IrLowererLowerSetupStage.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerEntrySetup.h"
#include "primec/ir_lowerer/IrLowererLowerImportsStructsSetup.h"
#include "primec/ir_lowerer/IrLowererLowerLocalsSetup.h"
#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererSetupMathHelpers.h"
#include "primec/semantics/ImportedDefinitions.h"
#include "primec/support/SourceLocationMapper.h"

#include <algorithm>
#include <unordered_set>

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

  stateOut.entryDef = nullptr;
  stateOut.defMap.clear();
  stateOut.definitionPathIndex.reset();
  stateOut.structNames.clear();
  stateOut.importAliases.clear();
  stateOut.structFieldInfoByName.clear();
  stateOut.function = {};
  stateOut.sawReturn = false;
  stateOut.locals.clear();
  stateOut.nextLocal = 0;
  stateOut.onErrorTempCounter = 0;
  stateOut.stringTable.clear();
  stateOut.instructionSourceRangesByFunction.clear();
  stateOut.functionSyntaxProvenanceByName.clear();
  stateOut.expandedSource = nullptr;
  stateOut.fileScopeStack.clear();
  stateOut.currentOnError.reset();
  stateOut.currentReturnResult.reset();
  stateOut.hasMathImport = false;
  stateOut.inferenceSetupBootstrap = {};
  stateOut.expandedSource = input.expandedSource;
  stateOut.userSourceColumns = input.userSourceColumns;
  stateOut.allowDynamicStrings = input.validationTarget == IrValidationTarget::Vm;
  stateOut.realCallEligibleOrder.clear();
  stateOut.realCallReservationIndex.clear();
  // The address ABI needs a local address that names the same slot in every frame: the VM and optcpp number all live
  // locals, and the arm64 native backend addresses locals by their machine address (a caller's frame stays in place
  // while its callees run). The x86_64 native backend and its JIT keep the scalar ABI for now (TODO-5752).
  constexpr bool NativeHasFrameAddresses =
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
      true;
#else
      false;
#endif
  // The vm, serialized bytecode, optimized C++ and (where it has threads) arm64 native run tasks.
  stateOut.tasksRunOnScheduler =
      input.validationTarget == IrValidationTarget::Vm ||
      input.validationTarget == IrValidationTarget::Serialized ||
      input.validationTarget == IrValidationTarget::OptCpp ||
      (NativeHasFrameAddresses && input.validationTarget == IrValidationTarget::Native);
  stateOut.realCallAllowStructs =
      input.validationTarget == IrValidationTarget::Vm ||
      input.validationTarget == IrValidationTarget::OptCpp ||
      (NativeHasFrameAddresses && input.validationTarget == IrValidationTarget::Native);
  stateOut.realCallAllowStrings = input.validationTarget == IrValidationTarget::Vm ||
                                  input.validationTarget == IrValidationTarget::OptCpp;
  stateOut.runtimeErrorsTrap = stateOut.realCallAllowStructs ||
                               input.validationTarget == IrValidationTarget::Native ||
                               input.validationTarget == IrValidationTarget::Cpp ||
                               input.validationTarget == IrValidationTarget::Wasm ||
                               input.validationTarget == IrValidationTarget::WasmBrowser;
  {
    std::vector<std::string> ordered;
    for (const std::string &path :
         computeRealCallEligibleDefinitionPaths(*input.program,
                                                *input.entryPath,
                                                stateOut.realCallAllowStructs,
                                                stateOut.runtimeErrorsTrap,
                                                stateOut.realCallAllowStrings,
                                                input.libraryRoots)) {
      ordered.push_back(path);
    }
    std::sort(ordered.begin(), ordered.end());
    stateOut.realCallReservationIndex.reserve(ordered.size());
    for (size_t i = 0; i < ordered.size(); ++i) {
      stateOut.realCallReservationIndex.emplace(ordered[i], static_cast<uint64_t>(i));
    }
    stateOut.realCallEligibleOrder = std::move(ordered);
  }

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

  if (input.libraryRoots != nullptr) {
    RealCallShapeContext shapeContext{&stateOut.defMap,
                                      &stateOut.structNames,
                                      stateOut.realCallAllowStructs,
                                      stateOut.runtimeErrorsTrap,
                                      stateOut.realCallAllowStrings};
    shapeContext.allowSlotResults = true;
    for (const std::string &root : *input.libraryRoots) {
      if (stateOut.realCallReservationIndex.count(root) != 0) {
        continue;
      }
      std::string reason;
      const auto defIt = stateOut.defMap.find(root);
      if (defIt != stateOut.defMap.end() && defIt->second != nullptr) {
        reason = realCallBlockerReason(*defIt->second, shapeContext);
      }
      errorOut = "public function " + root + " cannot be compiled into a library: " +
                 (reason.empty() ? "its result, receiver or body shape is not one a real call takes"
                                 : reason);
      return false;
    }
  }

  // A function a library defines (an imported declaration) is called with the address ABI, as the library's code was
  // lowered: refuse a target that has no such ABI, and a declaration whose shape a real call cannot take.
  {
    std::vector<std::string> importedPaths;
    for (const auto &[path, definition] : stateOut.defMap) {
      if (definition != nullptr && isImportedDefinition(*definition)) {
        importedPaths.push_back(path);
      }
    }
    if (!importedPaths.empty()) {
      const std::unordered_set<std::string> reachable =
          findReachableDefinitionPaths(*input.program, *input.entryPath);
      std::sort(importedPaths.begin(), importedPaths.end());
      RealCallShapeContext shapeContext{&stateOut.defMap,
                                        &stateOut.structNames,
                                        stateOut.realCallAllowStructs,
                                        stateOut.runtimeErrorsTrap,
                                        stateOut.realCallAllowStrings};
      shapeContext.allowSlotResults = true;
      for (const std::string &path : importedPaths) {
        if (reachable.count(path) == 0) {
          continue;
        }
        const Definition &definition = *stateOut.defMap.at(path);
        if (!stateOut.realCallAllowStructs) {
          errorOut = "linking a library needs a target with the address ABI (the vm, optexe, "
                     "optcpp and arm64 "
                     "native): " +
                     path + " is imported from library " + importedLibraryName(definition);
          return false;
        }
        if (stateOut.realCallReservationIndex.count(path) == 0) {
          const std::string reason = realCallBlockerReason(definition, shapeContext);
          errorOut =
              "imported function " + path + " cannot be called on this target: " +
              (reason.empty() ? "its result, receiver or body shape is not one a real call takes"
                              : reason);
          return false;
        }
      }
    }
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
      if (input.expandedSource != nullptr && definition->sourceLine > 0 && definition->sourceColumn > 0) {
        const std::optional<SourceUnitLocation> location =
            input.userSourceColumns
                ? mapFilteredSourceLocation(
                      *input.expandedSource, definition->sourceLine, definition->sourceColumn)
                : mapExpandedSourceLocation(
                      *input.expandedSource, definition->sourceLine, definition->sourceColumn);
        if (location.has_value()) {
          if (location->line > 0) {
            provenance.line = static_cast<uint32_t>(location->line);
          }
          if (location->column > 0) {
            provenance.column = static_cast<uint32_t>(location->column);
          }
          provenance.sourceUnit = location->file;
        }
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

bool registerRealCallOwnedParameters(
    const Definition &def,
    LocalMap &bodyLocals,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const std::function<const Definition *(const std::string &)> &findOwnedSumDefinition,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::vector<LowerSetupStageState::DropEntry> &dropEntries,
    std::vector<std::vector<int32_t>> &fileScopeStack,
    std::string &errorOut) {
  if (fileScopeStack.empty()) {
    errorOut = "internal error: no scope for the parameters of real-call target " + def.fullPath;
    return false;
  }
  for (const Expr &param : def.parameters) {
    // A builtin `vector<T>` parameter (a spawned call's): the record's address arrives in one slot. The body needs
    // the element layout to step through it, and owns the vector (destroys its elements and frees the buffer) when
    // the parameter is `copy` or `move`.
    // A borrowed one (`mut`, read-only, or `Reference<vector<T>>`: a direct call passes the caller's record address)
    // needs the same layout and, for `mut`, the element path that assigning to the parameter replaces the caller's
    // record with.
    if (const auto vectorIt = bodyLocals.find(param.name);
        vectorIt != bodyLocals.end() &&
        ((vectorIt->second.kind == LocalInfo::Kind::Vector && !vectorIt->second.isSoaVector) ||
         (vectorIt->second.kind == LocalInfo::Kind::Reference &&
          vectorIt->second.referenceToVector && !vectorIt->second.isSoaVector))) {
      LocalInfo &vectorInfo = vectorIt->second;
      std::string elementStructPath;
      bool elementKnown = false;
      for (const Transform &transform : param.transforms) {
        if (transform.templateArgs.size() != 1) {
          continue;
        }
        std::string elementText;
        if (transform.name == "vector") {
          elementText = transform.templateArgs.front();
        } else if (transform.name == "Reference") {
          std::string base;
          std::string argText;
          if (!splitTemplateTypeName(
                  trimTemplateTypeText(transform.templateArgs.front()), base, argText) ||
              trimTemplateTypeText(base) != "vector") {
            continue;
          }
          elementText = argText;
        } else {
          continue;
        }
        elementKnown = resolveBuiltinVectorElementStructPath(
            elementText, param.namespacePrefix, resolveStructTypeName, elementStructPath);
        break;
      }
      if (elementKnown && vectorInfo.kind == LocalInfo::Kind::Vector &&
          !parameterHasTransform(param, "copy") && !parameterHasTransform(param, "move") &&
          parameterHasTransform(param, "mut")) {
        vectorInfo.vectorElementLifecyclePath = elementStructPath;
      }
      StructSlotLayoutInfo elementLayout;
      if (elementKnown && !elementStructPath.empty() &&
          vectorInfo.vectorStructElementSlotCount == 0 &&
          resolveStructSlotLayout(elementStructPath, elementLayout)) {
        vectorInfo.vectorStructElementSlotCount = elementLayout.totalSlots;
      }
      if (elementKnown &&
          (parameterHasTransform(param, "copy") || parameterHasTransform(param, "move"))) {
        std::string destroyElementPath;
        if (!elementStructPath.empty() &&
            structNeedsDestroyHelpers(
                elementStructPath,
                [&](const std::string &path) { return findStackDestroyHelper(defMap, path); },
                resolveStructSlotLayout)) {
          destroyElementPath = elementStructPath;
        }
        if (parameterHasTransform(param, "mut")) {
          vectorInfo.vectorElementLifecyclePath = elementStructPath;
        }
        vectorInfo.dropFlagLocal = allocTempLocal();
        instructions.push_back({IrOpcode::PushI32, 1});
        instructions.push_back(
            {IrOpcode::StoreLocal, static_cast<uint64_t>(vectorInfo.dropFlagLocal)});
        LowerSetupStageState::DropEntry entry{
            vectorInfo.index, vectorInfo.dropFlagLocal, destroyElementPath};
        entry.builtinVectorElementSlots =
            std::max<int32_t>(1, vectorInfo.vectorStructElementSlotCount);
        dropEntries.push_back(std::move(entry));
        fileScopeStack.back().push_back(-static_cast<int32_t>(dropEntries.size()));
      }
      continue;
    }
    if (!parameterHasTransform(param, "copy") && !parameterHasTransform(param, "move")) {
      continue;
    }
    auto ownedIt = bodyLocals.find(param.name);
    // An owned sum (a spawned call's): the body owns its active payload and destroys it, unless it is moved on.
    if (ownedIt != bodyLocals.end() && ownedIt->second.kind == LocalInfo::Kind::Value &&
        !ownedIt->second.structTypeName.empty() && findOwnedSumDefinition) {
      if (const Definition *sumDef = findOwnedSumDefinition(ownedIt->second.structTypeName);
          sumDef != nullptr) {
        LocalInfo &sumInfo = ownedIt->second;
        sumInfo.dropFlagLocal = allocTempLocal();
        instructions.push_back({IrOpcode::PushI32, 1});
        instructions.push_back(
            {IrOpcode::StoreLocal, static_cast<uint64_t>(sumInfo.dropFlagLocal)});
        LowerSetupStageState::DropEntry entry{
            sumInfo.index, sumInfo.dropFlagLocal, sumDef->fullPath};
        entry.sumDef = sumDef;
        dropEntries.push_back(std::move(entry));
        fileScopeStack.back().push_back(-static_cast<int32_t>(dropEntries.size()));
        continue;
      }
    }
    if (ownedIt == bodyLocals.end() || ownedIt->second.structTypeName.empty() ||
        (ownedIt->second.kind != LocalInfo::Kind::Value &&
         ownedIt->second.kind != LocalInfo::Kind::Reference) ||
        !structNeedsDestroyHelpers(
            ownedIt->second.structTypeName,
            [&](const std::string &path) { return findStackDestroyHelper(defMap, path); },
            resolveStructSlotLayout)) {
      continue;
    }
    LocalInfo &ownedInfo = ownedIt->second;
    ownedInfo.dropFlagLocal = allocTempLocal();
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ownedInfo.dropFlagLocal)});
    dropEntries.push_back({ownedInfo.index, ownedInfo.dropFlagLocal, ownedInfo.structTypeName});
    fileScopeStack.back().push_back(-static_cast<int32_t>(dropEntries.size()));
  }
  return true;
}

} // namespace primec::ir_lowerer

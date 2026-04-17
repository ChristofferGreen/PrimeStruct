cmake_minimum_required(VERSION 3.20)
project(PrimeStruct LANGUAGES CXX)

option(PRIMESTRUCT_BUILD_TESTS "Build PrimeStruct tests" ON)
option(PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE
       "Enable ThreadSanitizer instrumentation and semantics smoke target"
       OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE)
  if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR
            "PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE requires clang++ on macOS because GCC does not provide a usable libtsan runtime")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR
            "PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE requires Clang or GCC")
  endif()
  include(CheckCXXSourceCompiles)
  set(_primestruct_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
  set(_primestruct_saved_required_link_options "${CMAKE_REQUIRED_LINK_OPTIONS}")
  string(APPEND CMAKE_REQUIRED_FLAGS " -fsanitize=thread")
  list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=thread")
  check_cxx_source_compiles(
    "int main() { return 0; }"
    PRIMESTRUCT_TSAN_LINK_PROBE_PASSED
  )
  set(CMAKE_REQUIRED_FLAGS "${_primestruct_saved_required_flags}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "${_primestruct_saved_required_link_options}")
  if(NOT PRIMESTRUCT_TSAN_LINK_PROBE_PASSED)
    message(FATAL_ERROR
            "PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE requested but the toolchain cannot link a ThreadSanitizer binary")
  endif()
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

function(primestructEnableWarnings targetName)
  if(MSVC)
    target_compile_options(${targetName} PRIVATE
      /W4
      $<$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>:/WX>
    )
  else()
    target_compile_options(${targetName} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      $<$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>:-Werror>
    )
  endif()
endfunction()

set(PRIMESTRUCT_SUPPORT_SOURCES
  src/Diagnostics.cpp
  src/EmitKind.cpp
  src/OptionsParser.cpp
  src/ProcessRunner.cpp
  src/SymbolInterner.cpp
  src/TempPaths.cpp
  src/TransformRules.cpp
  src/TransformRegistry.cpp
)

set(PRIMESTRUCT_FRONTEND_SOURCES
  src/AstPrinter.cpp
  src/ImportResolver.cpp
  src/ImportResolverHelpers.cpp
  src/Lexer.cpp
  src/SemanticProduct.cpp
  src/parser/ParserCore.cpp
  src/parser/ParserCoreBodyStatements.cpp
  src/parser/ParserCoreDefinitionBodies.cpp
  src/parser/ParserCoreDefinitions.cpp
  src/parser/ParserExpr.cpp
  src/parser/ParserExprControl.cpp
  src/parser/ParserHelpers.cpp
  src/parser/ParserLists.cpp
  src/parser/ParserListsTransforms.cpp
  src/parser/ParserListsSignatures.cpp
  src/semantics/SemanticsBindingTypeHelpers.cpp
  src/semantics/SemanticsBuiltinPathHelpers.cpp
  src/semantics/SemanticsCallPathHelpers.cpp
  src/semantics/SemanticsDefinitionPrepass.cpp
  src/semantics/SemanticsDefinitionPartitioner.cpp
  src/semantics/SemanticsReturnKindHelpers.cpp
  src/semantics/SemanticsHelpersCore.cpp
  src/semantics/SemanticsHelpersValidation.cpp
  src/semantics/SemanticsArgumentOrdering.cpp
  src/semantics/TemplateMonomorph.cpp
  src/semantics/CondensationDag.cpp
  src/semantics/StronglyConnectedComponents.cpp
  src/semantics/TypeResolutionGraph.cpp
  src/semantics/TypeResolutionGraphPreparation.cpp
  src/semantics/SemanticsValidate.cpp
  src/semantics/SemanticsValidateConvertConstructors.cpp
  src/semantics/SemanticsValidateExperimentalGfxConstructors.cpp
  src/semantics/SemanticsValidateExperimentalGfxConstructorsContext.cpp
  src/semantics/SemanticsValidateReflectionGeneratedHelpersCloneDebug.cpp
  src/semantics/SemanticsValidateReflectionGeneratedHelpersCompare.cpp
  src/semantics/SemanticsValidateReflectionGeneratedHelpers.cpp
  src/semantics/SemanticsValidateReflectionGeneratedHelpersSerialization.cpp
  src/semantics/SemanticsValidateReflectionGeneratedHelpersState.cpp
  src/semantics/SemanticsValidateReflectionGeneratedHelpersValidate.cpp
  src/semantics/SemanticsValidateReflectionMetadataContext.cpp
  src/semantics/SemanticsValidateReflectionMetadata.cpp
  src/semantics/SemanticsValidateTransformsEnums.cpp
  src/semantics/SemanticsValidateTransforms.cpp
  src/semantics/SemanticsValidator.cpp
  src/semantics/SemanticsValidatorSnapshots.cpp
  src/semantics/SemanticsValidatorSnapshotLocals.cpp
  src/semantics/SemanticsValidatorValidationContext.cpp
  src/semantics/SemanticsValidatorDiagnostics.cpp
  src/semantics/SemanticsValidatorBorrowExpiry.cpp
  src/semantics/SemanticsValidatorResultHelpers.cpp
  src/semantics/SemanticsValidatorBuild.cpp
  src/semantics/SemanticsValidatorBuildTransforms.cpp
  src/semantics/SemanticsValidatorBuildImports.cpp
  src/semantics/SemanticsValidatorBuildReturnKinds.cpp
  src/semantics/SemanticsValidatorBuildLifecycle.cpp
  src/semantics/SemanticsValidatorBuildParameters.cpp
  src/semantics/SemanticsValidatorBuildCallResolution.cpp
  src/semantics/SemanticsValidatorBuildStructFields.cpp
  src/semantics/SemanticsValidatorBuildUninitialized.cpp
  src/semantics/SemanticsValidatorBuildUtility.cpp
  src/semantics/SemanticsValidatorBuildDirectCallBinding.cpp
  src/semantics/SemanticsValidatorBuildInitializerInference.cpp
  src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp
  src/semantics/SemanticsValidatorEffectFreeCollections.cpp
  src/semantics/SemanticsValidatorExprBlock.cpp
  src/semantics/SemanticsValidatorExprCaptureSplitStep.cpp
  src/semantics/SemanticsValidatorExprBodyArguments.cpp
  src/semantics/SemanticsValidatorExprControlFlow.cpp
  src/semantics/SemanticsValidatorExprDispatchBootstrap.cpp
  src/semantics/SemanticsValidatorExprPreDispatchDirectCalls.cpp
  src/semantics/SemanticsValidatorExprMethodCompatibilitySetup.cpp
  src/semantics/SemanticsValidatorExprCollectionDispatchSetup.cpp
  src/semantics/SemanticsValidatorExprCollectionAccessSetup.cpp
  src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp
  src/semantics/SemanticsValidatorExprDirectCollectionFallbacks.cpp
  src/semantics/SemanticsValidatorExprPostAccessPrechecks.cpp
  src/semantics/SemanticsValidatorExprBuiltinContextSetup.cpp
  src/semantics/SemanticsValidatorExprCollectionAccess.cpp
  src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp
  src/semantics/SemanticsValidatorExprCollectionCountCapacity.cpp
  src/semantics/SemanticsValidatorExprCountCapacityMapBuiltins.cpp
  src/semantics/SemanticsValidatorExprLateCollectionAccessFallbacks.cpp
  src/semantics/SemanticsValidatorExprLateFallbackBuiltins.cpp
  src/semantics/SemanticsValidatorExprCollectionPredicates.cpp
  src/semantics/SemanticsValidatorExprCollectionLiterals.cpp
  src/semantics/SemanticsValidatorExprFieldResolution.cpp
  src/semantics/SemanticsValidatorExprGpuBuffer.cpp
  src/semantics/SemanticsValidatorExprLambda.cpp
  src/semantics/SemanticsValidatorExprLateCallCompatibility.cpp
  src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp
  src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp
  src/semantics/SemanticsValidatorExprMethodResolution.cpp
  src/semantics/SemanticsValidatorExprNumeric.cpp
  src/semantics/SemanticsValidatorExprNumericPredicates.cpp
  src/semantics/SemanticsValidatorExprArgumentValidation.cpp
  src/semantics/SemanticsValidatorExprArgumentValidationCollections.cpp
  src/semantics/SemanticsValidatorExprLateBuiltins.cpp
  src/semantics/SemanticsValidatorExprLateMapSoaBuiltins.cpp
  src/semantics/SemanticsValidatorExprLateUnknownTargetFallbacks.cpp
  src/semantics/SemanticsValidatorExprNamedArgumentBuiltins.cpp
  src/semantics/SemanticsValidatorExprMutationBorrows.cpp
  src/semantics/SemanticsValidatorExprPointerLike.cpp
  src/semantics/SemanticsValidatorExprReferenceEscapes.cpp
  src/semantics/SemanticsValidatorExprReceiverPaths.cpp
  src/semantics/SemanticsValidatorCollectionHelperRewrites.cpp
  src/semantics/SemanticsValidatorExprResultFile.cpp
  src/semantics/SemanticsValidatorExprScalarPointerMemory.cpp
  src/semantics/SemanticsValidatorExprResolvedCallSetup.cpp
  src/semantics/SemanticsValidatorExprResolvedCallArguments.cpp
  src/semantics/SemanticsValidatorExprStructConstructors.cpp
  src/semantics/SemanticsValidatorExprTry.cpp
  src/semantics/SemanticsValidatorExprVectorHelpers.cpp
  src/semantics/SemanticsValidatorExprCallResolution.cpp
  src/semantics/SemanticsValidatorExpr.cpp
  src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp
  src/semantics/SemanticsValidatorInferCollectionCallResolution.cpp
  src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp
  src/semantics/SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp
  src/semantics/SemanticsValidatorInferCollectionStringResolver.cpp
  src/semantics/SemanticsValidatorInferCollectionCountCapacity.cpp
  src/semantics/SemanticsValidatorInferCollectionDirectCountCapacity.cpp
  src/semantics/SemanticsValidatorInferCollectionDispatch.cpp
  src/semantics/SemanticsValidatorInferCollectionDispatchSetup.cpp
  src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp
  src/semantics/SemanticsValidatorInferCollections.cpp
  src/semantics/SemanticsValidatorInferControlFlow.cpp
  src/semantics/SemanticsValidatorInferDefinition.cpp
  src/semantics/SemanticsValidatorInferGraph.cpp
  src/semantics/SemanticsValidatorInferMethodResolution.cpp
  src/semantics/SemanticsValidatorInferMethodResolutionHelpers.cpp
  src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp
  src/semantics/SemanticsValidatorInferResolvedCalls.cpp
  src/semantics/SemanticsValidatorInferScalarBuiltins.cpp
  src/semantics/SemanticsValidatorInferStructReturnHelpers.cpp
  src/semantics/SemanticsValidatorInferTargetResolution.cpp
  src/semantics/SemanticsValidatorInferUtility.cpp
  src/semantics/SemanticsValidatorInferStructReturn.cpp
  src/semantics/SemanticsValidatorInfer.cpp
  src/semantics/SemanticsValidatorPassesEffects.cpp
  src/semantics/SemanticsValidatorPassesDiagnostics.cpp
  src/semantics/SemanticsValidatorExecutionDiagnostics.cpp
  src/semantics/SemanticsValidatorPassesDefinitions.cpp
  src/semantics/SemanticsValidatorPassesEffectFree.cpp
  src/semantics/SemanticsValidatorPassesExecutions.cpp
  src/semantics/SemanticsValidatorPassesOmittedInitializers.cpp
  src/semantics/SemanticsValidatorPassesStructLayouts.cpp
  src/semantics/SemanticsValidatorPassesUninitialized.cpp
  src/semantics/SemanticsValidatorPasses.cpp
  src/semantics/SemanticsValidatorStatementBindings.cpp
  src/semantics/SemanticsValidatorStatementBodyArguments.cpp
  src/semantics/SemanticsValidatorStatementBuiltins.cpp
  src/semantics/SemanticsValidatorStatementContainerHelpers.cpp
  src/semantics/SemanticsValidatorStatementControlFlow.cpp
  src/semantics/SemanticsValidatorStatementLoopCountStep.cpp
  src/semantics/SemanticsValidatorStatementPrintability.cpp
  src/semantics/SemanticsValidatorStatementReturns.cpp
  src/semantics/SemanticsValidatorStatementVectorHelpers.cpp
  src/semantics/SemanticsValidatorStatementVectorResolution.cpp
  src/semantics/SemanticsValidatorStatement.cpp
  src/semantics/SemanticsValidatorTraits.cpp
  src/text_filter/TextFilterHelpers.cpp
  src/text_filter/TextFilterUnaryRewrites.cpp
  src/text_filter/TextFilterPipelineEnvelope.cpp
  src/text_filter/TextFilterPipelineEnvelopeHelpers.cpp
  src/text_filter/TextFilterPipelineOperatorRewrite.cpp
  src/text_filter/TextFilterPipelinePass.cpp
  src/text_filter/TextFilterPipelineScan.cpp
  src/text_filter/TextFilterPipeline.cpp
)

set(PRIMESTRUCT_IR_SOURCES
  src/CliDriver.cpp
  src/CompilePipeline.cpp
  src/IrBackendProfiles.cpp
  src/IrInliner.cpp
  src/IrPrinterHelpers.cpp
  src/IrPreparation.cpp
  src/IrPrinter.cpp
  src/IrSerializer.cpp
  src/IrValidation.cpp
  src/IrVirtualRegisterAllocator.cpp
  src/IrVirtualRegisterLiveness.cpp
  src/IrVirtualRegisterLowering.cpp
  src/IrVirtualRegisterScheduler.cpp
  src/IrVirtualRegisterSpillInsertion.cpp
  src/IrVirtualRegisterVerifier.cpp
  src/ir_lowerer/IrLowererCallHelpers.cpp
  src/ir_lowerer/IrLowererSemanticProductTargetAdapters.cpp
  src/ir_lowerer/IrLowererAccessTargetResolution.cpp
  src/ir_lowerer/IrLowererAccessLoadHelpers.cpp
  src/ir_lowerer/IrLowererIndexedAccessEmit.cpp
  src/ir_lowerer/IrLowererCallResolution.cpp
  src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp
  src/ir_lowerer/IrLowererNativeTailDispatch.cpp
  src/ir_lowerer/IrLowererBindingTransformHelpers.cpp
  src/ir_lowerer/IrLowererBindingTypeHelpers.cpp
  src/ir_lowerer/IrLowererCountAccessClassifiers.cpp
  src/ir_lowerer/IrLowererCountAccessHelpers.cpp
  src/ir_lowerer/IrLowererFileWriteHelpers.cpp
  src/ir_lowerer/IrLowererFlowBufferHelpers.cpp
  src/ir_lowerer/IrLowererFlowControlHelpers.cpp
  src/ir_lowerer/IrLowererFlowVectorHelpers.cpp
  src/ir_lowerer/IrLowererFlowVectorResolutionHelpers.cpp
  src/ir_lowerer/IrLowererFlowHelpers.cpp
  src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp
  src/ir_lowerer/IrLowererHelpers.cpp
  src/ir_lowerer/IrLowererIndexKindHelpers.cpp
  src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp
  src/ir_lowerer/IrLowererInlinePackedArgs.cpp
  src/ir_lowerer/IrLowererInlineParamHelpers.cpp
  src/ir_lowerer/IrLowererInlineStructArgHelpers.cpp
  src/ir_lowerer/IrLowererLower.cpp
  src/ir_lowerer/IrLowererLowerEffects.cpp
  src/ir_lowerer/IrLowererLowerEntrySetup.cpp
  src/ir_lowerer/IrLowererLowerExprEmitSetup.cpp
  src/ir_lowerer/IrLowererLowerImportsStructsSetup.cpp
  src/ir_lowerer/IrLowererLowerInferenceArrayKindSetup.cpp
  src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp
  src/ir_lowerer/IrLowererLowerInferenceCallReturnSetup.cpp
  src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp
  src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp
  src/ir_lowerer/IrLowererLowerInferenceReturnInfoHelpers.cpp
  src/ir_lowerer/IrLowererLowerInferenceSetup.cpp
  src/ir_lowerer/IrLowererLowerInlineCallActiveContextStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallCleanupStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallContextSetupStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallGpuLocalsStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallReturnValueStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallStatementStep.cpp
  src/ir_lowerer/IrLowererLowerLocalsSetup.cpp
  src/ir_lowerer/IrLowererLowerReturnCallsSetup.cpp
  src/ir_lowerer/IrLowererLowerStatementsCallsStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsEntryExecutionStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsEntryStatementStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsFunctionTableStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsSourceMapStep.cpp
  src/ir_lowerer/IrLowererOnErrorHelpers.cpp
  src/ir_lowerer/IrLowererOperatorArithmeticHelpers.cpp
  src/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.cpp
  src/ir_lowerer/IrLowererOperatorAngleTrigHelpers.cpp
  src/ir_lowerer/IrLowererOperatorExpLogHyperbolicHelpers.cpp
  src/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.cpp
  src/ir_lowerer/IrLowererOperatorComparisonHelpers.cpp
  src/ir_lowerer/IrLowererOperatorConversionsAndCallsControlHelpers.cpp
  src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp
  src/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.cpp
  src/ir_lowerer/IrLowererOperatorMemoryPointerHelpers.cpp
  src/ir_lowerer/IrLowererOperatorPowAbsSignHelpers.cpp
  src/ir_lowerer/IrLowererOperatorSaturateRoundingRootsHelpers.cpp
  src/ir_lowerer/IrLowererReturnInferenceHelpers.cpp
  src/ir_lowerer/IrLowererResultMetadataHelpers.cpp
  src/ir_lowerer/IrLowererResultHelpers.cpp
  src/ir_lowerer/IrLowererPackedResultHelpers.cpp
  src/ir_lowerer/IrLowererResultWhyHelpers.cpp
  src/ir_lowerer/IrLowererRuntimeErrorHelpers.cpp
  src/ir_lowerer/IrLowererSetupInferenceHelpers.cpp
  src/ir_lowerer/IrLowererSetupInferenceReturnKinds.cpp
  src/ir_lowerer/IrLowererSetupLocalsHelpers.cpp
  src/ir_lowerer/IrLowererSetupMathHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp
  src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp
  src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeReceiverTargetHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeReturnKindHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeStructPathHelpers.cpp
  src/ir_lowerer/IrLowererStatementBindingStatementEmit.cpp
  src/ir_lowerer/IrLowererStatementBindingHelpers.cpp
  src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp
  src/ir_lowerer/IrLowererStatementCallEmission.cpp
  src/ir_lowerer/IrLowererStatementCallHelpers.cpp
  src/ir_lowerer/IrLowererStringCallHelpers.cpp
  src/ir_lowerer/IrLowererStringLiteralHelpers.cpp
  src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp
  src/ir_lowerer/IrLowererStructLayoutHelpers.cpp
  src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp
  src/ir_lowerer/IrLowererStructReturnPathHelpers.cpp
  src/ir_lowerer/IrLowererStructTypeHelpers.cpp
  src/ir_lowerer/IrLowererTemplateTypeParseHelpers.cpp
  src/ir_lowerer/IrLowererUninitializedSetupBuilders.cpp
  src/ir_lowerer/IrLowererUninitializedStructInference.cpp
  src/ir_lowerer/IrLowererUninitializedTypeHelpers.cpp
)

set(PRIMESTRUCT_CODEGEN_SOURCES
  src/ExternalTooling.cpp
  src/emitter/EmitterEmit.cpp
  src/emitter/EmitterEmitSetupLifecycleHelperStep.cpp
  src/emitter/EmitterEmitSetupMathImport.cpp
  src/emitter/EmitterExpr.cpp
  src/emitter/EmitterExprControlBuiltinBlockEarlyReturnStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockBindingAutoStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockBindingExplicitStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockBindingFallbackStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockBindingPreludeStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockBindingQualifiersStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockStatementStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockFinalValueStep.cpp
  src/emitter/EmitterExprControlBoolLiteralStep.cpp
  src/emitter/EmitterExprControlBuiltinBlockPreludeStep.cpp
  src/emitter/EmitterExprControlBodyWrapperStep.cpp
  src/emitter/EmitterExprControlCallPathStep.cpp
  src/emitter/EmitterExprControlCountRewriteStep.cpp
  src/emitter/EmitterExprControlFieldAccessStep.cpp
  src/emitter/EmitterExprControlFloatLiteralStep.cpp
  src/emitter/EmitterExprControlIfBlockBindingAutoStep.cpp
  src/emitter/EmitterExprControlIfBlockBindingExplicitStep.cpp
  src/emitter/EmitterExprControlIfBlockBindingFallbackStep.cpp
  src/emitter/EmitterExprControlIfBranchBodyBindingStep.cpp
  src/emitter/EmitterExprControlIfBranchBodyDispatchStep.cpp
  src/emitter/EmitterExprControlIfBranchBodyHandlersStep.cpp
  src/emitter/EmitterExprControlIfBranchBodyStep.cpp
  src/emitter/EmitterExprControlIfBranchBodyReturnStep.cpp
  src/emitter/EmitterExprControlIfBranchBodyStatementStep.cpp
  src/emitter/EmitterExprControlIfBranchEmitStep.cpp
  src/emitter/EmitterExprControlIfBranchValueStep.cpp
  src/emitter/EmitterExprControlIfBranchPreludeStep.cpp
  src/emitter/EmitterExprControlIfBranchWrapperStep.cpp
  src/emitter/EmitterExprControlIfBlockBindingPreludeStep.cpp
  src/emitter/EmitterExprControlIfBlockBindingQualifiersStep.cpp
  src/emitter/EmitterExprControlIfBlockStatementStep.cpp
  src/emitter/EmitterExprControlIfBlockFinalValueStep.cpp
  src/emitter/EmitterExprControlIfBlockEarlyReturnStep.cpp
  src/emitter/EmitterExprControlIfTernaryFallbackStep.cpp
  src/emitter/EmitterExprControlIfTernaryStep.cpp
  src/emitter/EmitterExprControlIfEnvelopeStep.cpp
  src/emitter/EmitterExprControlIntegerLiteralStep.cpp
  src/emitter/EmitterExprControlMethodPathStep.cpp
  src/emitter/EmitterExprControlStringLiteralStep.cpp
  src/emitter/EmitterExprControlNameStep.cpp
  src/emitter/EmitterBuiltinCallPathHelpers.cpp
  src/emitter/EmitterBuiltinCollectionInferenceHelpers.cpp
  src/emitter/EmitterBuiltinMethodResolutionHelpers.cpp
  src/emitter/EmitterBuiltinMethodResolutionMetadataHelpers.cpp
  src/emitter/EmitterBuiltinMethodResolutionTypeInferenceHelpers.cpp
  src/emitter/EmitterHelpersBuiltins.cpp
  src/emitter/EmitterHelpersStructTypes.cpp
  src/emitter/EmitterHelpersTypes.cpp
  src/glsl_emitter/GlslEmitter.cpp
  src/glsl_emitter/GlslEmitterExpr.cpp
  src/glsl_emitter/GlslEmitterExprBuiltins.cpp
  src/glsl_emitter/GlslEmitterExprMathBuiltins.cpp
  src/glsl_emitter/GlslEmitterHelpers.cpp
  src/glsl_emitter/GlslEmitterStatements.cpp
  src/IrToCppEmitter.cpp
  src/IrToCppEmitterInstructionEmitter.cpp
  src/IrToCppEmitterPrintAndFileEmitter.cpp
  src/IrToGlslEmitter.cpp
  src/IrToGlslEmitterFunctionEmitter.cpp
  src/native_emitter/NativeEmitterHelpers.cpp
  src/native_emitter/NativeEmitterMachO.cpp
  src/native_emitter/NativeEmitterEmit.cpp
  src/native_emitter/NativeEmitterFunctionEmit.cpp
  src/wasm_emitter/WasmEmitter.cpp
  src/wasm_emitter/WasmEmitterControlFlow.cpp
  src/wasm_emitter/WasmEmitterFunctionBodies.cpp
  src/wasm_emitter/WasmEmitterIo.cpp
  src/wasm_emitter/WasmEmitterModule.cpp
  src/wasm_emitter/WasmEmitterRuntimeContext.cpp
  src/wasm_emitter/WasmEmitterSections.cpp
  src/wasm_emitter/WasmEmitterWasiHelpers.cpp
)

set(PRIMESTRUCT_RUNTIME_SOURCES
  src/Vm.cpp
  src/VmDebugHelpers.cpp
  src/VmDebugSession.cpp
  src/VmDebugSessionInstruction.cpp
  src/VmDebugSessionInstructionNumeric.cpp
  src/VmDebugSessionSetup.cpp
  src/VmExecution.cpp
  src/VmExecutionNumeric.cpp
  src/VmHeapHelpers.cpp
  src/VmIoHelpers.cpp
  src/VmDebugAdapter.cpp
  src/VmDebugDap.cpp
  src/VmDebugDapProtocol.cpp
)

set(PRIMESTRUCT_BACKEND_REGISTRY_SOURCES
  src/IrBackends.cpp
)

add_library(primec_support_lib ${PRIMESTRUCT_SUPPORT_SOURCES})
target_include_directories(primec_support_lib PUBLIC include)
primestructEnableWarnings(primec_support_lib)

add_library(primec_frontend_lib ${PRIMESTRUCT_FRONTEND_SOURCES})
target_include_directories(primec_frontend_lib PUBLIC include)
target_link_libraries(primec_frontend_lib PUBLIC primec_support_lib)
primestructEnableWarnings(primec_frontend_lib)

add_library(primec_runtime_lib ${PRIMESTRUCT_RUNTIME_SOURCES})
target_include_directories(primec_runtime_lib PUBLIC include)
target_link_libraries(primec_runtime_lib PUBLIC primec_frontend_lib primec_support_lib)
primestructEnableWarnings(primec_runtime_lib)

add_library(primec_backend_emitters_lib ${PRIMESTRUCT_CODEGEN_SOURCES})
target_include_directories(primec_backend_emitters_lib PUBLIC include)
target_link_libraries(primec_backend_emitters_lib PUBLIC primec_frontend_lib primec_support_lib)
primestructEnableWarnings(primec_backend_emitters_lib)

add_library(primec_codegen_lib INTERFACE)
target_include_directories(primec_codegen_lib INTERFACE include)
target_link_libraries(primec_codegen_lib INTERFACE primec_backend_emitters_lib)

add_library(primec_ir_lib ${PRIMESTRUCT_IR_SOURCES})
target_include_directories(primec_ir_lib PUBLIC include)
target_link_libraries(primec_ir_lib PUBLIC primec_frontend_lib primec_support_lib)
primestructEnableWarnings(primec_ir_lib)

add_library(primec_backend_registry_lib ${PRIMESTRUCT_BACKEND_REGISTRY_SOURCES})
target_include_directories(primec_backend_registry_lib PUBLIC include)
target_link_libraries(primec_backend_registry_lib PRIVATE primec_backend_emitters_lib primec_runtime_lib)
primestructEnableWarnings(primec_backend_registry_lib)

add_library(primec_backend_lib INTERFACE)
target_include_directories(primec_backend_lib INTERFACE include)
target_link_libraries(primec_backend_lib INTERFACE primec_ir_lib primec_backend_registry_lib)

add_library(primec_lib INTERFACE)
target_include_directories(primec_lib INTERFACE include)
target_link_libraries(primec_lib INTERFACE primec_backend_lib primec_frontend_lib primec_support_lib)

add_executable(primec src/main.cpp)
target_link_libraries(primec PRIVATE primec_ir_lib primec_backend_registry_lib)
primestructEnableWarnings(primec)

add_executable(primevm src/primevm_main.cpp)
target_link_libraries(primevm PRIVATE primec_ir_lib primec_runtime_lib)
primestructEnableWarnings(primevm)

if(PRIMESTRUCT_BUILD_TESTS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  enable_testing()

  add_executable(PrimeStruct_misc_tests
    tests/unit/test_main.cpp
    tests/unit/test_import_resolver.cpp
    tests/unit/test_import_resolver_versions.cpp
    tests/unit/test_import_errors.cpp
    tests/unit/test_import_multiple.cpp
    tests/unit/test_ast_ir_dump.cpp
    tests/unit/test_ast_ir_dump_inference.cpp
    tests/unit/test_ast_ir_dump_namespace.cpp
    tests/unit/test_printers_manual.cpp
    tests/unit/test_semantics_manual.cpp
    tests/unit/test_lexer.cpp
    tests/unit/test_diagnostics_codes.cpp
  )
  target_include_directories(PrimeStruct_misc_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_misc_tests PRIVATE primec_ir_lib)
  primestructEnableWarnings(PrimeStruct_misc_tests)

  add_executable(PrimeStruct_backend_tests
    tests/unit/test_main.cpp
    tests/unit/test_ir_pipeline.cpp
    tests/unit/test_ir_pipeline_serialization_calls.cpp
    tests/unit/test_ir_pipeline_serialization_control_flow.cpp
    tests/unit/test_ir_pipeline_serialization_structs.cpp
    tests/unit/test_ir_pipeline_validation.cpp
    tests/unit/test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_entry_setup_step_resolves_entry_metadata.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inference_expr_kind_call_base_setup_infers_try_from_indexed_borrowed_and_po.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inference_call_return_setup_resolves_namespaced_capacity_definition_directl.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inference_call_return_setup_resolves_canonical_namespaced_map_access_direct.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inference_get_return_info_step_reports_missing_definitions.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inference_expr_kind_dispatch_infers_try_from_indexed_map_tryat_args_pack_lo.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statements_calls_step_validates_dependencies.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_call_return_value_step_emits_expected_instructions.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_expr_emit_setup_dispatches_upload_readback_passthrough_step.cpp
    tests/unit/test_ir_pipeline_validation_emitter_expr_control_count_rewrite_step_only_rewrites_bare_collection_calls.cpp
    tests/unit/test_ir_pipeline_validation_emitter_expr_control_builtin_block_final_value_step_handles_final_statements.cpp
    tests/unit/test_ir_pipeline_validation_emitter_expr_control_if_block_final_value_step_emits_final_returns.cpp
    tests/unit/test_ir_pipeline_validation_emitter_expr_control_if_branch_body_return_step_dispatches_returns.cpp
    tests/unit/test_ir_pipeline_validation_emitter_expr_control_if_branch_emit_step_composes_value_and_handlers.cpp
    tests/unit/test_ir_pipeline_validation_emitter_expr_source_delegation_stays_stable.cpp
    tests/unit/test_ir_pipeline_validation_semantics_validate_source_delegation_stays_stable.cpp
    tests/unit/test_ir_pipeline_validation_semantics_validator_infer_source_delegation_stays_stable.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_build_bundled_entry_call_setup.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_field_binding_helpers_resolve_layout_bindings.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_return_helpers_reject_canonical_fallback_from_alias_defs_without_ret.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_return_helpers_keep_map_tryat_alias_precedence.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_layout_helpers_compute_uncached_diagnostics.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_inline_call_with_count_fallbacks.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_inline_calls_with_locals.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_keep_explicit_map_helpers_out_of_native_builtin_emission.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_native_call_tail_orchestration.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_buffer_and_native_tail_wrappers.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_try_emit_map_access_lookup.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_emit_builtin_array_access.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_emit_map_lookup_loop_search_scaffold.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_on_error_helpers_wire_definition_handlers.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_build_bundled_program_entry_return_runtime_and_s.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_method_definitions_from_receiver_targets.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_indexed_args_pack_pointer_map_receivers.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_allows_count_capacity_receiver_probing.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_indexed_args_pack_struct_receiver_methods_from_e.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_keeps_reject_diagnostics_for_canonical_map_helper_receive.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_rejects_wrapper_string_slash_method_access_primitive_fall.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_keeps_builtin_array_count_fallback_and_rejects_bare_vecto.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_direct_definition_call_return_kinds_via_removed.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_skips_explicit_vector_helper_call_routing.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_rejects_canonical_map_access_fallback_to_compatibility_de.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_builtin_like_count_call_methods.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_struct_arg_helper_reports_diagnostics.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_aliases_pure_fileerror_variadic_forwarding.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_rejects_borrowed_fileerror_variadic_alias_type_mismatch.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_borrowed_result_variadic_args_packs.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_rejects_variadic_reference_packs_without_location_forwa.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_borrowed_array_variadic_args_packs.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_pointer_array_variadic_args_packs.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_pointer_soa_vector_variadic_args_packs.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_materializes_borrowed_vector_variadic_args_packs.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_rejects_borrowed_vector_variadic_alias_type_mismatch.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_inline_param_helper_aliases_pure_map_variadic_forwarding.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_type_helpers_infer_struct_return_paths.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_type_helpers_resolve_struct_slot_layouts_from_definition_fields.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_struct_type_helpers_skip_unsupported_struct_value_paths.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_infer_call_target_struct_paths_with_visited_fiel.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_resolve_field_storage_access.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_build_storage_resolver_from_definition_field_ind.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_count_access_helpers_build_bundled_entry_count_setup.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_count_access_helpers_emit_count_access_calls.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_runtime_error_helpers_emit_fileerror_why_call_paths.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_rejects_invalid_pointer_targets.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_resolves_reordered_named_access_kinds.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_handles_non_math_and_invalid_math_calls.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_setup_inference_helper_handles_invalid_comparison_operator_calls.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_classifies_variadic_fileerror_parameters.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_classifies_variadic_struct_reference_parameters.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_emits_runtime_string_call_bindings.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_binding_helper_validates_print_statement_builtin_diagnostics.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_buffer_store_diagnostics.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_direct_call_diagnostics.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_function_table_diagnostics.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_pow_helper_rejects_mixed_signed_unsigned_operands.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_conversions_helper_rejects_immutable_assign_target.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_result_helpers_try_emit_result_ok_method_calls.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_result_helpers_resolve_file_handle_result_payload_metadata.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_result_helpers_resolve_final_if_result_and_then_metadata.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_result_helpers_resolve_indexed_dereferenced_args_pack_result_references.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_result_helpers_try_emit_result_why_method_calls.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_result_helpers_emit_result_why_with_composed_ops.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_flow_helpers_emit_counted_loop_scaffolding.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_flow_helpers_emit_vector_statement_helpers_through_variadic_vector_receiver.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_flow_helpers_skip_user_defined_vector_helper_names.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_flow_helpers_resolve_buffer_load_info.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_file_write_helpers_resolve_write_opcodes.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_file_write_helpers_emit_write_bytes_loops.cpp
    tests/unit/test_ir_pipeline_validation_ir_lowerer_string_call_helpers_handle_call_expression_paths.cpp
    tests/unit/test_ir_pipeline_validation_ir_validator_wasm_target_accepts_heap_alloc_effects_and_capabilities.cpp
    tests/unit/test_ir_pipeline_conversions.cpp
    tests/unit/test_ir_pipeline_conversions_method_calls_and_argv.cpp
    tests/unit/test_ir_pipeline_conversions_numbers.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_basics.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_results.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_file_errors.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_file_handles.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_borrowed_vectors.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_collection_refs.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_pointer_refs.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_field_refs_and_maps.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_pointer_maps.cpp
    tests/unit/test_ir_pipeline_conversions_variadic_pointer_vectors.cpp
    tests/unit/test_ir_pipeline_entry_args.cpp
    tests/unit/test_ir_pipeline_gpu.cpp
    tests/unit/test_ir_pipeline_pointers.cpp
    tests/unit/test_ir_pipeline_type_resolution_parity.cpp
    tests/unit/test_ir_pipeline_backends.cpp
    tests/unit/test_ir_pipeline_backends_architecture.cpp
    tests/unit/test_ir_pipeline_backends_cpp_vm.cpp
    tests/unit/test_ir_pipeline_backends_graph_contexts.cpp
    tests/unit/test_ir_pipeline_backends_graph_pilot.cpp
    tests/unit/test_ir_pipeline_backends_graph_utilities.cpp
    tests/unit/test_ir_pipeline_backends_glsl.cpp
    tests/unit/test_ir_pipeline_backends_registry.cpp
    tests/unit/test_vm_debug_session.cpp
    tests/unit/test_ir_pipeline_external_tooling.cpp
    tests/unit/test_ir_pipeline_to_cpp.cpp
    tests/unit/test_ir_pipeline_to_glsl.cpp
    tests/unit/test_ir_pipeline_to_glsl_runtime.cpp
    tests/unit/test_ir_pipeline_wasm.cpp
    tests/unit/test_ir_pipeline_wasm_wasi.cpp
    tests/unit/test_compile_run_benchmark_harness.cpp
    tests/unit/test_compile_run_bindings_and_examples.cpp
    tests/unit/test_compile_run_bindings_basic.cpp
    tests/unit/test_compile_run_examples_browser_smoke.cpp
    tests/unit/test_compile_run_examples_demo_script_args.cpp
    tests/unit/test_compile_run_examples_demo_script_core.cpp
    tests/unit/test_compile_run_examples_docs.cpp
    tests/unit/test_compile_run_examples_docs_commands.cpp
    tests/unit/test_compile_run_examples_docs_locks.cpp
    tests/unit/test_compile_run_examples_metal_and_browser_hosts.cpp
    tests/unit/test_compile_run_examples_metal_smoke_and_borrows.cpp
    tests/unit/test_compile_run_examples_native_launcher.cpp
    tests/unit/test_compile_run_examples_native_visual.cpp
    tests/unit/test_compile_run_examples_spinning_cube_native.cpp
    tests/unit/test_compile_run_examples_spinning_cube_runtime.cpp
    tests/unit/test_compile_run_emitters.cpp
    tests/unit/test_compile_run_emitters_core_behaviors.cpp
    tests/unit/test_compile_run_emitters_lambda_mutator_resolution.cpp
    tests/unit/test_compile_run_emitters_namespaced_vector_push_and_count_helpers.cpp
    tests/unit/test_compile_run_emitters_map_access_and_collection_rewrites.cpp
    tests/unit/test_compile_run_emitters_vector_access_metadata_resolution.cpp
    tests/unit/test_compile_run_emitters_map_metadata_resolution.cpp
    tests/unit/test_compile_run_emitters_vector_receiver_metadata_resolution.cpp
    tests/unit/test_compile_run_emitters_vector_alias_template_forwarding.cpp
    tests/unit/test_compile_run_emitters_namespaced_vector_map_helper_resolution.cpp
    tests/unit/test_compile_run_emitters_canonical_map_helper_calls.cpp
    tests/unit/test_compile_run_emitters_wrapper_map_count_sugar.cpp
    tests/unit/test_compile_run_emitters_map_count_and_wrapper_capacity.cpp
    tests/unit/test_compile_run_emitters_local_vector_count_receiver_resolution.cpp
    tests/unit/test_compile_run_emitters_string_receiver_vector_access.cpp
    tests/unit/test_compile_run_emitters_compatibility_chain_forwarding_rejections.cpp
    tests/unit/test_compile_run_emitters_canonical_map_method_forwarding.cpp
    tests/unit/test_compile_run_emitters_vector_capacity_receiver_resolution.cpp
    tests/unit/test_compile_run_emitters_explicit_vector_count_capacity_helpers.cpp
    tests/unit/test_compile_run_emitters_vector_access_slash_alias_helpers.cpp
    tests/unit/test_compile_run_emitters_explicit_vector_mutator_statement_helpers.cpp
    tests/unit/test_compile_run_emitters_wrapper_map_count_and_string_fallback.cpp
    tests/unit/test_compile_run_emitters_wrapper_direct_call_receiver_fallbacks.cpp
    tests/unit/test_compile_run_emitters_matrix_quaternion_support.cpp
    tests/unit/test_compile_run_emitters_loop_sugar_runtime.cpp
    tests/unit/test_compile_run_emitters_variadic_file_packs.cpp
    tests/unit/test_compile_run_emitters_variadic_pointer_pack_access.cpp
    tests/unit/test_compile_run_emitters_variadic_reference_pack_access.cpp
    tests/unit/test_compile_run_glsl.cpp
    tests/unit/test_compile_run_imports_versions.cpp
    tests/unit/test_compile_run_imports_blocks.cpp
    tests/unit/test_compile_run_imports_operations.cpp
    tests/unit/test_compile_run_native_backend_argv.cpp
    tests/unit/test_compile_run_native_backend_core.cpp
    tests/unit/test_compile_run_native_backend_core_array_and_pointer_variadics.cpp
    tests/unit/test_compile_run_native_backend_core_buffer_and_collection_wrappers.cpp
    tests/unit/test_compile_run_native_backend_core_error_and_file_variadics.cpp
    tests/unit/test_compile_run_native_backend_core_file_and_struct_variadics.cpp
    tests/unit/test_compile_run_native_backend_core_image_io_a.cpp
    tests/unit/test_compile_run_native_backend_core_image_io_b.cpp
    tests/unit/test_compile_run_native_backend_core_image_io_c.cpp
    tests/unit/test_compile_run_native_backend_core_image_io_d.cpp
    tests/unit/test_compile_run_native_backend_core_map_and_vector_variadics.cpp
    tests/unit/test_compile_run_native_backend_core_reference_and_uninitialized_variadics.cpp
    tests/unit/test_compile_run_native_backend_core_result_and_vector_variadics.cpp
    tests/unit/test_compile_run_native_backend_core_result_payloads_and_strings.cpp
    tests/unit/test_compile_run_native_backend_core_runtime_and_ir_paths.cpp
    tests/unit/test_compile_run_native_backend_core_ui_layout_a.cpp
    tests/unit/test_compile_run_native_backend_core_ui_layout_b.cpp
    tests/unit/test_compile_run_native_backend_core_vector_and_experimental_map_variadics.cpp
    tests/unit/test_compile_run_native_backend_collections.cpp
    tests/unit/test_compile_run_native_backend_collections_alias_diagnostics_a.cpp
    tests/unit/test_compile_run_native_backend_collections_alias_diagnostics_b.cpp
    tests/unit/test_compile_run_native_backend_collections_arrays_and_aliases.cpp
    tests/unit/test_compile_run_native_backend_collections_auto_inferred_helper_precedence.cpp
    tests/unit/test_compile_run_native_backend_collections_canonical_count_shadows.cpp
    tests/unit/test_compile_run_native_backend_collections_canonical_map_helpers.cpp
    tests/unit/test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp
    tests/unit/test_compile_run_native_backend_collections_map_literals_and_string_keys.cpp
    tests/unit/test_compile_run_native_backend_collections_mutators_and_limits_a.cpp
    tests/unit/test_compile_run_native_backend_collections_mutators_and_limits_b.cpp
    tests/unit/test_compile_run_native_backend_collections_shadow_precedence_and_counts.cpp
    tests/unit/test_compile_run_native_backend_collections_shims_maps_a.cpp
    tests/unit/test_compile_run_native_backend_collections_shims_maps_b.cpp
    tests/unit/test_compile_run_native_backend_collections_shims_maps_c.cpp
    tests/unit/test_compile_run_native_backend_collections_shims_vectors.cpp
    tests/unit/test_compile_run_native_backend_collections_user_shadow_methods_a.cpp
    tests/unit/test_compile_run_native_backend_collections_user_shadow_methods_b.cpp
    tests/unit/test_compile_run_native_backend_collections_vector_alias_rejects_a.cpp
    tests/unit/test_compile_run_native_backend_collections_vector_alias_rejects_b.cpp
    tests/unit/test_compile_run_native_backend_collections_wrapper_rejects_maps.cpp
    tests/unit/test_compile_run_native_backend_collections_wrapper_rejects_vectors.cpp
    tests/unit/test_compile_run_native_backend_collections_wrapper_shadow_precedence.cpp
    tests/unit/test_compile_run_native_backend_control.cpp
    tests/unit/test_compile_run_native_backend_imports.cpp
    tests/unit/test_compile_run_native_backend_math_numeric.cpp
    tests/unit/test_compile_run_native_backend_maybe.cpp
    tests/unit/test_compile_run_native_backend_pointers.cpp
    tests/unit/test_compile_run_native_backend_uninitialized.cpp
    tests/unit/test_compile_run_reflection_codegen.cpp
    tests/unit/test_compile_run_reflection_codegen_runtime.cpp
    tests/unit/test_compile_run_smoke.cpp
    tests/unit/test_compile_run_smoke_argv.cpp
    tests/unit/test_compile_run_smoke_collective.cpp
    tests/unit/test_compile_run_smoke_core_basic.cpp
    tests/unit/test_compile_run_smoke_core_contracts_and_cli.cpp
    tests/unit/test_compile_run_smoke_core_debug_and_docs.cpp
    tests/unit/test_compile_run_smoke_core_demo_scripts.cpp
    tests/unit/test_compile_run_smoke_core_gfx_end_to_end.cpp
    tests/unit/test_compile_run_smoke_core_gfx_entrypoints.cpp
    tests/unit/test_compile_run_smoke_core_gfx_imports.cpp
    tests/unit/test_compile_run_smoke_core_wasm_core.cpp
    tests/unit/test_compile_run_smoke_core_wasm_limits.cpp
    tests/unit/test_compile_run_smoke_core_wasm_profiles.cpp
    tests/unit/test_compile_run_smoke_core_wasm_wasi_core.cpp
    tests/unit/test_compile_run_smoke_core_wasm_wasi_png_decode.cpp
    tests/unit/test_compile_run_smoke_core_wasm_wasi_png_interlaced.cpp
    tests/unit/test_compile_run_smoke_core_wasm_wasi_png_write.cpp
    tests/unit/test_compile_run_text_filters.cpp
    tests/unit/test_compile_run_text_filters_core_lists.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_a.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_b.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_c.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_d.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_e.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_f.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_g.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_h.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_i.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_j.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_k.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_l.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_m.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_n.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_o.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_p.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_q.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_r.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_s.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_t.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_u.cpp
    tests/unit/test_compile_run_text_filters_diagnostics_v.cpp
    tests/unit/test_compile_run_text_filters_dumps.cpp
    tests/unit/test_compile_run_text_filters_misc.cpp
    tests/unit/test_compile_run_text_filters_runtime_if.cpp
    tests/unit/test_compile_run_text_filters_semantic_rules.cpp
    tests/unit/test_compile_run_text_filters_text_rules.cpp
    tests/unit/test_compile_run_vm_bounds.cpp
    tests/unit/test_compile_run_vm_core.cpp
    tests/unit/test_compile_run_vm_core_basic.cpp
    tests/unit/test_compile_run_vm_core_ui.cpp
    tests/unit/test_compile_run_vm_core_runtime.cpp
    tests/unit/test_compile_run_vm_core_results_basic.cpp
    tests/unit/test_compile_run_vm_core_results_structs.cpp
    tests/unit/test_compile_run_vm_core_file_helpers.cpp
    tests/unit/test_compile_run_vm_core_gfx_helpers.cpp
    tests/unit/test_compile_run_vm_core_variadics.cpp
    tests/unit/test_compile_run_vm_collections.cpp
    tests/unit/test_compile_run_vm_collections_array_and_wrapper_shadows.cpp
    tests/unit/test_compile_run_vm_collections_core_aliases.cpp
    tests/unit/test_compile_run_vm_collections_map_vector_shadows.cpp
    tests/unit/test_compile_run_vm_collections_map_wrapper_shadows.cpp
    tests/unit/test_compile_run_vm_collections_method_aliases_a.cpp
    tests/unit/test_compile_run_vm_collections_method_aliases_b.cpp
    tests/unit/test_compile_run_vm_collections_shim_maps_a.cpp
    tests/unit/test_compile_run_vm_collections_shim_maps_b.cpp
    tests/unit/test_compile_run_vm_collections_shim_maps_c.cpp
    tests/unit/test_compile_run_vm_collections_vector_aliases_a.cpp
    tests/unit/test_compile_run_vm_collections_vector_aliases_b.cpp
    tests/unit/test_compile_run_vm_collections_vector_limits_a.cpp
    tests/unit/test_compile_run_vm_collections_vector_limits_b.cpp
    tests/unit/test_compile_run_vm_collections_vector_shadow_access.cpp
    tests/unit/test_compile_run_vm_collections_wrapper_temporaries_a.cpp
    tests/unit/test_compile_run_vm_collections_wrapper_temporaries_b.cpp
    tests/unit/test_compile_run_vm_collections_wrapper_temporaries_c.cpp
    tests/unit/test_compile_run_vm_gpu.cpp
    tests/unit/test_compile_run_vm_maps.cpp
    tests/unit/test_compile_run_vm_math.cpp
    tests/unit/test_compile_run_math_conformance.cpp
    tests/unit/test_compile_run_vm_maybe.cpp
    tests/unit/test_compile_run_vm_outputs.cpp
    tests/unit/test_compile_run_vm_uninitialized.cpp
  )
  target_include_directories(PrimeStruct_backend_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_backend_tests PRIVATE primec_ir_lib primec_backend_registry_lib)
  primestructEnableWarnings(PrimeStruct_backend_tests)

  add_executable(PrimeStruct_semantics_tests
    tests/unit/test_main.cpp
    tests/unit/test_semantics.cpp
    tests/unit/test_semantics_capabilities.cpp
    tests/unit/test_semantics_capabilities_builtins.cpp
    tests/unit/test_semantics_capabilities_lifecycle.cpp
    tests/unit/test_semantics_capabilities_structs_core.cpp
    tests/unit/test_semantics_capabilities_structs_metadata.cpp
    tests/unit/test_semantics_capabilities_structs_reflect_clear.cpp
    tests/unit/test_semantics_capabilities_structs_reflect_compare.cpp
    tests/unit/test_semantics_capabilities_structs_reflect_default.cpp
    tests/unit/test_semantics_capabilities_structs_reflect_equal.cpp
    tests/unit/test_semantics_capabilities_structs_reflect_serde.cpp
    tests/unit/test_semantics_capabilities_structs_reflect_validate.cpp
    tests/unit/test_semantics_bindings_assignments.cpp
    tests/unit/test_semantics_bindings_core.cpp
    tests/unit/test_semantics_bindings_core_transforms.cpp
    tests/unit/test_semantics_bindings_control_flow.cpp
    tests/unit/test_semantics_bindings_control_flow_borrowing.cpp
    tests/unit/test_semantics_bindings_pointers.cpp
    tests/unit/test_semantics_bindings_struct_defaults.cpp
    tests/unit/test_semantics_calls_and_flow_access.cpp
    tests/unit/test_semantics_calls_and_flow_collections.cpp
    tests/unit/test_semantics_calls_and_flow_collections_count_helpers_and_bare_map_calls.cpp
    tests/unit/test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp
    tests/unit/test_semantics_calls_and_flow_collections_soa_vector_builtins_named_args.cpp
    tests/unit/test_semantics_calls_and_flow_collections_wrapper_temporary_access_resolution.cpp
    tests/unit/test_semantics_calls_and_flow_collections_user_array_helper_precedence.cpp
    tests/unit/test_semantics_calls_and_flow_collections_relocation_trivial_vector_elements.cpp
    tests/unit/test_semantics_calls_and_flow_collections_bare_vector_pop_helper_resolution.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_mutator_named_args.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_helper_reordered_expression.cpp
    tests/unit/test_semantics_calls_and_flow_collections_stdlib_map_constructor_resolution.cpp
    tests/unit/test_semantics_calls_and_flow_collections_experimental_map_result_payloads.cpp
    tests/unit/test_semantics_calls_and_flow_collections_experimental_map_deref_and_struct_storage.cpp
    tests/unit/test_semantics_calls_and_flow_collections_experimental_map_auto_inference.cpp
    tests/unit/test_semantics_calls_and_flow_collections_stdlib_map_constructor_parameter_shapes.cpp
    tests/unit/test_semantics_calls_and_flow_collections_field_bound_map_compatibility_calls.cpp
    tests/unit/test_semantics_calls_and_flow_collections_wrapper_temporary_tryat_contains_inference.cpp
    tests/unit/test_semantics_calls_and_flow_collections_variadic_fileerror_and_file_handles.cpp
    tests/unit/test_semantics_calls_and_flow_collections_wrapper_returned_map_method_resolution.cpp
    tests/unit/test_semantics_calls_and_flow_collections_namespaced_collection_statement_body_args.cpp
    tests/unit/test_semantics_calls_and_flow_collections_bare_map_call_form_statement_args.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_method_alias_struct_diagnostics.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_namespaced_alias_calls.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_alias_unknown_expected_forwarding.cpp
    tests/unit/test_semantics_calls_and_flow_collections_wrapper_temporary_templated_vector_methods.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_capacity_alias_named_args.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_stdlib_push_auto_inference.cpp
    tests/unit/test_semantics_calls_and_flow_collections_map_compatibility_explicit_template_calls.cpp
    tests/unit/test_semantics_calls_and_flow_collections_wrapper_returned_map_string_branch_paths.cpp
    tests/unit/test_semantics_calls_and_flow_collections_wrapper_returned_map_count_expression.cpp
    tests/unit/test_semantics_calls_and_flow_collections_vector_helper_call_form_named_receivers.cpp
    tests/unit/test_semantics_calls_and_flow_collections_user_named_map_block_args.cpp
    tests/unit/test_semantics_calls_and_flow_comparisons_literals.cpp
    tests/unit/test_semantics_calls_and_flow_control_core.cpp
    tests/unit/test_semantics_calls_and_flow_control_misc.cpp
    tests/unit/test_semantics_calls_and_flow_named_args.cpp
    tests/unit/test_semantics_calls_and_flow_numeric_builtins.cpp
    tests/unit/test_semantics_condensation_dag.cpp
    tests/unit/test_semantics_calls_and_flow_effects.cpp
    tests/unit/test_semantics_entry_builtins_numeric.cpp
    tests/unit/test_semantics_entry_entry.cpp
    tests/unit/test_semantics_entry_executions.cpp
    tests/unit/test_semantics_entry_math_imports.cpp
    tests/unit/test_semantics_entry_operators.cpp
    tests/unit/test_semantics_entry_tags.cpp
    tests/unit/test_semantics_enum.cpp
    tests/unit/test_semantics_lambdas.cpp
    tests/unit/test_semantics_move.cpp
    tests/unit/test_semantics_numeric_software.cpp
    tests/unit/test_semantics_strongly_connected_components.cpp
    tests/unit/test_semantics_struct_helpers.cpp
    tests/unit/test_semantics_traits.cpp
    tests/unit/test_semantics_type_resolution_graph.cpp
    tests/unit/test_semantics_type_resolution_graph_snapshots.cpp
    tests/unit/test_semantics_type_resolution_return_solver.cpp
    tests/unit/test_semantics_symbol_interner.cpp
    tests/unit/test_semantics_uninitialized_fields.cpp
    tests/unit/test_semantics_entry_parameters.cpp
    tests/unit/test_semantics_entry_return_inference.cpp
    tests/unit/test_semantics_entry_resolution.cpp
    tests/unit/test_semantics_entry_transforms.cpp
    tests/unit/test_semantics_definition_prepass.cpp
    tests/unit/test_semantics_definition_partitioner.cpp
    tests/unit/test_semantics_imports.cpp
    tests/unit/test_semantics_maybe.cpp
    tests/unit/test_semantics_result_helpers.cpp
    tests/unit/test_semantics_result_helpers_file.cpp
    tests/unit/test_semantics_result_helpers_errors.cpp
    tests/unit/test_semantics_result_helpers_gfx.cpp
  )
  target_include_directories(PrimeStruct_semantics_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_semantics_tests PRIVATE primec_ir_lib)
  primestructEnableWarnings(PrimeStruct_semantics_tests)

  add_executable(PrimeStruct_text_filter_tests
    tests/unit/test_main.cpp
    tests/unit/test_text_filter_helpers.cpp
    tests/unit/test_text_filter_transform_rules.cpp
    tests/unit/test_text_filter_pipeline_basics.cpp
    tests/unit/test_text_filter_pipeline_rewrites.cpp
    tests/unit/test_text_filter_pipeline_implicit_utf8.cpp
    tests/unit/test_text_filter_pipeline_implicit_i32.cpp
    tests/unit/test_text_filter_pipeline_collections.cpp
  )
  target_include_directories(PrimeStruct_text_filter_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_text_filter_tests PRIVATE primec_frontend_lib)
  primestructEnableWarnings(PrimeStruct_text_filter_tests)

  add_executable(PrimeStruct_parser_tests
    tests/unit/test_main.cpp
    tests/unit/test_parser_basic_block_control_flow.cpp
    tests/unit/test_parser_basic_control_flow.cpp
    tests/unit/test_parser_basic_definitions_core.cpp
    tests/unit/test_parser_basic_definition_execution.cpp
    tests/unit/test_parser_basic_definition_transforms.cpp
    tests/unit/test_parser_basic_expr_forms.cpp
    tests/unit/test_parser_basic_literals.cpp
    tests/unit/test_parser_basic_parser_helpers.cpp
    tests/unit/test_parser_basic_semantic_transforms_a.cpp
    tests/unit/test_parser_basic_semantic_transforms_b.cpp
    tests/unit/test_parser_basic_sugar.cpp
    tests/unit/test_parser_errors.cpp
    tests/unit/test_parser_errors_more_identifiers.cpp
    tests/unit/test_parser_errors_more_punctuation.cpp
    tests/unit/test_parser_errors_more_literals.cpp
    tests/unit/test_parser_errors_more_named_args.cpp
    tests/unit/test_parser_errors_more_transforms.cpp
    tests/unit/test_parser_errors_more_transforms_canonical.cpp
    tests/unit/test_execution_parsing.cpp
    tests/unit/test_execution_whitespace.cpp
    tests/unit/test_template_parsing.cpp
  )
  target_include_directories(PrimeStruct_parser_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_parser_tests PRIVATE primec_frontend_lib)
  primestructEnableWarnings(PrimeStruct_parser_tests)

  if(PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE)
    add_executable(PrimeStruct_semantics_tsan_smoke
      tests/unit/test_main.cpp
      tests/unit/test_semantics_tsan_smoke.cpp
    )
    target_include_directories(PrimeStruct_semantics_tsan_smoke PRIVATE third_party ${CMAKE_SOURCE_DIR})
    target_link_libraries(PrimeStruct_semantics_tsan_smoke PRIVATE primec_ir_lib)
    primestructEnableWarnings(PrimeStruct_semantics_tsan_smoke)

    add_test(
      NAME PrimeStruct_semantics_tsan_smoke
      COMMAND $<TARGET_FILE:PrimeStruct_semantics_tsan_smoke>
              "--test-suite=primestruct.semantics.tsan_smoke"
              "--order-by=file"
    )
    set_tests_properties(
      PrimeStruct_semantics_tsan_smoke
      PROPERTIES TIMEOUT 300 LABELS "optional-ci;tsan")
  endif()

  set(PrimeStructMiscTestSuites
    primestruct.dumps.ast_ir
    primestruct.dumps.namespace
    primestruct.imports.errors
    primestruct.imports.multiple
    primestruct.imports.resolver
    primestruct.lexer
    primestruct.printers.manual
    primestruct.semantics.manual
    primestruct.diagnostics.codes
  )

  set(PrimeStructBackendTestSuites
    primestruct.ir.pipeline.serialization
    primestruct.ir.pipeline.pointers
    primestruct.ir.pipeline.conversions
    primestruct.ir.pipeline.entry_args
    primestruct.ir.pipeline.backends.core
    primestruct.ir.pipeline.backends.registry
    primestruct.ir.pipeline.backends.glsl
    primestruct.ir.pipeline.backends.cpp_vm
    primestruct.ir.pipeline.type_resolution_parity
    primestruct.ir.pipeline.to_cpp
    primestruct.ir.pipeline.to_glsl
    primestruct.ir.pipeline.gpu
    primestruct.ir.pipeline.validation
    primestruct.vm.debug.session
  )

  set(PrimeStructSemanticsTestSuites
    primestruct.semantics.entry
    primestruct.semantics.resolution
    primestruct.semantics.parameters
    primestruct.semantics.return_inference
    primestruct.semantics.operators
    primestruct.semantics.math_imports
    primestruct.semantics.tags
    primestruct.semantics.builtins_numeric
    primestruct.semantics.executions
    primestruct.semantics.transforms
    primestruct.semantics.effects
    primestruct.semantics.struct_transforms
    primestruct.semantics.builtin_calls
    primestruct.semantics.bindings.core
    primestruct.semantics.bindings.pointers
    primestruct.semantics.bindings.assignments
    primestruct.semantics.bindings.control_flow
    primestruct.semantics.bindings.struct_defaults
    primestruct.semantics.struct_helpers
    primestruct.semantics.result_helpers
    primestruct.semantics.calls_flow.control
    primestruct.semantics.calls_flow.collections
    primestruct.semantics.calls_flow.access
    primestruct.semantics.calls_flow.named_args
    primestruct.semantics.calls_flow.numeric_builtins
    primestruct.semantics.calls_flow.comparisons_literals
    primestruct.semantics.calls_flow.effects
    primestruct.semantics.enum
    primestruct.semantics.imports
    primestruct.semantics.lambdas
    primestruct.semantics.maybe
    primestruct.semantics.traits
    primestruct.semantics.move
    primestruct.semantics.software_numeric
    primestruct.semantics.condensation_dag
    primestruct.semantics.scc
    primestruct.semantics.type_resolution_graph
    primestruct.semantics.uninitialized_fields
  )

  set(PrimeStructTextFilterTestSuites
    primestruct.text_filters.pipeline.basics
    primestruct.text_filters.pipeline.rewrites
    primestruct.text_filters.pipeline.implicit_utf8
    primestruct.text_filters.pipeline.implicit_i32
    primestruct.text_filters.pipeline.collections
    primestruct.text_filters.helpers
  )

  set(PrimeStructParserTestSuites
    primestruct.parser.executions
    primestruct.parser.execution.whitespace
    primestruct.parser.basic
    primestruct.parser.errors
    primestruct.parser.errors.identifiers
    primestruct.parser.errors.punctuation
    primestruct.parser.errors.literals
    primestruct.parser.errors.named_args
    primestruct.parser.errors.transforms
    primestruct.parser.templates
  )

  function(addPrimeStructDoctestSuite suite)
    set(options)
    set(oneValueArgs TARGET TIMEOUT LABEL SHARD_NAME FIRST LAST SOURCE_FILE TEST_CASE TEST_CASE_EXCLUDE)
    cmake_parse_arguments(PrimeStructSuite "${options}" "${oneValueArgs}" "" ${ARGN})
    if (NOT PrimeStructSuite_TARGET)
      set(PrimeStructSuite_TARGET PrimeStruct_backend_tests)
    endif()
    if (NOT PrimeStructSuite_TIMEOUT)
      set(PrimeStructSuite_TIMEOUT 300)
    endif()

    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    if (PrimeStructSuite_SHARD_NAME)
      set(shardId "${PrimeStructSuite_SHARD_NAME}")
    elseif (PrimeStructSuite_FIRST AND PrimeStructSuite_LAST)
      set(shardId "cases${PrimeStructSuite_FIRST}_${PrimeStructSuite_LAST}")
    endif()
    if (shardId)
      set(testName "${testName}_${shardId}")
    endif()

    set(commandArgs "--test-suite=${suite}" "--order-by=file")
    if (PrimeStructSuite_SOURCE_FILE)
      list(APPEND commandArgs "--source-file=${PrimeStructSuite_SOURCE_FILE}")
    endif()
    if (PrimeStructSuite_TEST_CASE)
      list(APPEND commandArgs "--test-case=${PrimeStructSuite_TEST_CASE}")
    endif()
    if (PrimeStructSuite_TEST_CASE_EXCLUDE)
      list(APPEND commandArgs "--test-case-exclude=${PrimeStructSuite_TEST_CASE_EXCLUDE}")
    endif()
    if (PrimeStructSuite_FIRST AND PrimeStructSuite_LAST)
      list(APPEND commandArgs "--first=${PrimeStructSuite_FIRST}"
                              "--last=${PrimeStructSuite_LAST}")
    endif()

    add_test(NAME "${testName}"
             COMMAND $<TARGET_FILE:${PrimeStructSuite_TARGET}> ${commandArgs})
    set_tests_properties("${testName}" PROPERTIES TIMEOUT "${PrimeStructSuite_TIMEOUT}")
    if (PrimeStructSuite_LABEL)
      set_tests_properties("${testName}" PROPERTIES LABELS "${PrimeStructSuite_LABEL}")
    endif()
  endfunction()

  function(addPrimeStructManagedDoctestSuite suite)
    set(options)
    set(oneValueArgs TARGET TIMEOUT LABEL SOURCE_FILE TEST_CASE TEST_CASE_EXCLUDE TOTAL_CASES SHARD_PREFIX CASES_PER_SHARD RANGE_FIRST RANGE_LAST)
    cmake_parse_arguments(PrimeStructManaged "${options}" "${oneValueArgs}" "" ${ARGN})
    if (NOT PrimeStructManaged_CASES_PER_SHARD)
      set(PrimeStructManaged_CASES_PER_SHARD 10)
    endif()
    if (NOT PrimeStructManaged_RANGE_FIRST)
      set(PrimeStructManaged_RANGE_FIRST 1)
    endif()
    if (PrimeStructManaged_RANGE_LAST AND NOT PrimeStructManaged_TOTAL_CASES)
      math(EXPR PrimeStructManaged_TOTAL_CASES
           "${PrimeStructManaged_RANGE_LAST} - ${PrimeStructManaged_RANGE_FIRST} + 1")
    endif()
    if (PrimeStructManaged_RANGE_LAST)
      set(rangeMax "${PrimeStructManaged_RANGE_LAST}")
    else()
      math(EXPR rangeMax
           "${PrimeStructManaged_RANGE_FIRST} + ${PrimeStructManaged_TOTAL_CASES} - 1")
    endif()

    set(commonArgs)
    if (PrimeStructManaged_TARGET)
      list(APPEND commonArgs TARGET "${PrimeStructManaged_TARGET}")
    endif()
    if (PrimeStructManaged_TIMEOUT)
      list(APPEND commonArgs TIMEOUT "${PrimeStructManaged_TIMEOUT}")
    endif()
    if (PrimeStructManaged_LABEL)
      list(APPEND commonArgs LABEL "${PrimeStructManaged_LABEL}")
    endif()
    if (PrimeStructManaged_SOURCE_FILE)
      list(APPEND commonArgs SOURCE_FILE "${PrimeStructManaged_SOURCE_FILE}")
    endif()
    if (PrimeStructManaged_TEST_CASE)
      list(APPEND commonArgs TEST_CASE "${PrimeStructManaged_TEST_CASE}")
    endif()
    if (PrimeStructManaged_TEST_CASE_EXCLUDE)
      list(APPEND commonArgs TEST_CASE_EXCLUDE "${PrimeStructManaged_TEST_CASE_EXCLUDE}")
    endif()
    if (NOT PrimeStructManaged_TOTAL_CASES OR
        PrimeStructManaged_TOTAL_CASES LESS_EQUAL PrimeStructManaged_CASES_PER_SHARD)
      set(singleShardArgs ${commonArgs})
      if (PrimeStructManaged_RANGE_LAST)
        list(APPEND singleShardArgs
             FIRST "${PrimeStructManaged_RANGE_FIRST}"
             LAST "${PrimeStructManaged_RANGE_LAST}")
      endif()
      if (PrimeStructManaged_SHARD_PREFIX)
        addPrimeStructDoctestSuite("${suite}" ${singleShardArgs}
                                   SHARD_NAME "${PrimeStructManaged_SHARD_PREFIX}")
      else()
        addPrimeStructDoctestSuite("${suite}" ${singleShardArgs})
      endif()
      return()
    endif()

    math(EXPR shardCount
         "(${PrimeStructManaged_TOTAL_CASES} + ${PrimeStructManaged_CASES_PER_SHARD} - 1) / ${PrimeStructManaged_CASES_PER_SHARD}")
    math(EXPR lastShardIndex "${shardCount} - 1")
    foreach(shardIndex RANGE ${lastShardIndex})
      math(EXPR first
           "${PrimeStructManaged_RANGE_FIRST} + ${shardIndex} * ${PrimeStructManaged_CASES_PER_SHARD}")
      math(EXPR last "${first} + ${PrimeStructManaged_CASES_PER_SHARD} - 1")
      if (last GREATER rangeMax)
        set(last "${rangeMax}")
      endif()
      set(firstLastSuffix "${first}_${last}")
      if (PrimeStructManaged_SHARD_PREFIX)
        set(shardName "${PrimeStructManaged_SHARD_PREFIX}_${firstLastSuffix}")
      else()
        set(shardName "cases_${firstLastSuffix}")
      endif()
      addPrimeStructDoctestSuite("${suite}" ${commonArgs}
                                 SHARD_NAME "${shardName}"
                                 FIRST "${first}"
                                 LAST "${last}")
    endforeach()
  endfunction()

  set(PrimeStructManagedMiscSuites)
  set(PrimeStructManagedBackendSuites)
  set(PrimeStructManagedSemanticsSuites)
  set(PrimeStructManagedTextFilterSuites)
  set(PrimeStructManagedParserSuites)
  set(PrimeStructManagedCompileRunSuites)
  include("${CMAKE_SOURCE_DIR}/cmake/PrimeStructManagedUnitBackendSuites.cmake" OPTIONAL)
  include("${CMAKE_SOURCE_DIR}/cmake/PrimeStructManagedSemanticsSuites.cmake" OPTIONAL)
  include("${CMAKE_SOURCE_DIR}/cmake/PrimeStructManagedParserTextMiscSuites.cmake" OPTIONAL)
  include("${CMAKE_SOURCE_DIR}/cmake/PrimeStructManagedCompileRunSuites.cmake" OPTIONAL)

  foreach(suite IN LISTS PrimeStructMiscTestSuites)
    if (suite IN_LIST PrimeStructManagedMiscSuites)
      continue()
    endif()
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_misc_tests> "--test-suite=${suite}")
    set_tests_properties("${testName}" PROPERTIES TIMEOUT 300)
  endforeach()

  foreach(suite IN LISTS PrimeStructBackendTestSuites)
    if (suite IN_LIST PrimeStructManagedBackendSuites)
      continue()
    endif()
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_backend_tests> "--test-suite=${suite}")
    set_tests_properties("${testName}" PROPERTIES TIMEOUT 300 LABELS "parallel-safe")
  endforeach()

  foreach(suite IN LISTS PrimeStructSemanticsTestSuites)
    if (suite IN_LIST PrimeStructManagedSemanticsSuites)
      continue()
    endif()
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    set(semanticsTimeout 300)
    set(semanticsLabels "parallel-safe")
    if (suite STREQUAL "primestruct.semantics.math_imports" OR
        suite STREQUAL "primestruct.semantics.imports")
      set(semanticsTimeout 900)
    elseif(suite STREQUAL "primestruct.semantics.result_helpers")
      set(semanticsTimeout 600)
    endif()
    if (suite STREQUAL "primestruct.semantics.operators")
      add_test(NAME "${testName}_matrix_and_shape_rules_core"
               COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests>
                       "--test-suite=${suite}"
                       "--source-file=*test_semantics_entry_operators.cpp"
                       "--order-by=file"
                       "--first=1"
                       "--last=5")
      add_test(NAME "${testName}_matrix_and_shape_rules_shapes"
               COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests>
                       "--test-suite=${suite}"
                       "--source-file=*test_semantics_entry_operators.cpp"
                       "--order-by=file"
                       "--first=6"
                       "--last=10")
      add_test(NAME "${testName}_matrix_and_shape_rules_products"
               COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests>
                       "--test-suite=${suite}"
                       "--source-file=*test_semantics_entry_operators.cpp"
                       "--order-by=file"
                       "--first=11"
                       "--last=15")
      add_test(NAME "${testName}_scalar_and_mixed_numeric_rules"
               COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests>
                       "--test-suite=${suite}"
                       "--source-file=*test_semantics_entry_operators.cpp"
                       "--order-by=file"
                       "--first=16"
                       "--last=29")
      # Semantics-only shards avoid the compile-run temp-artifact conflicts that still
      # require serialization elsewhere in the suite.
      set_tests_properties("${testName}_matrix_and_shape_rules_core" PROPERTIES TIMEOUT 600 LABELS "parallel-safe")
      set_tests_properties("${testName}_matrix_and_shape_rules_shapes" PROPERTIES TIMEOUT 600 LABELS "parallel-safe")
      set_tests_properties("${testName}_matrix_and_shape_rules_products" PROPERTIES TIMEOUT 600 LABELS "parallel-safe")
      set_tests_properties("${testName}_scalar_and_mixed_numeric_rules" PROPERTIES TIMEOUT 900 LABELS "parallel-safe")
    else()
      add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests> "--test-suite=${suite}")
      set_tests_properties("${testName}" PROPERTIES TIMEOUT "${semanticsTimeout}" LABELS "${semanticsLabels}")
    endif()
  endforeach()

  foreach(suite IN LISTS PrimeStructTextFilterTestSuites)
    if (suite IN_LIST PrimeStructManagedTextFilterSuites)
      continue()
    endif()
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_text_filter_tests> "--test-suite=${suite}")
    set_tests_properties("${testName}" PROPERTIES TIMEOUT 300 LABELS "parallel-safe")
  endforeach()

  foreach(suite IN LISTS PrimeStructParserTestSuites)
    if (suite IN_LIST PrimeStructManagedParserSuites)
      continue()
    endif()
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_parser_tests> "--test-suite=${suite}")
    set_tests_properties("${testName}" PROPERTIES TIMEOUT 300 LABELS "parallel-safe")
  endforeach()

  add_test(
    NAME PrimeStruct_include_layers
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_include_layers.py
            --root ${CMAKE_SOURCE_DIR}
            --allowlist ${CMAKE_SOURCE_DIR}/scripts/include_layer_allowlist.txt
  )
  set_tests_properties(PrimeStruct_include_layers PROPERTIES TIMEOUT 60)

  add_test(
    NAME PrimeStruct_test_suite_naming
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_test_suite_naming.py
            --root ${CMAKE_SOURCE_DIR}
  )
  set_tests_properties(PrimeStruct_test_suite_naming PROPERTIES TIMEOUT 60)

  add_custom_target(
    primestruct_graph_budget_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_graph_budget.py
            --primec $<TARGET_FILE:primec>
            --repo-root ${CMAKE_SOURCE_DIR}
            --baseline ${CMAKE_SOURCE_DIR}/benchmarks/type_graph_budget_baseline.json
    DEPENDS primec
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking type-graph budget thresholds"
    VERBATIM
  )

  add_test(
    NAME PrimeStruct_graph_budget
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_graph_budget.py
            --primec $<TARGET_FILE:primec>
            --repo-root ${CMAKE_SOURCE_DIR}
            --baseline ${CMAKE_SOURCE_DIR}/benchmarks/type_graph_budget_baseline.json
  )
  set_tests_properties(PrimeStruct_graph_budget PROPERTIES TIMEOUT 120 LABELS "parallel-safe")

  set(PRIMESTRUCT_SEMANTIC_MEMORY_POLICY
      ${CMAKE_SOURCE_DIR}/benchmarks/semantic_memory_budget_policy.json)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PRIMESTRUCT_SEMANTIC_MEMORY_POLICY
        ${CMAKE_SOURCE_DIR}/benchmarks/semantic_memory_budget_policy_debug.json)
  endif()

  add_custom_target(
    primestruct_semantic_memory_benchmark
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/semantic_memory_ci_artifacts.py
            --mode benchmark
            --run-label semantic_memory_benchmark
            --primec $<TARGET_FILE:primec>
            --repo-root ${CMAKE_SOURCE_DIR}
            --runs 3
            --policy ${PRIMESTRUCT_SEMANTIC_MEMORY_POLICY}
            --ignore-wall-seconds
            --history-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_history
            --history-limit 2
            --benchmark-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_report.json
            --budget-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_budget_check_report.json
            --trend-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_trend_report.json
            --artifacts-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_artifacts
    DEPENDS primec
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running semantic memory benchmark + budget gate with artifact capture"
    VERBATIM
  )

  add_test(
    NAME PrimeStruct_semantic_memory_benchmark
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/semantic_memory_ci_artifacts.py
            --mode benchmark
            --run-label semantic_memory_benchmark
            --primec $<TARGET_FILE:primec>
            --repo-root ${CMAKE_SOURCE_DIR}
            --runs 3
            --policy ${PRIMESTRUCT_SEMANTIC_MEMORY_POLICY}
            --ignore-wall-seconds
            --history-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_history
            --history-limit 2
            --benchmark-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_report.json
            --budget-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_budget_check_report.json
            --trend-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_trend_report.json
            --artifacts-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_artifacts
  )
  set_tests_properties(
    PrimeStruct_semantic_memory_benchmark
    PROPERTIES TIMEOUT 1800)

  add_custom_target(
    primestruct_semantic_memory_definition_worker_parity
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/semantic_memory_ci_artifacts.py
            --mode benchmark
            --run-label semantic_memory_definition_worker_parity
            --primec $<TARGET_FILE:primec>
            --repo-root ${CMAKE_SOURCE_DIR}
            --runs 3
            --policy ${PRIMESTRUCT_SEMANTIC_MEMORY_POLICY}
            --ignore-wall-seconds
            --benchmark-definition-validation-workers both
            --history-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_history
            --history-limit 2
            --benchmark-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_report.json
            --budget-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_budget_check_report.json
            --trend-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_trend_report.json
            --artifacts-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_artifacts
    DEPENDS primec
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running semantic memory benchmark definition-worker parity gate"
    VERBATIM
  )

  add_test(
    NAME PrimeStruct_semantic_memory_definition_worker_parity
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/semantic_memory_ci_artifacts.py
            --mode benchmark
            --run-label semantic_memory_definition_worker_parity
            --primec $<TARGET_FILE:primec>
            --repo-root ${CMAKE_SOURCE_DIR}
            --runs 3
            --policy ${PRIMESTRUCT_SEMANTIC_MEMORY_POLICY}
            --ignore-wall-seconds
            --benchmark-definition-validation-workers both
            --history-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_history
            --history-limit 2
            --benchmark-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_report.json
            --budget-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_budget_check_report.json
            --trend-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_trend_report.json
            --artifacts-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_definition_worker_parity_artifacts
  )
  set_tests_properties(
    PrimeStruct_semantic_memory_definition_worker_parity
    PROPERTIES TIMEOUT 1800)

  add_custom_target(
    primestruct_semantic_memory_trend_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/semantic_memory_ci_artifacts.py
            --mode trend
            --run-label semantic_memory_trend
            --primec $<TARGET_FILE:primec>
            --runs 3
            --policy ${PRIMESTRUCT_SEMANTIC_MEMORY_POLICY}
            --ignore-wall-seconds
            --history-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_history
            --history-limit 2
            --benchmark-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_report.json
            --budget-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_budget_check_report.json
            --artifacts-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_artifacts
            --repo-root ${CMAKE_SOURCE_DIR}
    DEPENDS primec primestruct_semantic_memory_benchmark
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking semantic memory sustained trend policy with artifact capture"
    VERBATIM
  )

  add_test(
    NAME PrimeStruct_semantic_memory_trend
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/semantic_memory_ci_artifacts.py
            --mode trend
            --run-label semantic_memory_trend
            --primec $<TARGET_FILE:primec>
            --runs 3
            --policy ${PRIMESTRUCT_SEMANTIC_MEMORY_POLICY}
            --ignore-wall-seconds
            --history-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_history
            --history-limit 2
            --benchmark-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_report.json
            --budget-report ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_budget_check_report.json
            --artifacts-dir ${CMAKE_BINARY_DIR}/benchmarks/semantic_memory_artifacts
            --repo-root ${CMAKE_SOURCE_DIR}
  )
  set_tests_properties(
    PrimeStruct_semantic_memory_trend
    PROPERTIES DEPENDS PrimeStruct_semantic_memory_benchmark TIMEOUT 300)

  set(PrimeStructCompileRunSuites
    primestruct.compile.run.smoke
    primestruct.compile.run.vm.core
    primestruct.compile.run.vm.collections
    primestruct.compile.run.vm.bounds
    primestruct.compile.run.vm.maps
    primestruct.compile.run.vm.math
    primestruct.compile.run.vm.outputs
    primestruct.compile.run.vm.uninitialized
    primestruct.compile.run.vm.maybe
    primestruct.compile.run.vm.gpu
    primestruct.compile.run.emitters.cpp
    primestruct.compile.run.math_conformance
    primestruct.compile.run.glsl
    primestruct.compile.run.native_backend.core
    primestruct.compile.run.native_backend.uninitialized
    primestruct.compile.run.native_backend.maybe
    primestruct.compile.run.native_backend.argv
    primestruct.compile.run.native_backend.control
    primestruct.compile.run.native_backend.pointers
    primestruct.compile.run.native_backend.math_numeric
    primestruct.compile.run.native_backend.collections
    primestruct.compile.run.native_backend.imports
    primestruct.compile.run.reflection_codegen
    primestruct.compile.run.benchmark_harness
    primestruct.compile.run.imports
    primestruct.compile.run.text_filters
    primestruct.compile.run.bindings
    primestruct.compile.run.examples
  )
  foreach(suite IN LISTS PrimeStructCompileRunSuites)
    if (suite IN_LIST PrimeStructManagedCompileRunSuites)
      continue()
    endif()
    if (suite STREQUAL "primestruct.compile.run.vm.core")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_01a"
                                 SOURCE_FILE "*test_compile_run_vm_core_*.cpp" FIRST 1 LAST 14)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_01b"
                                 SOURCE_FILE "*test_compile_run_vm_core_*.cpp" FIRST 15 LAST 28)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_02"
                                 SOURCE_FILE "*test_compile_run_vm_core_*.cpp" FIRST 29 LAST 56)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_03a"
                                 SOURCE_FILE "*test_compile_run_vm_core_*.cpp" FIRST 57 LAST 70)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_03b"
                                 SOURCE_FILE "*test_compile_run_vm_core_*.cpp" FIRST 71 LAST 83)
    elseif(suite STREQUAL "primestruct.compile.run.vm.collections")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "alias_and_basics"
                                 SOURCE_FILE "*test_compile_run_vm_collections_*.cpp" FIRST 1 LAST 60)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "templated_wrapper_parity"
                                 SOURCE_FILE "*test_compile_run_vm_collections_*.cpp" FIRST 61 LAST 138)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims"
                                 SOURCE_FILE "*test_compile_run_vm_collections_*.cpp" FIRST 139 LAST 254)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "user_shadow_and_receiver_precedence"
                                 SOURCE_FILE "*test_compile_run_vm_collections_*.cpp" FIRST 255 LAST 331)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "growth_limits_and_syntax"
                                 SOURCE_FILE "*test_compile_run_vm_collections_*.cpp" FIRST 332 LAST 352)
    elseif(suite STREQUAL "primestruct.compile.run.vm.bounds")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "bounds_and_safety"
                                 SOURCE_FILE "*test_compile_run_vm_bounds.cpp")
    elseif(suite STREQUAL "primestruct.compile.run.vm.maps")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "map_helpers"
                                 SOURCE_FILE "*test_compile_run_vm_maps.cpp")
    elseif(suite STREQUAL "primestruct.compile.run.vm.math")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "math_helpers"
                                 SOURCE_FILE "*test_compile_run_vm_math.cpp")
    elseif(suite STREQUAL "primestruct.compile.run.vm.outputs")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_basics"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.cpp" FIRST 1 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_emitters"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.cpp" FIRST 23 LAST 44)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_exe_backend"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.cpp" FIRST 45 LAST 66)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_argv_arrays"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.cpp" FIRST 67 LAST 88)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.core")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "01a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 1 LAST 10)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "01b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 11 LAST 20)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "02a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 21 LAST 30)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "02b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 31 LAST 40)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "03a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 41 LAST 50)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "03b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 51 LAST 60)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "04a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 61 LAST 70)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "04b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 71 LAST 80)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "05a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 81 LAST 90)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "05b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 91 LAST 100)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "06a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 101 LAST 110)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "06b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 111 LAST 120)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "07a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 121 LAST 130)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "07b"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 131 LAST 140)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "08a"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 141 LAST 147)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "08b1"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 148 LAST 151)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 1800 SHARD_NAME "08b2"
                                 SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp" FIRST 152 LAST 154)
    elseif(suite STREQUAL "primestruct.compile.run.emitters.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "lambda_and_mutator_resolution"
                                 SOURCE_FILE "*test_compile_run_emitters_*.cpp" FIRST 1 LAST 45)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "collection_access_and_alias_forwarding"
                                 SOURCE_FILE "*test_compile_run_emitters_*.cpp" FIRST 46 LAST 100)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "map_wrapper_and_fallback_inference"
                                 SOURCE_FILE "*test_compile_run_emitters_*.cpp" FIRST 101 LAST 160)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "numeric_math_and_control_flow_core"
                                 SOURCE_FILE "*test_compile_run_emitters_*.cpp" FIRST 161 LAST 176)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "numeric_math_and_control_flow_loops"
                                 SOURCE_FILE "*test_compile_run_emitters_*.cpp" FIRST 177 LAST 192)
    elseif(suite STREQUAL "primestruct.compile.run.math_conformance")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "harness_and_docs"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 1 LAST 4)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "scalar_basics"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 5 LAST 8)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_quadrant_axes"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 9 LAST 9)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_quadrant_symmetries"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 10 LAST 10)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_quadrant_range_reduction"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 11 LAST 11)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_parser_helpers"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 12 LAST 13)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_basics"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 14 LAST 14)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "inverse_trig_and_atan2"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 15 LAST 15)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "hyperbolic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 16 LAST 16)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "inverse_hyperbolic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 17 LAST 17)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "exp_and_log"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 18 LAST 18)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "exp_log_domains"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 19 LAST 19)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_basics"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 20 LAST 20)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_inverse_trig_logs"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 21 LAST 21)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_hyperbolic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 22 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_grids"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 23 LAST 23)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_rounding"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 24 LAST 24)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "roots_rounding_and_misc"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 25 LAST 27)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "stress_grid"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 28 LAST 28)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_baseline_trigonometric"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 29 LAST 29)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_baseline_transcendental"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 30 LAST 30)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_baseline_composition"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 31 LAST 31)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_sin"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 32 LAST 32)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_cos"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 33 LAST 33)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_exp_log"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 34 LAST 34)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_hypot"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 35 LAST 35)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "native_approximation_limits"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 36 LAST 36)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "heavy_trig_workload"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 37 LAST 37)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "heavy_exp_log_workload"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 38 LAST 38)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "array_dense_and_deterministic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 39 LAST 41)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "deterministic_and_conversions"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 42 LAST 44)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "policy_and_edge_cases"
                                 SOURCE_FILE "*test_compile_run_math_conformance.cpp" FIRST 45 LAST 49)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.collections")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_aliases_and_wrappers"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 1 LAST 63)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "templated_wrapper_parity"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 64 LAST 141)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 142 LAST 180)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims_extended"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 181 LAST 218)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims_accessors"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 219 LAST 257)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "user_shadow_and_receiver_precedence_core"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 258 LAST 296)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "user_shadow_and_receiver_precedence_advanced"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 297 LAST 335)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "growth_limits_and_map_access"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections_*.cpp"
                                 FIRST 336 LAST 380)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.math_numeric")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "math_builtins"
                                 SOURCE_FILE "*test_compile_run_native_backend_math_numeric.cpp"
                                 FIRST 1 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "numeric_and_boolean_flow"
                                 SOURCE_FILE "*test_compile_run_native_backend_math_numeric.cpp"
                                 FIRST 23 LAST 34)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "conversions_and_rejections"
                                 SOURCE_FILE "*test_compile_run_native_backend_math_numeric.cpp"
                                 FIRST 35 LAST 44)
    elseif(suite STREQUAL "primestruct.compile.run.imports")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "versioned_import_resolution"
                                 SOURCE_FILE "*test_compile_run_imports_versions.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "block_and_operator_rewrites"
                                 SOURCE_FILE "*test_compile_run_imports_blocks.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "operations_and_collections"
                                 SOURCE_FILE "*test_compile_run_imports_operations.cpp")
    elseif(suite STREQUAL "primestruct.compile.run.text_filters")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "flag_and_transform_basics"
                                 SOURCE_FILE "*test_compile_run_text_filters_core_lists.cpp" FIRST 1 LAST 28)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "semantic_transform_rules"
                                 SOURCE_FILE "*test_compile_run_text_filters_semantic_rules.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "text_transform_rules"
                                 SOURCE_FILE "*test_compile_run_text_filters_text_rules.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "misc_language_and_diagnostics_basics"
                                 SOURCE_FILE "*test_compile_run_text_filters_misc.cpp" FIRST 1 LAST 18)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "misc_language_and_diagnostics_extended"
                                 SOURCE_FILE "*test_compile_run_text_filters_misc.cpp" FIRST 19 LAST 35)
    elseif(suite STREQUAL "primestruct.compile.run.examples")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "spinning_cube_pipeline_and_scripts"
                                 SOURCE_FILE "*test_compile_run_examples_*.cpp"
                                 FIRST 1 LAST 23)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "spinning_cube_argument_validation"
                                 SOURCE_FILE "*test_compile_run_examples_*.cpp"
                                 FIRST 24 LAST 47)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "native_window_launcher_and_preflight"
                                 SOURCE_FILE "*test_compile_run_examples_*.cpp"
                                 FIRST 48 LAST 62)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "metal_pipeline_and_borrow_checker"
                                 SOURCE_FILE "*test_compile_run_examples_*.cpp"
                                 FIRST 63 LAST 68)
    else()
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900)
    endif()
  endforeach()

  # VM compile-run coverage is broader than the current VM runtime surface.
  # Keep the unsupported shards disabled until image IO, GPU dispatch fallback,
  # and the stdlib/experimental map shim paths are implemented for the VM.
  function(addPrimeStructSuiteRegistration testName target expectedSuitesFileName)
    set(expectedSuitesFile "${CMAKE_CURRENT_BINARY_DIR}/${expectedSuitesFileName}")
    file(WRITE "${expectedSuitesFile}" "")
    foreach(suite IN LISTS ARGN)
      file(APPEND "${expectedSuitesFile}" "${suite}\n")
    endforeach()
    add_test(
      NAME "${testName}"
      COMMAND ${CMAKE_COMMAND}
              -DTEST_BINARY=$<TARGET_FILE:${target}>
              -DEXPECTED_SUITES_FILE=${expectedSuitesFile}
              -P ${CMAKE_SOURCE_DIR}/cmake/CheckDoctestSuites.cmake
    )
    set_tests_properties("${testName}" PROPERTIES TIMEOUT 60)
  endfunction()

  addPrimeStructSuiteRegistration(
    PrimeStruct_misc_suite_registration
    PrimeStruct_misc_tests
    expected_misc_doctest_suites.txt
    ${PrimeStructMiscTestSuites}
  )
  addPrimeStructSuiteRegistration(
    PrimeStruct_backend_suite_registration
    PrimeStruct_backend_tests
    expected_backend_doctest_suites.txt
    ${PrimeStructBackendTestSuites}
    ${PrimeStructCompileRunSuites}
  )
  addPrimeStructSuiteRegistration(
    PrimeStruct_parser_suite_registration
    PrimeStruct_parser_tests
    expected_parser_doctest_suites.txt
    ${PrimeStructParserTestSuites}
  )
  addPrimeStructSuiteRegistration(
    PrimeStruct_text_filter_suite_registration
    PrimeStruct_text_filter_tests
    expected_text_filter_doctest_suites.txt
    ${PrimeStructTextFilterTestSuites}
  )

  # Attach each CTest test name to its environment so helper wrappers can
  # attribute runtime/memory observations to the owning CTest registration.
  get_property(allPrimeStructTests DIRECTORY PROPERTY TESTS)
  foreach(testName IN LISTS allPrimeStructTests)
    set_property(TEST "${testName}" APPEND PROPERTY ENVIRONMENT
                 "PRIMESTRUCT_CURRENT_CTEST_NAME=${testName}")
  endforeach()
endif()

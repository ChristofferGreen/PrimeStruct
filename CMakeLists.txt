cmake_minimum_required(VERSION 3.20)
project(PrimeStruct LANGUAGES CXX)

option(PRIMESTRUCT_BUILD_TESTS "Build PrimeStruct tests" ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_library(primec_lib
  src/AstPrinter.cpp
  src/CompilePipeline.cpp
  src/Diagnostics.cpp
  src/EmitKind.cpp
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
  src/emitter/EmitterHelpersBuiltins.cpp
  src/emitter/EmitterHelpersTypes.cpp
  src/glsl_emitter/GlslEmitter.cpp
  src/glsl_emitter/GlslEmitterExpr.cpp
  src/glsl_emitter/GlslEmitterHelpers.cpp
  src/glsl_emitter/GlslEmitterStatements.cpp
  src/ImportResolver.cpp
  src/IrBackends.cpp
  src/IrInliner.cpp
  src/IrPreparation.cpp
  src/IrToCppEmitter.cpp
  src/IrToGlslEmitter.cpp
  src/ir_lowerer/IrLowererCallHelpers.cpp
  src/ir_lowerer/IrLowererBindingTransformHelpers.cpp
  src/ir_lowerer/IrLowererBindingTypeHelpers.cpp
  src/ir_lowerer/IrLowererCountAccessHelpers.cpp
  src/ir_lowerer/IrLowererLowerEntrySetup.cpp
  src/ir_lowerer/IrLowererLowerInlineCallActiveContextStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallCleanupStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallContextSetupStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallGpuLocalsStep.cpp
  src/ir_lowerer/IrLowererLowerInlineCallReturnValueStep.cpp
  src/ir_lowerer/IrLowererLowerImportsStructsSetup.cpp
  src/ir_lowerer/IrLowererLowerInlineCallStatementStep.cpp
  src/ir_lowerer/IrLowererLowerLocalsSetup.cpp
  src/ir_lowerer/IrLowererLowerInferenceSetup.cpp
  src/ir_lowerer/IrLowererLowerExprEmitSetup.cpp
  src/ir_lowerer/IrLowererLowerReturnCallsSetup.cpp
  src/ir_lowerer/IrLowererLowerStatementsCallsStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsEntryExecutionStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsEntryStatementStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsFunctionTableStep.cpp
  src/ir_lowerer/IrLowererLowerStatementsSourceMapStep.cpp
  src/ir_lowerer/IrLowererFileWriteHelpers.cpp
  src/ir_lowerer/IrLowererFlowHelpers.cpp
  src/ir_lowerer/IrLowererHelpers.cpp
  src/ir_lowerer/IrLowererIndexKindHelpers.cpp
  src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp
  src/ir_lowerer/IrLowererInlineParamHelpers.cpp
  src/ir_lowerer/IrLowererInlineStructArgHelpers.cpp
  src/ir_lowerer/IrLowererLowerEffects.cpp
  src/ir_lowerer/IrLowererOnErrorHelpers.cpp
  src/ir_lowerer/IrLowererOperatorArithmeticHelpers.cpp
  src/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.cpp
  src/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.cpp
  src/ir_lowerer/IrLowererOperatorComparisonHelpers.cpp
  src/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.cpp
  src/ir_lowerer/IrLowererOperatorPowAbsSignHelpers.cpp
  src/ir_lowerer/IrLowererOperatorSaturateRoundingRootsHelpers.cpp
  src/ir_lowerer/IrLowererReturnInferenceHelpers.cpp
  src/ir_lowerer/IrLowererResultHelpers.cpp
  src/ir_lowerer/IrLowererRuntimeErrorHelpers.cpp
  src/ir_lowerer/IrLowererSetupInferenceHelpers.cpp
  src/ir_lowerer/IrLowererSetupLocalsHelpers.cpp
  src/ir_lowerer/IrLowererSetupMathHelpers.cpp
  src/ir_lowerer/IrLowererStatementBindingHelpers.cpp
  src/ir_lowerer/IrLowererStatementCallHelpers.cpp
  src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp
  src/ir_lowerer/IrLowererStructLayoutHelpers.cpp
  src/ir_lowerer/IrLowererStructReturnPathHelpers.cpp
  src/ir_lowerer/IrLowererStructTypeHelpers.cpp
  src/ir_lowerer/IrLowererSetupTypeHelpers.cpp
  src/ir_lowerer/IrLowererUninitializedTypeHelpers.cpp
  src/ir_lowerer/IrLowererStringLiteralHelpers.cpp
  src/ir_lowerer/IrLowererStringCallHelpers.cpp
  src/ir_lowerer/IrLowererTemplateTypeParseHelpers.cpp
  src/ir_lowerer/IrLowererLower.cpp
  src/IrPrinter.cpp
  src/IrSerializer.cpp
  src/IrValidation.cpp
  src/IrVirtualRegisterAllocator.cpp
  src/IrVirtualRegisterLiveness.cpp
  src/IrVirtualRegisterLowering.cpp
  src/IrVirtualRegisterScheduler.cpp
  src/IrVirtualRegisterSpillInsertion.cpp
  src/IrVirtualRegisterVerifier.cpp
  src/Lexer.cpp
  src/TransformRules.cpp
  src/TransformRegistry.cpp
  src/native_emitter/NativeEmitterHelpers.cpp
  src/native_emitter/NativeEmitterEmit.cpp
  src/OptionsParser.cpp
  src/ProcessRunner.cpp
  src/VmDebugAdapter.cpp
  src/VmDebugDap.cpp
  src/parser/ParserCore.cpp
  src/parser/ParserExpr.cpp
  src/parser/ParserHelpers.cpp
  src/parser/ParserLists.cpp
  src/semantics/SemanticsHelpersCore.cpp
  src/semantics/SemanticsHelpersValidation.cpp
  src/semantics/TemplateMonomorph.cpp
  src/semantics/SemanticsValidate.cpp
  src/semantics/SemanticsValidator.cpp
  src/semantics/SemanticsValidatorBuild.cpp
  src/semantics/SemanticsValidatorExprCaptureSplitStep.cpp
  src/semantics/SemanticsValidatorExpr.cpp
  src/semantics/SemanticsValidatorInfer.cpp
  src/semantics/SemanticsValidatorPasses.cpp
  src/semantics/SemanticsValidatorStatementLoopCountStep.cpp
  src/semantics/SemanticsValidatorStatementReturns.cpp
  src/semantics/SemanticsValidatorStatement.cpp
  src/semantics/SemanticsValidatorTraits.cpp
  src/text_filter/TextFilterHelpers.cpp
  src/text_filter/TextFilterPipelineEnvelope.cpp
  src/text_filter/TextFilterPipelinePass.cpp
  src/text_filter/TextFilterPipelineScan.cpp
  src/text_filter/TextFilterPipeline.cpp
  src/wasm_emitter/WasmEmitter.cpp
  src/Vm.cpp
)

target_include_directories(primec_lib PUBLIC include)
primestructEnableWarnings(primec_lib)

add_executable(primec src/main.cpp)
target_link_libraries(primec PRIVATE primec_lib)
primestructEnableWarnings(primec)

add_executable(primevm src/primevm_main.cpp)
target_link_libraries(primevm PRIVATE primec_lib)
primestructEnableWarnings(primevm)

if(PRIMESTRUCT_BUILD_TESTS)
  enable_testing()
  add_executable(PrimeStruct_tests
    tests/unit/test_main.cpp
    tests/unit/test_parser_basic.cpp
    tests/unit/test_import_resolver.cpp
    tests/unit/test_import_errors.cpp
    tests/unit/test_import_multiple.cpp
    tests/unit/test_semantics.cpp
    tests/unit/test_ast_ir_dump.cpp
    tests/unit/test_ast_ir_dump_namespace.cpp
    tests/unit/test_parser_errors.cpp
    tests/unit/test_parser_errors_more.cpp
    tests/unit/test_execution_parsing.cpp
    tests/unit/test_execution_whitespace.cpp
    tests/unit/test_ir_pipeline.cpp
    tests/unit/test_template_parsing.cpp
    tests/unit/test_compile_run.cpp
    tests/unit/test_text_filter_helpers.cpp
    tests/unit/test_text_filter_pipeline_basics.cpp
    tests/unit/test_text_filter_pipeline_rewrites.cpp
    tests/unit/test_text_filter_pipeline_implicit_utf8.cpp
    tests/unit/test_text_filter_pipeline_implicit_i32.cpp
    tests/unit/test_text_filter_pipeline_collections.cpp
    tests/unit/test_printers_manual.cpp
    tests/unit/test_semantics_manual.cpp
    tests/unit/test_lexer.cpp
    tests/unit/test_diagnostics_codes.cpp
  )
  target_include_directories(PrimeStruct_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_tests PRIVATE primec_lib)
  primestructEnableWarnings(PrimeStruct_tests)

  set(PrimeStructTestSuites
    primestruct.dumps.ast_ir
    primestruct.dumps.namespace
    primestruct.parser.executions
    primestruct.parser.execution.whitespace
    primestruct.imports.errors
    primestruct.imports.multiple
    primestruct.imports.resolver
    primestruct.ir.pipeline.serialization
    primestruct.ir.pipeline.pointers
    primestruct.ir.pipeline.conversions
    primestruct.ir.pipeline.entry_args
    primestruct.ir.pipeline.backends
    primestruct.ir.pipeline.to_cpp
    primestruct.ir.pipeline.to_glsl
    primestruct.ir.pipeline.gpu
    primestruct.ir.pipeline.validation
    primestruct.vm.debug.session
    primestruct.lexer
    primestruct.parser.basic
    primestruct.parser.errors
    primestruct.parser.errors.identifiers
    primestruct.parser.errors.punctuation
    primestruct.parser.errors.literals
    primestruct.parser.errors.named_args
    primestruct.parser.errors.transforms
    primestruct.printers.manual
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
    primestruct.semantics.manual
    primestruct.semantics.move
    primestruct.semantics.software_numeric
    primestruct.semantics.uninitialized_fields
    primestruct.parser.templates
    primestruct.text_filters.pipeline.basics
    primestruct.text_filters.pipeline.rewrites
    primestruct.text_filters.pipeline.implicit_utf8
    primestruct.text_filters.pipeline.implicit_i32
    primestruct.text_filters.pipeline.collections
    primestruct.text_filters.helpers
    primestruct.diagnostics.codes
  )

  foreach(suite IN LISTS PrimeStructTestSuites)
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_tests> "--test-suite=${suite}")
    set_tests_properties("${testName}" PROPERTIES RUN_SERIAL TRUE)
    set_tests_properties("${testName}" PROPERTIES TIMEOUT 300)
  endforeach()

  function(addPrimeStructDoctestSuite suite)
    set(options)
    set(oneValueArgs TIMEOUT LABEL SHARD_NAME FIRST LAST SOURCE_FILE TEST_CASE TEST_CASE_EXCLUDE)
    cmake_parse_arguments(PrimeStructSuite "${options}" "${oneValueArgs}" "" ${ARGN})
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

    set(commandArgs "--test-suite=${suite}")
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
      list(APPEND commandArgs "--order-by=file"
                              "--first=${PrimeStructSuite_FIRST}"
                              "--last=${PrimeStructSuite_LAST}")
    endif()

    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeStruct_tests> ${commandArgs})
    set_tests_properties("${testName}" PROPERTIES RUN_SERIAL TRUE TIMEOUT "${PrimeStructSuite_TIMEOUT}")
    if (PrimeStructSuite_LABEL)
      set_tests_properties("${testName}" PROPERTIES LABELS "${PrimeStructSuite_LABEL}")
    endif()
  endfunction()

  set(PrimeStructCompileRunSuites
    primestruct.compile.run.smoke
    primestruct.compile.run.vm.core
    primestruct.compile.run.vm.collections
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
    if (suite STREQUAL "primestruct.compile.run.smoke")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_paths_foundation"
                                 SOURCE_FILE "*test_compile_run_smoke_core.h" FIRST 1 LAST 31)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_paths_wasm_and_debug"
                                 SOURCE_FILE "*test_compile_run_smoke_core.h" FIRST 32 LAST 62)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "collective_paths_core"
                                 SOURCE_FILE "*test_compile_run_smoke_collective.h" FIRST 1 LAST 11)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "collective_paths_extended"
                                 SOURCE_FILE "*test_compile_run_smoke_collective.h" FIRST 12 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "argv_and_cli"
                                 SOURCE_FILE "*test_compile_run_smoke_argv.h")
    elseif(suite STREQUAL "primestruct.compile.run.vm.collections")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "bounds_and_safety"
                                 SOURCE_FILE "*test_compile_run_vm_bounds.h")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "map_helpers"
                                 SOURCE_FILE "*test_compile_run_vm_maps.h")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "math_helpers"
                                 SOURCE_FILE "*test_compile_run_vm_math.h")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_basics"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.h" FIRST 1 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_emitters"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.h" FIRST 23 LAST 44)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_exe_backend"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.h" FIRST 45 LAST 66)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "ir_and_output_modes_argv_arrays"
                                 SOURCE_FILE "*test_compile_run_vm_outputs.h" FIRST 67 LAST 88)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "alias_and_basics"
                                 SOURCE_FILE "*test_compile_run_vm_collections.h" FIRST 1 LAST 60)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "templated_wrapper_parity"
                                 SOURCE_FILE "*test_compile_run_vm_collections.h" FIRST 61 LAST 138)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims"
                                 SOURCE_FILE "*test_compile_run_vm_collections.h" FIRST 139 LAST 254)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "user_shadow_and_receiver_precedence"
                                 SOURCE_FILE "*test_compile_run_vm_collections.h" FIRST 255 LAST 331)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "growth_limits_and_syntax"
                                 SOURCE_FILE "*test_compile_run_vm_collections.h" FIRST 332 LAST 352)
    elseif(suite STREQUAL "primestruct.compile.run.emitters.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "lambda_and_mutator_resolution"
                                 SOURCE_FILE "*test_compile_run_emitters.h" FIRST 1 LAST 45)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "collection_access_and_alias_forwarding"
                                 SOURCE_FILE "*test_compile_run_emitters.h" FIRST 46 LAST 100)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "map_wrapper_and_fallback_inference"
                                 SOURCE_FILE "*test_compile_run_emitters.h" FIRST 101 LAST 160)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "numeric_math_and_control_flow_core"
                                 SOURCE_FILE "*test_compile_run_emitters.h" FIRST 161 LAST 176)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "numeric_math_and_control_flow_loops"
                                 SOURCE_FILE "*test_compile_run_emitters.h" FIRST 177 LAST 192)
    elseif(suite STREQUAL "primestruct.compile.run.math_conformance")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "harness_and_docs"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 1 LAST 4)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "scalar_basics"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 5 LAST 8)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_quadrant_axes"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 9 LAST 9)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_quadrant_symmetries"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 10 LAST 10)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_quadrant_range_reduction"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 11 LAST 11)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_parser_helpers"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 12 LAST 13)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "trig_basics"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 14 LAST 14)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "inverse_trig_and_atan2"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 15 LAST 15)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "hyperbolic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 16 LAST 16)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "inverse_hyperbolic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 17 LAST 17)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "exp_and_log"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 18 LAST 18)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "exp_log_domains"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 19 LAST 19)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_basics"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 20 LAST 20)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_inverse_trig_logs"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 21 LAST 21)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_hyperbolic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 22 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_grids"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 23 LAST 23)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float64_rounding"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 24 LAST 24)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "roots_rounding_and_misc"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 25 LAST 27)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "stress_grid"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 28 LAST 28)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_baseline_trigonometric"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 29 LAST 29)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_baseline_transcendental"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 30 LAST 30)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_baseline_composition"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 31 LAST 31)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_sin"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 32 LAST 32)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_cos"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 33 LAST 33)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_exp_log"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 34 LAST 34)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "float_grid_hypot"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 35 LAST 35)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "native_approximation_limits"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 36 LAST 36)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "heavy_trig_workload"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 37 LAST 37)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "heavy_exp_log_workload"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 38 LAST 38)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "array_dense_and_deterministic"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 39 LAST 41)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "deterministic_and_conversions"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 42 LAST 44)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance"
                                 SHARD_NAME "policy_and_edge_cases"
                                 SOURCE_FILE "*test_compile_run_math_conformance.h" FIRST 45 LAST 49)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.collections")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "core_aliases_and_wrappers"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 1 LAST 63)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "templated_wrapper_parity"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 64 LAST 141)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 142 LAST 180)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims_extended"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 181 LAST 218)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "stdlib_collection_shims_accessors"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 219 LAST 257)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "user_shadow_and_receiver_precedence_core"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 258 LAST 296)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "user_shadow_and_receiver_precedence_advanced"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 297 LAST 335)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "growth_limits_and_map_access"
                                 SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                 FIRST 336 LAST 380)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.math_numeric")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "math_builtins"
                                 SOURCE_FILE "*test_compile_run_native_backend_math_numeric.h"
                                 FIRST 1 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "numeric_and_boolean_flow"
                                 SOURCE_FILE "*test_compile_run_native_backend_math_numeric.h"
                                 FIRST 23 LAST 34)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "conversions_and_rejections"
                                 SOURCE_FILE "*test_compile_run_native_backend_math_numeric.h"
                                 FIRST 35 LAST 44)
    elseif(suite STREQUAL "primestruct.compile.run.imports")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "versioned_import_resolution"
                                 SOURCE_FILE "*test_compile_run_imports_versions.h")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "block_and_operator_rewrites"
                                 SOURCE_FILE "*test_compile_run_imports_blocks.h")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "operations_and_collections"
                                 SOURCE_FILE "*test_compile_run_imports_operations.h")
    elseif(suite STREQUAL "primestruct.compile.run.text_filters")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "flag_and_transform_basics"
                                 SOURCE_FILE "*test_compile_run_text_filters_core.h" FIRST 1 LAST 28)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "semantic_transform_rules"
                                 SOURCE_FILE "*test_compile_run_text_filters_core.h" FIRST 29 LAST 53)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "text_transform_rules"
                                 SOURCE_FILE "*test_compile_run_text_filters_core.h" FIRST 54 LAST 84)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "misc_language_and_diagnostics_basics"
                                 SOURCE_FILE "*test_compile_run_text_filters_misc.h" FIRST 1 LAST 18)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "misc_language_and_diagnostics_extended"
                                 SOURCE_FILE "*test_compile_run_text_filters_misc.h" FIRST 19 LAST 35)
    elseif(suite STREQUAL "primestruct.compile.run.examples")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "spinning_cube_pipeline_and_scripts"
                                 SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                 FIRST 1 LAST 23)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "spinning_cube_argument_validation"
                                 SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                 FIRST 24 LAST 47)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "native_window_launcher_and_preflight"
                                 SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                 FIRST 48 LAST 62)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900
                                 SHARD_NAME "metal_pipeline_and_borrow_checker"
                                 SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                 FIRST 63 LAST 68)
    else()
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900)
    endif()
  endforeach()

  # VM compile-run coverage is broader than the current VM runtime surface.
  # Keep the unsupported shards disabled until image IO, GPU dispatch fallback,
  # and the stdlib/experimental map shim paths are implemented for the VM.
  set_tests_properties(
    PrimeStruct_primestruct_compile_run_vm_collections_ir_and_output_modes_basics
    PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics
    PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity
    PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims
    PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence
    PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax
    PrimeStruct_primestruct_compile_run_vm_gpu
    PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution
    PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference
    PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core
    PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops
    PrimeStruct_primestruct_compile_run_native_backend_core
    PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers
    PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity
    PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims
    PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_extended
    PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors
    PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core
    PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_advanced
    PrimeStruct_primestruct_compile_run_native_backend_collections_growth_limits_and_map_access
    PrimeStruct_primestruct_compile_run_imports_operations_and_collections
    PROPERTIES DISABLED TRUE)

  set(PrimeStructRegisteredSuites ${PrimeStructTestSuites} ${PrimeStructCompileRunSuites})
  set(PrimeStructExpectedSuitesFile "${CMAKE_CURRENT_BINARY_DIR}/expected_doctest_suites.txt")
  file(WRITE "${PrimeStructExpectedSuitesFile}" "")
  foreach(suite IN LISTS PrimeStructRegisteredSuites)
    file(APPEND "${PrimeStructExpectedSuitesFile}" "${suite}\n")
  endforeach()
  add_test(
    NAME PrimeStruct_suite_registration
    COMMAND ${CMAKE_COMMAND}
            -DTEST_BINARY=$<TARGET_FILE:PrimeStruct_tests>
            -DEXPECTED_SUITES_FILE=${PrimeStructExpectedSuitesFile}
            -P ${CMAKE_SOURCE_DIR}/cmake/CheckDoctestSuites.cmake
  )
  set_tests_properties(PrimeStruct_suite_registration PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
endif()

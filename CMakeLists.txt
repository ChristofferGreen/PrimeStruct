cmake_minimum_required(VERSION 3.20)
project(PrimeStruct LANGUAGES CXX)

option(PRIMESTRUCT_BUILD_TESTS "Build PrimeStruct tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
target_compile_options(primec_lib PRIVATE -Wall -Wextra -Wpedantic)

add_executable(primec src/main.cpp)
target_link_libraries(primec PRIVATE primec_lib)
target_compile_options(primec PRIVATE -Wall -Wextra -Wpedantic)

add_executable(primevm src/primevm_main.cpp)
target_link_libraries(primevm PRIVATE primec_lib)
target_compile_options(primevm PRIVATE -Wall -Wextra -Wpedantic)

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
  target_compile_options(PrimeStruct_tests PRIVATE -Wall -Wextra -Wpedantic)

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
    set(oneValueArgs TIMEOUT LABEL SHARD_NAME FIRST LAST)
    cmake_parse_arguments(PrimeStructSuite "${options}" "${oneValueArgs}" "" ${ARGN})
    if (NOT PrimeStructSuite_TIMEOUT)
      set(PrimeStructSuite_TIMEOUT 300)
    endif()

    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeStruct_${suiteId}")
    if (PrimeStructSuite_SHARD_NAME)
      set(testName "${testName}_${PrimeStructSuite_SHARD_NAME}")
    endif()

    set(commandArgs "--test-suite=${suite}")
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
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 37)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 38 LAST 73)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 74 LAST 109)
    elseif(suite STREQUAL "primestruct.compile.run.vm.collections")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 84)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 85 LAST 167)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 168 LAST 250)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice04" FIRST 251 LAST 333)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice05" FIRST 334 LAST 416)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice06" FIRST 417 LAST 500)
    elseif(suite STREQUAL "primestruct.compile.run.emitters.cpp")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 45)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 46 LAST 89)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 90 LAST 133)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice04" FIRST 134 LAST 177)
    elseif(suite STREQUAL "primestruct.compile.run.math_conformance")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance" SHARD_NAME "slice01" FIRST 1 LAST 11)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance" SHARD_NAME "slice02" FIRST 12 LAST 21)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance" SHARD_NAME "slice03" FIRST 22 LAST 31)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 600 LABEL "math-conformance" SHARD_NAME "slice04" FIRST 32 LAST 42)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.collections")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 63)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 64 LAST 126)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 127 LAST 189)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice04" FIRST 190 LAST 252)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice05" FIRST 253 LAST 315)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice06" FIRST 316 LAST 377)
    elseif(suite STREQUAL "primestruct.compile.run.native_backend.math_numeric")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 22)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 23 LAST 44)
    elseif(suite STREQUAL "primestruct.compile.run.imports")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 23)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 24 LAST 45)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 46 LAST 67)
    elseif(suite STREQUAL "primestruct.compile.run.text_filters")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 30)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 31 LAST 60)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 61 LAST 90)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice04" FIRST 91 LAST 119)
    elseif(suite STREQUAL "primestruct.compile.run.examples")
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice01" FIRST 1 LAST 23)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice02" FIRST 24 LAST 46)
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900 SHARD_NAME "slice03" FIRST 47 LAST 68)
    else()
      addPrimeStructDoctestSuite("${suite}" TIMEOUT 900)
    endif()
  endforeach()

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

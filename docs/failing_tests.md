# Failing Tests

This file is the live registry for test cases that failed on the most recent
release validation run. When `./scripts/compile.sh --release` or any focused
release test invocation fails, record the failing cases here before starting
new implementation work.

## Workflow

1. Run the release validation path first.
2. Record every failing doctest case here with the exact test case name and the
   command that exposed it.
3. Fix the smallest reproducible failure first.
4. Rerun the smallest relevant release-mode test binary or doctest case.
5. Keep `docs/todo.md` pointed at test-fix work before new feature work.

## Current Failures

- Latest full release gate:
  - Command: `./scripts/compile.sh --release`
  - Result: 141 failing CTest cases out of 1539 run; 74 disabled.
  - Run ended: 2026-06-01 17:40 UTC.
  - Authoritative list: `build-release/Testing/Temporary/LastTestsFailed.log`
  - Authoritative output: `build-release/Testing/Temporary/LastTest.log`

- Release failure clusters still open:
  - IR pipeline validation and conversion shards with semantic-product routing
    expectation drift, including
    `PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50` through
    `PrimeStruct_primestruct_ir_pipeline_validation_cases_821_830` and the
    borrowed/pointer variadic vector conversion tests.
  - Semantics collection shards from
    `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50`
    through
    `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_641_650`,
    plus map constructor and comparison/literal diagnostics.
  - VM/C++ compile-run collection and import shards with stdlib collection
    helper precedence drift, map access invalid IR, and SoA helper diagnostic
    drift.
  - C++ emitter collection helper shards with invalid indirect-address IR,
    missing semantic-product local-auto facts, or shifted diagnostics.
  - VM math shards
    `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_9_9` and
    `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_10_10`
    timeout at 900s.
  - Semantic memory, benchmark, backend registry, and Maybe migration
    harness failures.

- Focused failures reproduced after the latest full gate:
  - Command:
    `build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.comparisons_literals --order-by=file --source-file='*test_semantics_calls_and_flow_comparisons_literals.cpp'`
  - Command:
    `build-release/PrimeStruct_compile_run_tests --test-suite=primestruct.compile.run.vm.collections --order-by=file --source-file='*test_compile_run_vm_collections_array_and_wrapper_shadows.cpp'`
  - Failed cases: 12 failures remain in the VM collection shadow source file,
    mostly around shifted diagnostics plus map access lowering that still
    reaches `VM error: invalid indirect address in IR: 9223372036854775856`.

- Focused failures fixed pending full-gate confirmation:
  - Confirmed fixed in the latest full gate:
    `PrimeStruct_primestruct_ir_pipeline_validation_cases_311_320`,
    `PrimeStruct_primestruct_ir_pipeline_validation_cases_321_330`, and
    `PrimeStruct_primestruct_ir_pipeline_validation_cases_471_480`.
  - Backtrace:
    `build-release/backtrace-ir-validation-321-330.txt`
  - Command:
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_(311_320|321_330|471_480)$'`
  - Confirmed fixed in focused rerun:
    `PrimeStruct_misc_suite_registration` and
    `PrimeStruct_compile_run_suite_registration`.
  - Command:
    `build-release/ctest --output-on-failure -R '^(PrimeStruct_misc_suite_registration|PrimeStruct_compile_run_suite_registration)$'`
  - Confirmed fixed in the latest full gate:
    `PrimeStruct_vector_surface_traces`,
    `PrimeStruct_map_vector_compiler_knowledge_zero_audit`,
    `PrimeStruct_map_surface_strict_audit`, and
    `PrimeStruct_soa_surface_trace_zero_audit`.
  - Command:
    direct `python3 scripts/check_*` audit invocations from the repo root all
    passed after the trace-string cleanup.
  - Confirmed fixed in the latest full gate:
    `primestruct.compile.run.vm.maps`.
  - Command:
    `build-release/PrimeStruct_compile_run_tests --test-suite=primestruct.compile.run.vm.maps --order-by=file --source-file='*test_compile_run_vm_maps.cpp'`
  - Confirmed fixed in focused reruns:
    `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_7_7`
    and
    `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_12_12`
    through
    `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_16_16`.
  - Command:
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_(1_10_7_7|11_20_(12_12|13_13|14_14|15_15|16_16))$'`
  - Confirmed fixed in focused reruns:
    `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_111_120`
    and
    `PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130`.
  - Commands:
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_111_120$'`
    and
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130$'`
  - Linux native-on-Linux mixed compile-run shards are now disabled at CTest
    registration time when the native backend is unavailable.
  - Confirmed fixed in focused rerun:
    `map constructor odd raw argument count uses ordinary argument diagnostics`.
  - Command:
    `build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.comparisons_literals --order-by=file --source-file='*test_semantics_calls_and_flow_comparisons_literals.cpp'`
  - Confirmed fixed in focused reruns:
    `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_24_24`,
    `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_8_8`,
    and `PrimeStruct_primestruct_compile_run_vm_bounds_bounds_and_safety`.
  - Commands:
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_24_24$'`,
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_8_8$'`,
    and
    `build-release/ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_bounds_bounds_and_safety$'`.
  - Confirmed fixed in focused rerun:
    `builtin map at comparisons prefer canonical map access over root at`.
  - Command:
    `build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.comparisons_literals --order-by=file --source-file='*test_semantics_calls_and_flow_comparisons_literals.cpp'`

## Notes

- Add new failures here as soon as a release run exposes them.
- Remove entries only after the corresponding release-mode fix is verified.

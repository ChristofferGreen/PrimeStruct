# Failing Tests

Live registry of test cases that fail on the most recent gate. Historical
triage notes live in `docs/todo_archive/failing_tests_history.md`.

## Workflow

1. Run the release validation path first (`./scripts/compile.sh --release`).
2. `scripts/compile.sh` refreshes the managed block below after full runs.
3. Add every failing case under "Open Failures" before starting new work:
   one bullet per case, ``- `<exact ctest name>`: <location, command, note>``.
4. Fix the smallest reproducible failure first; rerun the smallest relevant
   release-mode doctest case, then the full gate.
5. Delete the bullet once the fix is verified. CTest
   (`PrimeStruct_failing_tests_doc`) fails if a listed case currently passes,
   so a stale entry cannot linger.

## Open Failures

None.

## Current Failures

<!-- compile.sh:failing-tests:start -->
- Last updated: `2026-10-21T21:45:55Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 22`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `30`: `PrimeStruct_native_ui_appkit_smoke`
  - `938`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_11_11`
  - `944`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_17_17`
  - `945`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_18_18`
  - `946`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_19_19`
  - `947`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_20_20`
  - `948`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_21_22_21_21`
  - `1002`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_argv_arrays_77_86_part2_48_48`
  - `1266`: `PrimeStruct_primestruct_compile_run_reflection_codegen_reflection_codegen_23_23`
  - `1523`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_2_2`
  - `1754`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_71_80`
  - `1755`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_81_90`
<!-- compile.sh:failing-tests:end -->

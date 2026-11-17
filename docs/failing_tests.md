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
- Last updated: `2026-11-17T22:45:58Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 22`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `16`: `PrimeStruct_embed_host_calls`
  - `29`: `PrimeStruct_embed_runtime_only`
  - `170`: `PrimeStruct_build_function_steps_generator_function_step`
  - `171`: `PrimeStruct_build_function_steps_two_function_steps_both`
  - `172`: `PrimeStruct_build_function_steps_writes_outside_the_output_directory_faults`
  - `173`: `PrimeStruct_build_function_steps_writes_an_absolute_path`
  - `175`: `PrimeStruct_build_function_steps_does_not_write_its_declared_output`
  - `176`: `PrimeStruct_build_function_steps_runs_again_when_a_declared_input`
  - `177`: `PrimeStruct_build_function_steps_runs_again_when_its_file_or`
  - `178`: `PrimeStruct_build_function_steps_dry_run_marks_a_graph`
  - `263`: `PrimeStruct_primestruct_ir_pipeline_serialization_cases_45_48`
  - `264`: `PrimeStruct_primestruct_ir_pipeline_serialization_cases_49_52`
  - `265`: `PrimeStruct_primestruct_ir_pipeline_serialization_cases_53_54`
<!-- compile.sh:failing-tests:end -->

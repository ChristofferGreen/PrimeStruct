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

- `PrimeStruct_primestruct_program_matrix_parameter_modes`: tests/unit/program_matrix/test_program_matrix_parameter_modes.cpp, case "a runtime error inside a real call stops the program" (added by e52fdaa9d, TODO-5734), fails on native-O0 and native-O2: `primec --emit=native` of the case's program prints the bounds error, then `3` and `99`, and exits 0. The native emitter has no `Trap`, so a runtime error in a real-call callee returns 3 to the caller instead of ending the program; fails the same way at e52fdaa9d without other changes. Tracked by TODO-5743.

## Current Failures

<!-- compile.sh:failing-tests:start -->
- Last updated: `2026-11-08T03:09:54Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 22`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `1896`: `PrimeStruct_primestruct_program_matrix_parameter_modes`
<!-- compile.sh:failing-tests:end -->

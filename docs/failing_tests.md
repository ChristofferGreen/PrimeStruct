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
- Last updated: `2026-11-21T05:24:24Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 22`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `672`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_251_260`
  - `703`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_561_570`
  - `704`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_571_580`
  - `2194`: `PrimeStruct_primestruct_compile_run_native_backend_collections_apple_collections_91_100`
<!-- compile.sh:failing-tests:end -->

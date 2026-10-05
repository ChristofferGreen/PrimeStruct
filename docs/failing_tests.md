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
- Last updated: `2026-10-05T06:49:23Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 8`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `239`: `PrimeStruct_primestruct_semantics_parameters_parameters_1_10`
  - `304`: `PrimeStruct_primestruct_semantics_bindings_pointers_bindings_pointers_41_50`
  - `393`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_91_100`
  - `409`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_251_260`
  - `410`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_261_270`
  - `411`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_271_280`
  - `412`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_281_290`
  - `413`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300`
  - `414`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_301_310`
  - `416`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_321_330`
  - `417`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_331_340`
  - `418`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_341_350`
  - `419`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_351_360`
  - `437`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_531_540`
  - `438`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_541_550`
  - `442`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590`
  - `464`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_801_810`
  - `465`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_811_820`
  - `466`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_821_830`
  - `627`: `PrimeStruct_primestruct_semantics_type_resolution_graph_type_resolution_graph_61_70`
  - `1043`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_389_392`
  - `1044`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_393_396`
  - `1046`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_401_404`
  - `1049`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_413_416`
  - `1050`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_417_420`
  - `1051`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_421_424`
  - `1052`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_425_428`
  - `1054`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_433_436`
  - `1057`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_445_448`
  - `1058`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_449_452`
  - `1059`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_453_456`
  - `1060`: `PrimeStruct_primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_457_460`
  - `1345`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_31_32`
  - `1346`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_33_34`
  - `1347`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_35_36`
  - `1354`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_49_50`
  - `1356`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_53_54`
  - `1357`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_55_56`
  - `1358`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_57_58`
  - `1361`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_63_64`
  - `1364`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_69_70`
  - `1365`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_71_72`
  - `1366`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_73_74`
  - `1371`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_83_84`
  - `1374`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_89_90`
  - `1376`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_part2_93_94`
<!-- compile.sh:failing-tests:end -->

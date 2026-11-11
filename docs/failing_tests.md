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
- Last updated: `2026-11-11T12:33:18Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 22`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `74`: `PrimeStruct_net_http_hello`
  - `75`: `PrimeStruct_net_web_app`
  - `76`: `PrimeStruct_net_todo_web`
  - `77`: `PrimeStruct_net_hello_button`
  - `78`: `PrimeStruct_net`
  - `79`: `PrimeStruct_http`
  - `80`: `PrimeStruct_http_workers`
  - `81`: `PrimeStruct_http_optexe_1_2`
  - `82`: `PrimeStruct_http_optexe_3_4`
  - `83`: `PrimeStruct_http_optexe_5_6`
  - `84`: `PrimeStruct_http_optexe_7_7`
  - `85`: `PrimeStruct_http_loopback`
  - `86`: `PrimeStruct_web_1_2`
  - `87`: `PrimeStruct_web_3_4`
  - `88`: `PrimeStruct_web_5_6`
  - `89`: `PrimeStruct_web_7_8`
  - `90`: `PrimeStruct_web_9_10`
  - `91`: `PrimeStruct_web_11_12`
  - `168`: `PrimeStruct_primestruct_ir_pipeline_conversions_numbers_1_10`
  - `169`: `PrimeStruct_primestruct_ir_pipeline_conversions_numbers_11_20`
  - `434`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_35_36`
  - `435`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_37_38`
  - `454`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_75_76`
  - `613`: `PrimeStruct_primestruct_semantics_calls_flow_numeric_builtins_calls_flow_numeric_builtins_11_15`
  - `916`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_90_92`
  - `923`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_111_113`
  - `924`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_114_116`
  - `925`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_117_119`
  - `926`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_120_122`
  - `927`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_123_125`
  - `928`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_126_128`
  - `929`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_newly_exposed_2026_07_16_129_129`
  - `987`: `PrimeStruct_primestruct_compile_run_vm_core_core_01b_15_19_19_19`
  - `988`: `PrimeStruct_primestruct_compile_run_vm_core_core_01b_20_24`
  - `993`: `PrimeStruct_primestruct_compile_run_vm_core_core_02_53_56`
  - `1003`: `PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68`
  - `1005`: `PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_70_70`
  - `1034`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_7_7`
  - `1035`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_8_8`
  - `1036`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_9_9`
  - `1037`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_10_10`
  - `1038`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_11_11`
  - `1039`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_12_12`
  - `1040`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_13_13`
  - `1041`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_14_14`
  - `1042`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_15_15`
  - `1043`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_16_16`
  - `1044`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_17_17`
  - `1045`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_18_18`
  - `1046`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_19_19`
  - `1047`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_20_20`
  - `1048`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_21_22_21_21`
  - `1049`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_21_22_22_22`
  - `1050`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_23_23`
  - `1051`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_24_24`
  - `1052`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_25_25`
  - `1053`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_26_26`
  - `1273`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_84_85`
  - `1851`: `PrimeStruct_semantic_memory_benchmark`
  - `1852`: `PrimeStruct_semantic_memory_definition_worker_parity`
  - `1853`: `PrimeStruct_semantic_memory_trend`
  - `1855`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_11_20`
  - `1860`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_61_70`
  - `1861`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_71_75`
  - `1862`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_76_80`
  - `1863`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_81_85`
  - `1864`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_86_90`
  - `1865`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_91_95`
  - `1866`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_96_100`
  - `1867`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_101_110`
  - `1871`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_141_150`
  - `1872`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_151_160`
  - `1873`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_161_170`
  - `1875`: `PrimeStruct_primestruct_compile_run_native_backend_core_apple_core_181_190`
  - `1952`: `PrimeStruct_primestruct_compile_run_benchmark_harness`
  - `1953`: `PrimeStruct_primestruct_compile_run_lazy_stdlib_imports`
  - `2053`: `PrimeStruct_primestruct_program_matrix_pick_subjects_13_15`
  - `2066`: `PrimeStruct_primestruct_program_matrix_results_7_9`
  - `2080`: `PrimeStruct_primestruct_program_matrix_tasks_7_9`
  - `2083`: `PrimeStruct_primestruct_program_matrix_text_views_1_2`
<!-- compile.sh:failing-tests:end -->

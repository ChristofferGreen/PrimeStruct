# Failing Tests

This file is the live registry for test cases that failed on the most recent
`scripts/compile.sh` test run. Focused release test invocations should still be
recorded here manually before starting new implementation work.

## Workflow

1. Run the release validation path first.
2. Let `scripts/compile.sh` refresh the managed failure list after full script
   runs.
3. Fix the smallest reproducible failure first.
4. Rerun the smallest relevant release-mode test binary or doctest case.
5. Keep `docs/todo.md` pointed at test-fix work before new feature work.

## Current Failures

<!-- compile.sh:failing-tests:start -->
- Last updated: `2026-06-12T07:59:15Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 11`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `1`: `PrimeStruct_compile_time_facade`
  - `47`: `PrimeStruct_primestruct_ir_pipeline_conversions_core_11_20`
  - `59`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors`
  - `60`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_collection_refs`
  - `61`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_pointer_refs`
  - `62`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps`
  - `63`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_pointer_maps`
  - `64`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_pointer_vectors`
  - `79`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50`
  - `80`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_51_60`
  - `81`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_61_70`
  - `87`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130`
  - `88`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_131_140`
  - `96`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_211_220`
  - `99`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250`
  - `104`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_291_300`
  - `111`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_361_370`
  - `112`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_371_380`
  - `155`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_801_810`
  - `193`: `PrimeStruct_primestruct_semantics_executions_executions_1_10`
  - `196`: `PrimeStruct_primestruct_semantics_transforms_transforms_1_10`
  - `240`: `PrimeStruct_primestruct_semantics_bindings_assignments_bindings_assignments_1_10`
  - `289`: `PrimeStruct_primestruct_semantics_calls_flow_control_calls_flow_control_11_20`
  - `303`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50`
  - `304`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_51_60`
  - `305`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_61_70`
  - `306`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_71_80`
  - `310`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_111_120`
  - `311`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_121_130`
  - `312`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_131_140`
  - `316`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_171_180`
  - `317`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_181_190`
  - `324`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_251_260`
  - `327`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_281_290`
  - `328`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300`
  - `329`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_301_310`
  - `330`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_311_320`
  - `331`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_321_330`
  - `332`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_331_340`
  - `333`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_341_350`
  - `334`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_351_360`
  - `335`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_361_370`
  - `336`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380`
  - `337`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_381_390`
  - `338`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_391_400`
  - `339`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_401_410`
  - `340`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_411_420`
  - `342`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_431_440`
  - `343`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_441_450`
  - `344`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_451_460`
  - `347`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_481_490`
  - `348`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_491_500`
  - `349`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_501_510`
  - `350`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_511_520`
  - `351`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_521_530`
  - `357`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590`
  - `358`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_591_600`
  - `359`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_601_610`
  - `360`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620`
  - `361`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_621_630`
  - `362`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_631_640`
  - `363`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_641_650`
  - `367`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_681_690`
  - `370`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_711_720`
  - `371`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_721_730`
  - `378`: `PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20`
  - `391`: `PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10`
  - `392`: `PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_11_20`
  - `394`: `PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_31_40`
  - `400`: `PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_51_60`
  - `401`: `PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_61_70`
  - `404`: `PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_91_100`
  - `415`: `PrimeStruct_primestruct_semantics_imports_imports_9_9`
  - `458`: `PrimeStruct_primestruct_semantics_imports_imports_52_52`
  - `643`: `PrimeStruct_primestruct_compile_run_smoke_collective_paths_extended_18_18`
  - `644`: `PrimeStruct_primestruct_compile_run_smoke_collective_paths_extended_19_19`
  - `646`: `PrimeStruct_primestruct_compile_run_smoke_collective_paths_extended_21_21`
  - `673`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05`
  - `674`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_6_6`
  - `675`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_7_7`
  - `676`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_8_8`
  - `677`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_9_9`
  - `678`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_10_10`
  - `679`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_11_14_11_11`
  - `680`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_11_14_12_12`
  - `681`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_11_14_13_13`
  - `682`: `PrimeStruct_primestruct_compile_run_vm_core_core_01a_11_14_14_14`
  - `711`: `PrimeStruct_primestruct_compile_run_vm_core_core_03b_76_80`
  - `721`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_9_9`
  - `722`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_10_10`
  - `749`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_7_7`
  - `750`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_8_8`
  - `751`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_9_9`
  - `752`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_10_10`
  - `753`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_11_11`
  - `756`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_14_14`
  - `757`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_15_15`
  - `758`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_16_16`
  - `759`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_17_17`
  - `760`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_18_18`
  - `761`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_19_19`
  - `762`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_20_20`
  - `763`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_21_22_21_21`
  - `764`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_21_22_22_22`
  - `765`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_23_23`
  - `766`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_24_24`
  - `767`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_25_25`
  - `768`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_26_26`
  - `821`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_11_20`
  - `822`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_21_30`
  - `823`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_31_40`
  - `824`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_41_50`
  - `825`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60`
  - `826`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_61_70`
  - `827`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_71_80`
  - `828`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_81_90`
  - `829`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_91_100`
  - `830`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_101_110`
  - `831`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_111_120`
  - `833`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_131_138`
  - `834`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148`
  - `835`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_149_158`
  - `838`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_179_188`
  - `839`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_189_198`
  - `840`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_199_208`
  - `841`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_209_218`
  - `842`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_219_228`
  - `843`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_229_238`
  - `844`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_239_248`
  - `845`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_249_254`
  - `846`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_255_264`
  - `847`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_265_274`
  - `848`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_275_284`
  - `849`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_285_294`
  - `850`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_295_304`
  - `851`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_305_314`
  - `852`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_315_324`
  - `855`: `PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_342_351`
  - `858`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_2_2`
  - `861`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_5_5`
  - `862`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_6_6`
  - `867`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_11_11`
  - `869`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_13_13`
  - `878`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_22_22`
  - `912`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_66_67`
  - `913`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_68_69`
  - `919`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_80_81`
  - `924`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_90_91`
  - `925`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_92_93`
  - `926`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_94_95`
  - `927`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_96_97`
  - `928`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_98_99`
  - `929`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_100_100`
  - `930`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_101_110`
  - `931`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_111_120`
  - `933`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_131_140`
  - `935`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_151_160`
  - `938`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_177_186`
  - `1277`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_1_2`
  - `1278`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_3_4`
  - `1284`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_15_16`
  - `1285`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_17_18`
  - `1288`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_23_24`
  - `1292`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_31_32`
  - `1296`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_39_40`
  - `1433`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_19_19`
  - `1445`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_argument_validation_31_35`
  - `1447`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_argument_validation_41_45`
  - `1448`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_argument_validation_46_50`
  - `1450`: `PrimeStruct_primestruct_compile_run_examples_native_window_launcher_and_preflight_56_56`
  - `1451`: `PrimeStruct_primestruct_compile_run_examples_native_window_launcher_and_preflight_57_57`
  - `1458`: `PrimeStruct_primestruct_compile_run_examples_native_window_launcher_and_preflight_64_64`
  - `1541`: `PrimeStruct_primestruct_compile_run_generic_requirements_generic_requirements_8_8`
  - `1563`: `PrimeStruct_primestruct_stdlib_map_ownership`
  - `1565`: `PrimeStruct_primestruct_ir_pipeline_entry_args`
  - `1566`: `PrimeStruct_primestruct_ir_pipeline_type_resolution_parity`
  - `1568`: `PrimeStruct_primestruct_ir_pipeline_backends_core`
  - `1572`: `PrimeStruct_primestruct_vm_execution_kernel`
  - `1578`: `PrimeStruct_primestruct_semantics_move`
  - `1588`: `PrimeStruct_test_suite_naming`
  - `1589`: `PrimeStruct_vector_surface_traces`
  - `1597`: `PrimeStruct_map_vector_compiler_knowledge_zero_audit`
  - `1599`: `PrimeStruct_map_surface_strict_audit`
  - `1605`: `PrimeStruct_semantic_memory_benchmark`
  - `1606`: `PrimeStruct_semantic_memory_definition_worker_parity`
  - `1607`: `PrimeStruct_semantic_memory_trend`
  - `1608`: `PrimeStruct_primestruct_compile_run_vm_bounds_bounds_and_safety`
  - `1609`: `PrimeStruct_primestruct_compile_run_vm_maps_map_helpers`
  - `1613`: `PrimeStruct_primestruct_compile_run_benchmark_harness`
  - `1614`: `PrimeStruct_misc_suite_registration`
<!-- compile.sh:failing-tests:end -->

## Notes

- The block under `## Current Failures` is managed by `scripts/compile.sh`
  when tests are run. Keep manual notes outside the managed markers.
- Add new failures here as soon as a release run exposes them.
- Remove entries only after the corresponding release-mode fix is verified.

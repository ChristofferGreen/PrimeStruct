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
- Last updated: `2026-06-03T06:13:29Z`
- Build type: `Release`
- Build dir: `build-release`
- Command: `ctest --test-dir build-release --output-on-failure --parallel 11`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `328`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300`
  - `336`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380`
  - `337`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_381_390`
  - `338`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_391_400`
  - `339`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_401_410`
  - `340`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_411_420`
  - `342`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_431_440`
  - `343`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_441_450`
  - `348`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_491_500`
  - `357`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590`
  - `358`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_591_600`
  - `359`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_601_610`
  - `360`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620`
  - `361`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_621_630`
  - `362`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_631_640`
  - `363`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_641_650`
  - `403`: `PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_81_90`
  - `549`: `PrimeStruct_primestruct_text_filters_pipeline_rewrites_cases_31_40`
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
  - `688`: `PrimeStruct_primestruct_compile_run_vm_core_core_01b_20_24`
  - `692`: `PrimeStruct_primestruct_compile_run_vm_core_core_02_45_52`
  - `701`: `PrimeStruct_primestruct_compile_run_vm_core_core_03a_64_66`
  - `703`: `PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68`
  - `706`: `PrimeStruct_primestruct_compile_run_vm_core_core_03b_71_75_71_71`
  - `711`: `PrimeStruct_primestruct_compile_run_vm_core_core_03b_76_80`
  - `722`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_10_10`
  - `820`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_1_10`
  - `821`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_11_20`
  - `822`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_21_30`
  - `825`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60`
  - `826`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_61_70`
  - `827`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_71_80`
  - `828`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_81_90`
  - `830`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_101_110`
  - `831`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_111_120`
  - `834`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148`
  - `835`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_149_158`
  - `836`: `PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_159_168`
  - `850`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_295_304`
  - `851`: `PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_305_314`
  - `855`: `PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_342_351`
  - `856`: `PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_352_352`
  - `858`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_2_2`
  - `860`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_4_4`
  - `861`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_5_5`
  - `862`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_6_6`
  - `871`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_15_15`
  - `873`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_17_17`
  - `878`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_22_22`
  - `879`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_23_23`
  - `882`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_26_26`
  - `883`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_27_27`
  - `884`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_28_28`
  - `885`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_29_29`
  - `886`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_30_30`
  - `889`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_33_33`
  - `892`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_36_36`
  - `895`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_39_39`
  - `896`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_40_40`
  - `897`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_41_41`
  - `899`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_43_43`
  - `905`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_52_53`
  - `912`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_66_67`
  - `913`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_68_69`
  - `914`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_70_71`
  - `920`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_82_83`
  - `924`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_90_91`
  - `925`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_92_93`
  - `926`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_94_95`
  - `927`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_96_97`
  - `928`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_98_99`
  - `929`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_100_100`
  - `930`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_101_110`
  - `932`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_121_130`
  - `933`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_131_140`
  - `935`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_151_160`
  - `936`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_161_170`
  - `937`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_171_176`
  - `938`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_177_186`
  - `1277`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_1_2`
  - `1278`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_3_4`
  - `1281`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_9_10`
  - `1282`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_11_12`
  - `1283`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_13_14`
  - `1284`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_15_16`
  - `1285`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_17_18`
  - `1288`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_23_24`
  - `1291`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_29_30`
  - `1292`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_31_32`
  - `1301`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_49_50`
  - `1302`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_51_52`
  - `1303`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_53_54`
  - `1304`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_55_56`
  - `1305`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_57_58`
  - `1306`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_59_60`
  - `1307`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_61_62`
  - `1308`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_63_64`
  - `1310`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_67_68`
  - `1311`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_69_70`
  - `1312`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_71_72`
  - `1313`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_73_74`
  - `1314`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_75_76`
  - `1317`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_81_81`
  - `1410`: `PrimeStruct_primestruct_compile_run_bindings_bindings_4_4`
  - `1433`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_19_19`
  - `1440`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_26_26`
  - `1560`: `PrimeStruct_primestruct_ir_pipeline_type_resolution_parity`
  - `1563`: `PrimeStruct_primestruct_ir_pipeline_backends_registry`
  - `1583`: `PrimeStruct_vector_surface_traces`
  - `1591`: `PrimeStruct_map_vector_compiler_knowledge_zero_audit`
  - `1593`: `PrimeStruct_map_surface_strict_audit`
  - `1595`: `PrimeStruct_soa_surface_trace_zero_audit`
  - `1600`: `PrimeStruct_semantic_memory_definition_worker_parity`
  - `1601`: `PrimeStruct_semantic_memory_trend`
  - `1603`: `PrimeStruct_primestruct_compile_run_vm_maps_map_helpers`
  - `1604`: `PrimeStruct_primestruct_compile_run_vm_uninitialized`
  - `1605`: `PrimeStruct_primestruct_compile_run_vm_maybe`
  - `1607`: `PrimeStruct_primestruct_compile_run_benchmark_harness`
<!-- compile.sh:failing-tests:end -->

## Notes

- The block under `## Current Failures` is managed by `scripts/compile.sh`
  when tests are run. Keep manual notes outside the managed markers.
- Add new failures here as soon as a release run exposes them.
- Remove entries only after the corresponding release-mode fix is verified.

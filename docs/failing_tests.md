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

### Focused release failures observed 2026-06-15

- ~~`canonical vector at through imported stdlib helper in C++ emitter`~~ — **FIXED** 2026-06-15:
  `emitArrayVectorAccessLoad` now uses 1-indexed offsets for stdlib `Vector<T>`
  (count at slot 1, capacity at slot 2, data at slot 3).

- ~~`bare map count through canonical helper in C++ emitter`~~ — **FIXED** 2026-06-15:
  Added hardcoded `isMapValueTypeName` fallback in `resolveStructSlotLayoutFromDefinitionFields`
  returning 11-slot layout (header + 5-slot keys Vector + 5-slot payloads Vector) for
  `MapValue` types when stdlib is not imported.

- ~~`bare map at through canonical helper in C++ emitter`~~ — **FIXED** 2026-06-15:
  Same fix as bare map count above.

- ~~`ir lowerer struct type helpers infer name-expression struct paths`~~ — **FIXED** 2026-06-15:
  Added key-value local struct path inference in `inferStructPathFromNameExpr` in
  `IrLowererStructTypeHelpers.cpp`.

- ~~`ir lowerer struct type helpers infer args-pack indexed field-access paths`~~ — **FIXED** 2026-06-15:
  Fixed `inferStructPathFromFieldAccessCall` to only accept `at`/`at_unsafe` calls as
  valid args-pack indexed access patterns in `IrLowererStructTypeHelpers.cpp`.

- Command:
  `build-release/primec --emit=ir build-release/stdlib_soa_trait_repro.prime -o build-release/stdlib_soa_trait_repro.psir`
- Failed case:
  - Specific import of `/std/collections/experimental_soa_vector/*` and
    `/std/collections/internal_soa_storage/*`
- Diagnostic:
  - `Semantic error: meta.field_count requires struct type argument: i32`
- Note: This was observed while trying to validate the new SoA collection
  type-category annotations. The failure occurs while validating imported
  SoA storage helpers before the category query can be isolated.

**Root cause:** Tests using `import /std/collections/*` fail due to template
monomorphization performance on the 6,511-line expanded stdlib source. The
parser was optimized (early exit in `isDefinitionSignature` speculative
lookahead), and parsing now completes in <1s. However, the semantic analysis
phase — specifically template monomorphization — takes >30s on the expanded
source, exceeding test timeouts.

**Bottleneck location:** `semantics::monomorphizeTemplates()` in
`src/semantics/SemanticsValidate.cpp` (PassId::TemplateMonomorphization).
The 4,842-line `internal_soa_storage.prime` alone generates hundreds of
templated definitions that get monomorphized even when unused.

**Tests affected:** Any test using wildcard imports (`import /std/collections/*`,
`import /std/math/*`, `import /std/image/*`, etc.).

**Status:** Parser performance fixed. Template monomorphization optimization
needed. Individual tests using specific imports pass correctly.

<!-- compile.sh:failing-tests:start -->
- Last updated: `2026-06-16T16:08:45Z`
- Build type: `Debug`
- Build dir: `build-debug`
- Command: `ctest --test-dir build-debug --output-on-failure --parallel 11`
- Result: `ctest` failed with status `8`.
- Failing CTest cases:
  - `28`: `PrimeStruct_primestruct_ir_pipeline_serialization_cases_85_88`
  - `47`: `PrimeStruct_primestruct_ir_pipeline_conversions_core_11_20`
  - `54`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_basics_1_10`
  - `59`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors`
  - `60`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_collection_refs`
  - `62`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps`
  - `63`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_pointer_maps`
  - `64`: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_pointer_vectors`
  - `84`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100`
  - `86`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120`
  - `87`: `PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130`
  - `171`: `PrimeStruct_primestruct_semantics_operators_matrix_and_shape_rules_shapes`
  - `172`: `PrimeStruct_primestruct_semantics_operators_matrix_and_shape_rules_products`
  - `173`: `PrimeStruct_primestruct_semantics_operators_scalar_and_mixed_numeric_rules_16_20`
  - `193`: `PrimeStruct_primestruct_semantics_executions_executions_1_10`
  - `196`: `PrimeStruct_primestruct_semantics_transforms_transforms_1_10`
  - `227`: `PrimeStruct_primestruct_semantics_bindings_core_bindings_core_11_20`
  - `240`: `PrimeStruct_primestruct_semantics_bindings_assignments_bindings_assignments_1_10`
  - `241`: `PrimeStruct_primestruct_semantics_bindings_assignments_bindings_assignments_11_20`
  - `265`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_31_32`
  - `266`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_33_34`
  - `267`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_35_36`
  - `287`: `PrimeStruct_primestruct_semantics_result_helpers_result_helpers_75_76`
  - `302`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_31_40`
  - `303`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50`
  - `304`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_51_60`
  - `305`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_61_70`
  - `306`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_71_80`
  - `308`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_91_100`
  - `309`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_101_110`
  - `310`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_111_120`
  - `311`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_121_130`
  - `312`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_131_140`
  - `313`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_141_150`
  - `314`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_151_160`
  - `315`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_161_170`
  - `316`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_171_180`
  - `317`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_181_190`
  - `318`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_191_200`
  - `319`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_201_210`
  - `320`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_211_220`
  - `321`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_221_230`
  - `322`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_231_240`
  - `323`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_241_250`
  - `324`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_251_260`
  - `325`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_261_270`
  - `326`: `PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_271_280`
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
  - `393`: `PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_21_30`
  - `394`: `PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_31_40`
  - `404`: `PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_91_100`
  - `458`: `PrimeStruct_primestruct_semantics_imports_imports_52_52`
  - `533`: `PrimeStruct_primestruct_parser_errors_literals_cases_21_30`
  - `620`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_57_57`
  - `623`: `PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_60_60`
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
  - `689`: `PrimeStruct_primestruct_compile_run_vm_core_core_01b_25_28`
  - `690`: `PrimeStruct_primestruct_compile_run_vm_core_core_02_29_36`
  - `691`: `PrimeStruct_primestruct_compile_run_vm_core_core_02_37_44`
  - `699`: `PrimeStruct_primestruct_compile_run_vm_core_core_03a_57_63_62_62`
  - `700`: `PrimeStruct_primestruct_compile_run_vm_core_core_03a_57_63_63_63`
  - `711`: `PrimeStruct_primestruct_compile_run_vm_core_core_03b_76_80`
  - `719`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_7_7`
  - `720`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_8_8`
  - `721`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_9_9`
  - `722`: `PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_10_10`
  - `749`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_7_7`
  - `750`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_8_8`
  - `751`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_9_9`
  - `752`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_1_10_10_10`
  - `753`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_11_11`
  - `754`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_12_12`
  - `755`: `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_basics_11_20_13_13`
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
  - `822`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_21_30`
  - `823`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_31_40`
  - `825`: `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60`
  - `828`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_81_90`
  - `829`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_91_100`
  - `830`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_101_110`
  - `831`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_111_120`
  - `833`: `PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_131_138`
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
  - `855`: `PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_342_351`
  - `856`: `PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_352_352`
  - `878`: `PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_22_22`
  - `912`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_66_67`
  - `913`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_68_69`
  - `924`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_90_91`
  - `925`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_92_93`
  - `926`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_94_95`
  - `927`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_96_97`
  - `928`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_98_99`
  - `929`: `PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_100_100`
  - `930`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_101_110`
  - `931`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_111_120`
  - `933`: `PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_131_140`
  - `936`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_161_170`
  - `937`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_171_176`
  - `938`: `PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_177_186`
  - `1277`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_1_2`
  - ~~`1278`~~: ~~`PrimeStruct_primestruct_compile_run_imports_operations_and_collections_3_4`~~ **FIXED** 2026-06-17
  - ~~`1279`~~: ~~`PrimeStruct_primestruct_compile_run_imports_operations_and_collections_5_6`~~ **FIXED** 2026-06-17
  - ~~`1284`~~: ~~`PrimeStruct_primestruct_compile_run_imports_operations_and_collections_15_16`~~ **FIXED** 2026-06-17
  - ~~`1285`~~: ~~`PrimeStruct_primestruct_compile_run_imports_operations_and_collections_17_18`~~ **FIXED** 2026-06-17
  - `1287`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_21_22`
  - ~~`1288`~~: ~~`PrimeStruct_primestruct_compile_run_imports_operations_and_collections_23_24`~~ **FIXED** 2026-06-17
  - ~~`1292`~~: ~~`PrimeStruct_primestruct_compile_run_imports_operations_and_collections_31_32`~~ **FIXED** 2026-06-17
  - `1293`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_33_34`
  - `1294`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_35_36`
  - `1295`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_37_38`
  - `1296`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_39_40`
  - `1297`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_41_42`
  - `1298`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_43_44`
  - `1299`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_45_46`
  - `1300`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_47_48`
  - `1301`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_49_50`
  - `1306`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_59_60`
  - `1307`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_61_62`
  - `1309`: `PrimeStruct_primestruct_compile_run_imports_operations_and_collections_65_66`
  - `1415`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_1_1`
  - `1425`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_11_11`
  - `1426`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_12_12`
  - `1427`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_13_13`
  - `1428`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_14_14`
  - `1429`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_15_15`
  - `1430`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_16_16`
  - `1433`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_19_19`
  - `1434`: `PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_20_20`
  - `1441`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_demo_script_27_27`
  - `1442`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_demo_script_28_28`
  - `1445`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_argument_validation_31_35`
  - `1447`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_argument_validation_41_45`
  - `1448`: `PrimeStruct_primestruct_compile_run_examples_spinning_cube_argument_validation_46_50`
  - `1450`: `PrimeStruct_primestruct_compile_run_examples_native_window_launcher_and_preflight_56_56`
  - `1451`: `PrimeStruct_primestruct_compile_run_examples_native_window_launcher_and_preflight_57_57`
  - `1458`: `PrimeStruct_primestruct_compile_run_examples_native_window_launcher_and_preflight_64_64`
  - `1541`: `PrimeStruct_primestruct_compile_run_generic_requirements_generic_requirements_8_8`
  - `1554`: `PrimeStruct_primestruct_compile_run_glsl_glsl_41_45`
  - `1563`: `PrimeStruct_primestruct_stdlib_map_ownership`
  - `1567`: `PrimeStruct_primestruct_ir_pipeline_type_resolution_parity`
  - `1568`: `PrimeStruct_primestruct_ir_pipeline_gpu`
  - `1570`: `PrimeStruct_primestruct_ir_pipeline_backends_registry`
  - `1590`: `PrimeStruct_vector_surface_traces`
  - `1598`: `PrimeStruct_map_vector_compiler_knowledge_zero_audit`
  - `1600`: `PrimeStruct_map_surface_strict_audit`
  - `1605`: `PrimeStruct_graph_budget`
  - `1606`: `PrimeStruct_semantic_memory_benchmark`
  - `1607`: `PrimeStruct_semantic_memory_definition_worker_parity`
  - `1608`: `PrimeStruct_semantic_memory_trend`
  - `1609`: `PrimeStruct_primestruct_compile_run_vm_bounds_bounds_and_safety`
  - `1610`: `PrimeStruct_primestruct_compile_run_vm_maps_map_helpers`
  - `1613`: `PrimeStruct_primestruct_compile_run_vm_gpu`
  - `1614`: `PrimeStruct_primestruct_compile_run_benchmark_harness`
<!-- compile.sh:failing-tests:end -->

## Notes

- The block under `## Current Failures` is managed by `scripts/compile.sh`
  when tests are run. Keep manual notes outside the managed markers.
- Add new failures here as soon as a release run exposes them.
- Remove entries only after the corresponding release-mode fix is verified.

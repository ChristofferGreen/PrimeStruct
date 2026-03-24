list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.smoke
  primestruct.compile.run.vm.core
  primestruct.compile.run.vm.collections
  primestruct.compile.run.emitters.cpp
  primestruct.compile.run.glsl
  primestruct.compile.run.native_backend.argv
  primestruct.compile.run.native_backend.control
  primestruct.compile.run.native_backend.pointers
  primestruct.compile.run.native_backend.math_numeric
  primestruct.compile.run.native_backend.collections
  primestruct.compile.run.native_backend.imports
  primestruct.compile.run.reflection_codegen
  primestruct.compile.run.imports
  primestruct.compile.run.text_filters
  primestruct.compile.run.examples
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation_01a"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation_01b"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 6
                                  RANGE_LAST 10)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation_02a"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 11
                                  RANGE_LAST 15)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation_02b"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 16
                                  RANGE_LAST 20)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation_03a"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 21
                                  RANGE_LAST 26)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation_03b"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 27
                                  RANGE_LAST 31)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug_01a"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 32
                                  RANGE_LAST 37)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug_01b"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 38
                                  RANGE_LAST 42)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug_02a"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 43
                                  RANGE_LAST 47)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug_02b"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 48
                                  RANGE_LAST 52)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug_03a"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 53
                                  RANGE_LAST 57)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug_03b"
                                  SOURCE_FILE "*test_compile_run_smoke_core.h"
                                  RANGE_FIRST 58
                                  RANGE_LAST 62)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "collective_paths_core"
                                  SOURCE_FILE "*test_compile_run_smoke_collective.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 11)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "collective_paths_extended"
                                  SOURCE_FILE "*test_compile_run_smoke_collective.h"
                                  RANGE_FIRST 12
                                  RANGE_LAST 22)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "argv_and_cli"
                                  SOURCE_FILE "*test_compile_run_smoke_argv.h"
                                  TOTAL_CASES 25)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01a"
                                  SOURCE_FILE "*test_compile_run_vm_core.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 14)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01b"
                                  SOURCE_FILE "*test_compile_run_vm_core.h"
                                  RANGE_FIRST 15
                                  RANGE_LAST 28)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_02"
                                  SOURCE_FILE "*test_compile_run_vm_core.h"
                                  RANGE_FIRST 29
                                  RANGE_LAST 56)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03a"
                                  SOURCE_FILE "*test_compile_run_vm_core.h"
                                  RANGE_FIRST 57
                                  RANGE_LAST 70)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03b"
                                  SOURCE_FILE "*test_compile_run_vm_core.h"
                                  RANGE_FIRST 71
                                  RANGE_LAST 83)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "bounds_and_safety"
                                  SOURCE_FILE "*test_compile_run_vm_bounds.h"
                                  TOTAL_CASES 20)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "map_helpers"
                                  SOURCE_FILE "*test_compile_run_vm_maps.h"
                                  TOTAL_CASES 21)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "math_helpers"
                                  SOURCE_FILE "*test_compile_run_vm_math.h"
                                  TOTAL_CASES 30)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_basics"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 22)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_emitters"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.h"
                                  RANGE_FIRST 23
                                  RANGE_LAST 44)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_exe_backend"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.h"
                                  RANGE_FIRST 45
                                  RANGE_LAST 66)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_argv_arrays"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.h"
                                  RANGE_FIRST 67
                                  RANGE_LAST 88)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "alias_and_basics"
                                  SOURCE_FILE "*test_compile_run_vm_collections.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 60)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity"
                                  SOURCE_FILE "*test_compile_run_vm_collections.h"
                                  RANGE_FIRST 61
                                  RANGE_LAST 138)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims"
                                  SOURCE_FILE "*test_compile_run_vm_collections.h"
                                  RANGE_FIRST 139
                                  RANGE_LAST 254)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "user_shadow_and_receiver_precedence"
                                  SOURCE_FILE "*test_compile_run_vm_collections.h"
                                  RANGE_FIRST 255
                                  RANGE_LAST 331)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "growth_limits_and_syntax"
                                  SOURCE_FILE "*test_compile_run_vm_collections.h"
                                  RANGE_FIRST 332
                                  RANGE_LAST 352)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "lambda_and_mutator_resolution"
                                  SOURCE_FILE "*test_compile_run_emitters.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 45)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "collection_access_and_alias_forwarding"
                                  SOURCE_FILE "*test_compile_run_emitters.h"
                                  RANGE_FIRST 46
                                  RANGE_LAST 100)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "map_wrapper_and_fallback_inference"
                                  SOURCE_FILE "*test_compile_run_emitters.h"
                                  RANGE_FIRST 101
                                  RANGE_LAST 160)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "numeric_math_and_control_flow_core"
                                  SOURCE_FILE "*test_compile_run_emitters.h"
                                  RANGE_FIRST 161
                                  RANGE_LAST 176)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "numeric_math_and_control_flow_loops"
                                  SOURCE_FILE "*test_compile_run_emitters.h"
                                  RANGE_FIRST 177
                                  RANGE_LAST 192)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.glsl"
                                  TIMEOUT 900
                                  SHARD_PREFIX "glsl"
                                  TOTAL_CASES 61)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.argv"
                                  TIMEOUT 900
                                  SHARD_PREFIX "argv"
                                  TOTAL_CASES 22)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.control"
                                  TIMEOUT 900
                                  SHARD_PREFIX "control"
                                  TOTAL_CASES 29)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.pointers"
                                  TIMEOUT 900
                                  SHARD_PREFIX "pointers"
                                  TOTAL_CASES 16)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.math_numeric"
                                  TIMEOUT 900
                                  SHARD_PREFIX "math_builtins"
                                  SOURCE_FILE "*test_compile_run_native_backend_math_numeric.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 22)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.math_numeric"
                                  TIMEOUT 900
                                  SHARD_PREFIX "numeric_and_boolean_flow"
                                  SOURCE_FILE "*test_compile_run_native_backend_math_numeric.h"
                                  RANGE_FIRST 23
                                  RANGE_LAST 34)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.math_numeric"
                                  TIMEOUT 900
                                  SHARD_PREFIX "conversions_and_rejections"
                                  SOURCE_FILE "*test_compile_run_native_backend_math_numeric.h"
                                  RANGE_FIRST 35
                                  RANGE_LAST 44)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_aliases_and_wrappers"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 63)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 64
                                  RANGE_LAST 141)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 142
                                  RANGE_LAST 180)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims_extended"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 181
                                  RANGE_LAST 218)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims_accessors"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 219
                                  RANGE_LAST 257)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "user_shadow_and_receiver_precedence_core"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 258
                                  RANGE_LAST 296)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "user_shadow_and_receiver_precedence_advanced"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 297
                                  RANGE_LAST 335)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "growth_limits_and_map_access"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.h"
                                  RANGE_FIRST 336
                                  RANGE_LAST 380)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.imports"
                                  TIMEOUT 900
                                  SHARD_PREFIX "imports"
                                  TOTAL_CASES 25)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.reflection_codegen"
                                  TIMEOUT 900
                                  SHARD_PREFIX "reflection_codegen"
                                  TOTAL_CASES 21)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.imports"
                                  TIMEOUT 900
                                  SHARD_PREFIX "versioned_import_resolution"
                                  SOURCE_FILE "*test_compile_run_imports_versions.h"
                                  TOTAL_CASES 22)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.imports"
                                  TIMEOUT 900
                                  SHARD_PREFIX "block_and_operator_rewrites"
                                  SOURCE_FILE "*test_compile_run_imports_blocks.h"
                                  TOTAL_CASES 25)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.imports"
                                  TIMEOUT 900
                                  NO_UNIQUE_TMPDIR
                                  RUN_SERIAL
                                  SHARD_PREFIX "operations_and_collections"
                                  SOURCE_FILE "*test_compile_run_imports_operations.h"
                                  TOTAL_CASES 81)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "flag_and_transform_basics"
                                  SOURCE_FILE "*test_compile_run_text_filters_core.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 28)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "semantic_transform_rules"
                                  SOURCE_FILE "*test_compile_run_text_filters_core.h"
                                  RANGE_FIRST 29
                                  RANGE_LAST 53)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "text_transform_rules"
                                  SOURCE_FILE "*test_compile_run_text_filters_core.h"
                                  RANGE_FIRST 54
                                  RANGE_LAST 84)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "misc_language_and_diagnostics_basics"
                                  SOURCE_FILE "*test_compile_run_text_filters_misc.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 18)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "misc_language_and_diagnostics_extended"
                                  SOURCE_FILE "*test_compile_run_text_filters_misc.h"
                                  RANGE_FIRST 19
                                  RANGE_LAST 35)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "spinning_cube_pipeline_and_scripts"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 23)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "spinning_cube_argument_validation"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                  RANGE_FIRST 24
                                  RANGE_LAST 47)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "native_window_launcher_and_preflight"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                  RANGE_FIRST 48
                                  RANGE_LAST 62)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "metal_pipeline_and_borrow_checker"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.h"
                                  RANGE_FIRST 63
                                  RANGE_LAST 68)

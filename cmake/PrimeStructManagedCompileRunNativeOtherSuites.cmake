list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.native_backend.argv
  primestruct.compile.run.native_backend.control
  primestruct.compile.run.native_backend.pointers
  primestruct.compile.run.native_backend.math_numeric
  primestruct.compile.run.native_backend.collections
  primestruct.compile.run.native_backend.imports
  primestruct.compile.run.reflection_codegen
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.argv"
                                  TIMEOUT 900
                                  SHARD_PREFIX "argv"
                                  TOTAL_CASES 22
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.control"
                                  TIMEOUT 900
                                  SHARD_PREFIX "control"
                                  TOTAL_CASES 29
                                  CASES_PER_SHARD 4)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.pointers"
                                  TIMEOUT 900
                                  SHARD_PREFIX "pointers"
                                  TOTAL_CASES 16
                                  CASES_PER_SHARD 2)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.math_numeric"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "math_builtins"
                                  SOURCE_FILE "*test_compile_run_native_backend_math_numeric.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 22
                                  CASES_PER_SHARD 2)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.math_numeric"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "numeric_and_boolean_flow"
                                  SOURCE_FILE "*test_compile_run_native_backend_math_numeric.cpp"
                                  RANGE_FIRST 23
                                  RANGE_LAST 34
                                  CASES_PER_SHARD 4)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.math_numeric"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "conversions_and_rejections"
                                  SOURCE_FILE "*test_compile_run_native_backend_math_numeric.cpp"
                                  RANGE_FIRST 35
                                  RANGE_LAST 44
                                  CASES_PER_SHARD 2)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_aliases_and_wrappers"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 63
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 64
                                  RANGE_LAST 141
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 142
                                  RANGE_LAST 180
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims_extended"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 181
                                  RANGE_LAST 218)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims_accessors"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 219
                                  RANGE_LAST 257)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "user_shadow_and_receiver_precedence_core"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 258
                                  RANGE_LAST 296
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "user_shadow_and_receiver_precedence_advanced"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 297
                                  RANGE_LAST 335
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "growth_limits_and_map_access"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  RANGE_FIRST 336
                                  RANGE_LAST 380
                                  CASES_PER_SHARD 5)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.imports"
                                  TIMEOUT 900
                                  SHARD_PREFIX "imports"
                                  TOTAL_CASES 25
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.reflection_codegen"
                                  TIMEOUT 900
                                  SHARD_PREFIX "reflection_codegen"
                                  TOTAL_CASES 21
                                  CASES_PER_SHARD 1)

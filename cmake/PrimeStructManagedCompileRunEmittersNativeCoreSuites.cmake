list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.emitters.cpp
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 30
                                  SHARD_PREFIX "lambda_and_mutator_resolution"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 45
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 30
                                  SHARD_PREFIX "collection_access_and_alias_forwarding"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 46
                                  RANGE_LAST 100
                                  CASES_PER_SHARD 2)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 30
                                  SHARD_PREFIX "map_wrapper_and_fallback_inference"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 101
                                  RANGE_LAST 160)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 30
                                  SHARD_PREFIX "numeric_math_and_control_flow_core"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 161
                                  RANGE_LAST 176)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 30
                                  SHARD_PREFIX "numeric_math_and_control_flow_loops"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 177
                                  RANGE_LAST 192)
# TODO-4711: this shard's slowest case measured ~28.8s real wall-clock
# (each compile_run test spawns a fresh ./primec subprocess - see TODO-4710's
# findings on why a single stdlib-importing compile is ~250-300x an
# import-free one). 90s = ~3x that measured worst case, still above the 30s
# ceiling from docs/TestRuntimeOptimization.md. Root-causing the per-compile
# cost itself is TODO-4710/TODO-5230's job, not this leaf's.
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 90
                                  SHARD_PREFIX "emitters_newly_exposed_2026_07_16"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 193
                                  RANGE_LAST 622
                                  CASES_PER_SHARD 10)

list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.native_backend.core
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.core"
                                  TIMEOUT 30
                                  SHARD_PREFIX "core"
                                  SOURCE_FILE "*test_compile_run_native_backend_core_*.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 154
                                  CASES_PER_SHARD 1)

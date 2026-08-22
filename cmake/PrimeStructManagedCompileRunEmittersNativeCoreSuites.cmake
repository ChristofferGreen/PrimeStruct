list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.emitters.cpp
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "lambda_and_mutator_resolution"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 45
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "collection_access_and_alias_forwarding"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 46
                                  RANGE_LAST 100
                                  CASES_PER_SHARD 2)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "map_wrapper_and_fallback_inference"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 101
                                  RANGE_LAST 160)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "numeric_math_and_control_flow_core"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 161
                                  RANGE_LAST 176)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
                                  SHARD_PREFIX "numeric_math_and_control_flow_loops"
                                  SOURCE_FILE "*test_compile_run_emitters_*.cpp"
                                  RANGE_FIRST 177
                                  RANGE_LAST 192)
# TODO-4711: reverted to the original 900s. This entire suite's shards
# (all sharing SOURCE_FILE "*test_compile_run_emitters_*.cpp") are at
# risk of a measurement problem this leaf's real-time data didn't
# account for: test_compile_run_emitters.cpp caches compiled native C++
# artifacts under a content-addressed `.primec_test_cache/` dir inside
# the build directory (see `emittedCppFixtureCacheDir()`), which had
# accumulated a warm cache from many prior manual runs earlier in the
# same session that produced this leaf's "real measured time" data. A
# cold-cache run of a single case in this shard range was independently
# observed taking ~7 minutes wall-clock (a real `cc1plus` invocation
# compiling generated C++ from scratch), vs. the ~0.05s the warm-cache
# data showed for the same case - a >1000x discrepancy invalidating the
# margin this leaf computed. Left at the original safe value rather than
# re-measuring with a deliberately-cleared cache (not attempted this
# round - would need to budget for genuinely multi-minute-per-case
# cold compiles across many shards). `primestruct.compile.run.bindings`
# (cmake/PrimeStructManagedCompileRunImportsTextExamplesSuites.cmake)
# uses the same cache dir and was reverted for the same reason.
addPrimeStructManagedDoctestSuite("primestruct.compile.run.emitters.cpp"
                                  TIMEOUT 900
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

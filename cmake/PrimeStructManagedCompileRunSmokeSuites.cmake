list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.smoke
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_foundation"
                                  SOURCE_FILE "*test_compile_run_smoke_core_*.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 31
                                  CASES_PER_SHARD 1)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_paths_wasm_and_debug"
                                  SOURCE_FILE "*test_compile_run_smoke_core_*.cpp"
                                  RANGE_FIRST 32
                                  RANGE_LAST 62
                                  CASES_PER_SHARD 1)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "collective_paths_core"
                                  SOURCE_FILE "*test_compile_run_smoke_collective.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 11
                                  CASES_PER_SHARD 1)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "collective_paths_extended"
                                  SOURCE_FILE "*test_compile_run_smoke_collective.cpp"
                                  RANGE_FIRST 12
                                  RANGE_LAST 22
                                  CASES_PER_SHARD 1)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.smoke"
                                  TIMEOUT 900
                                  SHARD_PREFIX "argv_and_cli"
                                  SOURCE_FILE "*test_compile_run_smoke_argv.cpp"
                                  TOTAL_CASES 25
                                  CASES_PER_SHARD 1)

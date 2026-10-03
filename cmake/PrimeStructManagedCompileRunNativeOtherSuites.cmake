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
                                  TIMEOUT 30
                                  SHARD_PREFIX "argv"
                                  TOTAL_CASES 17
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.control"
                                  TIMEOUT 30
                                  SHARD_PREFIX "control"
                                  TOTAL_CASES 9
                                  CASES_PER_SHARD 4)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.pointers"
                                  TIMEOUT 30
                                  SHARD_PREFIX "pointers"
                                  TOTAL_CASES 7
                                  CASES_PER_SHARD 2)


addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.collections"
                                  TIMEOUT 30
                                  SHARD_PREFIX "array_slice"
                                  SOURCE_FILE "*test_compile_run_native_backend_collections.cpp"
                                  TOTAL_CASES 1)


addPrimeStructManagedDoctestSuite("primestruct.compile.run.native_backend.imports"
                                  TIMEOUT 30
                                  SHARD_PREFIX "imports"
                                  TOTAL_CASES 24
                                  CASES_PER_SHARD 5)
# TODO-4711: measured ~68.5s real worst case for this shard (single-case
# shards, TOTAL_CASES 23/CASES_PER_SHARD 1); 210s = ~3x margin, still above
# the 30s ceiling (see TODO-4710/TODO-5230 for the per-compile subprocess
# cost root cause).
addPrimeStructManagedDoctestSuite("primestruct.compile.run.reflection_codegen"
                                  TIMEOUT 210
                                  SHARD_PREFIX "reflection_codegen"
                                  TOTAL_CASES 23
                                  CASES_PER_SHARD 1)

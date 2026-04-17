include("${CMAKE_CURRENT_LIST_DIR}/PrimeStructManagedCompileRunSmokeSuites.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/PrimeStructManagedCompileRunVmSuites.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/PrimeStructManagedCompileRunEmittersNativeCoreSuites.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/PrimeStructManagedCompileRunNativeOtherSuites.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/PrimeStructManagedCompileRunImportsTextExamplesSuites.cmake" OPTIONAL)

list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.glsl
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.glsl"
                                  TIMEOUT 900
                                  SHARD_PREFIX "glsl"
                                  TOTAL_CASES 61
                                  CASES_PER_SHARD 5)

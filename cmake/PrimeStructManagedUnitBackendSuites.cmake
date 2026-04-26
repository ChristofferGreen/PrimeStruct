list(APPEND PrimeStructManagedBackendSuites
  primestruct.ir.pipeline.serialization
  primestruct.ir.pipeline.pointers
  primestruct.ir.pipeline.conversions
  primestruct.ir.pipeline.to_cpp
  primestruct.ir.pipeline.to_glsl
  primestruct.ir.pipeline.validation
  primestruct.vm.debug.session
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.serialization"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  RANGE_FIRST 1
  RANGE_LAST 52
  CASES_PER_SHARD 4
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.serialization"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  RANGE_FIRST 53
  RANGE_LAST 64
  CASES_PER_SHARD 2
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.serialization"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  RANGE_FIRST 65
  RANGE_LAST 112
  CASES_PER_SHARD 4
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.pointers"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 32
  CASES_PER_SHARD 3
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_core.h"
  SHARD_PREFIX "core"
  TOTAL_CASES 21
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_numbers.cpp"
  SHARD_PREFIX "numbers"
  TOTAL_CASES 44
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_basics.cpp"
  SHARD_PREFIX "variadic_basics"
  TOTAL_CASES 16
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_results.cpp"
  SHARD_PREFIX "variadic_results"
  TOTAL_CASES 6
  CASES_PER_SHARD 6
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_file_errors.cpp"
  SHARD_PREFIX "variadic_file_errors"
  TOTAL_CASES 6
  CASES_PER_SHARD 6
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_file_handles.cpp"
  SHARD_PREFIX "variadic_file_handles"
  TOTAL_CASES 3
  CASES_PER_SHARD 3
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_borrowed_vectors.cpp"
  SHARD_PREFIX "variadic_borrowed_vectors"
  TOTAL_CASES 7
  CASES_PER_SHARD 7
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_collection_refs.cpp"
  SHARD_PREFIX "variadic_collection_refs"
  TOTAL_CASES 9
  CASES_PER_SHARD 9
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_pointer_refs.cpp"
  SHARD_PREFIX "variadic_pointer_refs"
  TOTAL_CASES 8
  CASES_PER_SHARD 8
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_field_refs_and_maps.cpp"
  SHARD_PREFIX "variadic_field_refs_and_maps"
  TOTAL_CASES 7
  CASES_PER_SHARD 7
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_pointer_maps.cpp"
  SHARD_PREFIX "variadic_pointer_maps"
  TOTAL_CASES 7
  CASES_PER_SHARD 7
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_variadic_pointer_vectors.cpp"
  SHARD_PREFIX "variadic_pointer_vectors"
  TOTAL_CASES 7
  CASES_PER_SHARD 7
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.to_cpp"
  TARGET PrimeStruct_backend_runtime_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 37
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.to_glsl"
  TARGET PrimeStruct_backend_runtime_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 58
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.validation"
  TARGET PrimeStruct_backend_ir_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 863
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.vm.debug.session"
  TARGET PrimeStruct_backend_runtime_tests
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 21
  CASES_PER_SHARD 10
)

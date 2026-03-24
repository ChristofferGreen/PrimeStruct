list(APPEND PrimeStructManagedBackendSuites
  primestruct.ir.pipeline.serialization
  primestruct.ir.pipeline.pointers
  primestruct.ir.pipeline.conversions
  primestruct.ir.pipeline.backends
  primestruct.ir.pipeline.to_cpp
  primestruct.ir.pipeline.to_glsl
  primestruct.ir.pipeline.validation
  primestruct.vm.debug.session
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.serialization"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 112
  CASES_PER_SHARD 4
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.pointers"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 32
  CASES_PER_SHARD 3
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_core.h"
  SHARD_PREFIX "core"
  TOTAL_CASES 21
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_numbers.h"
  SHARD_PREFIX "numbers"
  TOTAL_CASES 44
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_calls_and_args.h"
  SHARD_PREFIX "calls_and_args_first_half"
  RANGE_FIRST 1
  RANGE_LAST 38
  TOTAL_CASES 38
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.conversions"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 600
  SOURCE_FILE "*test_ir_pipeline_conversions_calls_and_args.h"
  SHARD_PREFIX "calls_and_args_second_half"
  RANGE_FIRST 39
  RANGE_LAST 76
  TOTAL_CASES 38
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.backends"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 102
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.to_cpp"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 37
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.to_glsl"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 58
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.ir.pipeline.validation"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 863
  CASES_PER_SHARD 10
)

addPrimeStructManagedDoctestSuite("primestruct.vm.debug.session"
  TARGET PrimeStruct_backend_tests
  NO_UNIQUE_TMPDIR
  LABEL "parallel-safe"
  TIMEOUT 300
  TOTAL_CASES 21
  CASES_PER_SHARD 10
)

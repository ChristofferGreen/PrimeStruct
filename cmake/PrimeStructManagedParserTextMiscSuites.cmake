list(APPEND PrimeStructManagedMiscSuites
  primestruct.dumps.ast_ir
  primestruct.imports.errors
  primestruct.imports.resolver
  primestruct.lexer
  primestruct.semantics.manual
)

addPrimeStructManagedDoctestSuite("primestruct.dumps.ast_ir"
  TARGET PrimeStruct_misc_tests
  TIMEOUT 300
  TOTAL_CASES 43
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.imports.errors"
  TARGET PrimeStruct_misc_tests
  TIMEOUT 300
  TOTAL_CASES 37
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.imports.resolver"
  TARGET PrimeStruct_misc_tests
  TIMEOUT 300
  TOTAL_CASES 38
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.lexer"
  TARGET PrimeStruct_misc_tests
  TIMEOUT 300
  TOTAL_CASES 19
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.semantics.manual"
  TARGET PrimeStruct_misc_tests
  TIMEOUT 300
  TOTAL_CASES 145
  CASES_PER_SHARD 10
)

list(APPEND PrimeStructManagedParserSuites
  primestruct.parser.basic
  primestruct.parser.errors.identifiers
  primestruct.parser.errors.punctuation
  primestruct.parser.errors.literals
  primestruct.parser.errors.named_args
  primestruct.parser.errors.transforms
)

addPrimeStructManagedDoctestSuite("primestruct.parser.basic"
  TARGET PrimeStruct_parser_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 305
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.parser.errors.identifiers"
  TARGET PrimeStruct_parser_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 29
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.parser.errors.punctuation"
  TARGET PrimeStruct_parser_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 17
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.parser.errors.literals"
  TARGET PrimeStruct_parser_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 44
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.parser.errors.named_args"
  TARGET PrimeStruct_parser_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 13
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.parser.errors.transforms"
  TARGET PrimeStruct_parser_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 49
  CASES_PER_SHARD 10
)

list(APPEND PrimeStructManagedTextFilterSuites
  primestruct.text_filters.pipeline.basics
  primestruct.text_filters.pipeline.rewrites
  primestruct.text_filters.pipeline.implicit_utf8
  primestruct.text_filters.pipeline.implicit_i32
  primestruct.text_filters.pipeline.collections
  primestruct.text_filters.helpers
)

addPrimeStructManagedDoctestSuite("primestruct.text_filters.pipeline.basics"
  TARGET PrimeStruct_text_filter_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 17
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.text_filters.pipeline.rewrites"
  TARGET PrimeStruct_text_filter_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 44
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.text_filters.pipeline.implicit_utf8"
  TARGET PrimeStruct_text_filter_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 55
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.text_filters.pipeline.implicit_i32"
  TARGET PrimeStruct_text_filter_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 19
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.text_filters.pipeline.collections"
  TARGET PrimeStruct_text_filter_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 15
  CASES_PER_SHARD 10
)
addPrimeStructManagedDoctestSuite("primestruct.text_filters.helpers"
  TARGET PrimeStruct_text_filter_tests
  TIMEOUT 300
  LABEL "parallel-safe"
  TOTAL_CASES 22
  CASES_PER_SHARD 10
)

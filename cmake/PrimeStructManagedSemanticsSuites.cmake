list(APPEND PrimeStructManagedSemanticsSuites
  primestruct.semantics.parameters
  primestruct.semantics.return_inference
  primestruct.semantics.operators
  primestruct.semantics.math_imports
  primestruct.semantics.tags
  primestruct.semantics.builtins_numeric
  primestruct.semantics.executions
  primestruct.semantics.transforms
  primestruct.semantics.effects
  primestruct.semantics.struct_transforms
  primestruct.semantics.builtin_calls
  primestruct.semantics.bindings.core
  primestruct.semantics.bindings.pointers
  primestruct.semantics.bindings.assignments
  primestruct.semantics.bindings.control_flow
  primestruct.semantics.bindings.struct_defaults
  primestruct.semantics.result_helpers
  primestruct.semantics.calls_flow.control
  primestruct.semantics.calls_flow.collections
  primestruct.semantics.calls_flow.access
  primestruct.semantics.calls_flow.named_args
  primestruct.semantics.calls_flow.numeric_builtins
  primestruct.semantics.calls_flow.comparisons_literals
  primestruct.semantics.calls_flow.effects
  primestruct.semantics.imports
  primestruct.semantics.lambdas
  primestruct.semantics.maybe
  primestruct.semantics.type_resolution_graph
)

set(PrimeStructManagedSemanticsCommon
  TARGET PrimeStruct_semantics_tests
  TIMEOUT 300
  LABEL "parallel-safe"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.parameters"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 30
  SHARD_PREFIX "parameters"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.return_inference"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 18
  SHARD_PREFIX "return_inference"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.operators"
  ${PrimeStructManagedSemanticsCommon}
  SOURCE_FILE "*test_semantics_entry_operators.cpp"
  RANGE_FIRST 1
  RANGE_LAST 5
  SHARD_PREFIX "matrix_and_shape_rules_core"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.operators"
  ${PrimeStructManagedSemanticsCommon}
  SOURCE_FILE "*test_semantics_entry_operators.cpp"
  RANGE_FIRST 6
  RANGE_LAST 10
  SHARD_PREFIX "matrix_and_shape_rules_shapes"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.operators"
  ${PrimeStructManagedSemanticsCommon}
  SOURCE_FILE "*test_semantics_entry_operators.cpp"
  RANGE_FIRST 11
  RANGE_LAST 15
  SHARD_PREFIX "matrix_and_shape_rules_products"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.operators"
  ${PrimeStructManagedSemanticsCommon}
  SOURCE_FILE "*test_semantics_entry_operators.cpp"
  RANGE_FIRST 16
  RANGE_LAST 29
  CASES_PER_SHARD 5
  SHARD_PREFIX "scalar_and_mixed_numeric_rules"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.math_imports"
  TARGET PrimeStruct_semantics_tests
  TIMEOUT 900
  LABEL "parallel-safe"
  TOTAL_CASES 39
  CASES_PER_SHARD 4
  SHARD_PREFIX "math_imports"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.tags"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 17
  SHARD_PREFIX "tags"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.builtins_numeric"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 22
  CASES_PER_SHARD 5
  SHARD_PREFIX "builtins_numeric"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.executions"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 26
  SHARD_PREFIX "executions"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.transforms"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 80
  SHARD_PREFIX "transforms"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.effects"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 12
  SHARD_PREFIX "effects"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.struct_transforms"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 152
  SHARD_PREFIX "struct_transforms"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.builtin_calls"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 35
  SHARD_PREFIX "builtin_calls"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.bindings.core"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 53
  SHARD_PREFIX "bindings_core"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.bindings.pointers"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 77
  SHARD_PREFIX "bindings_pointers"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.bindings.assignments"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 22
  SHARD_PREFIX "bindings_assignments"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.bindings.control_flow"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 40
  SHARD_PREFIX "bindings_control_flow"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.bindings.struct_defaults"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 28
  SHARD_PREFIX "bindings_struct_defaults"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.result_helpers"
  TARGET PrimeStruct_semantics_tests
  TIMEOUT 600
  LABEL "parallel-safe"
  TOTAL_CASES 76
  CASES_PER_SHARD 2
  SHARD_PREFIX "result_helpers"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.control"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 102
  SHARD_PREFIX "calls_flow_control"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.collections"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 771
  SHARD_PREFIX "calls_flow_collections"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.access"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 48
  SHARD_PREFIX "calls_flow_access"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.named_args"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 15
  SHARD_PREFIX "calls_flow_named_args"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.numeric_builtins"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 33
  CASES_PER_SHARD 5
  SHARD_PREFIX "calls_flow_numeric_builtins"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.comparisons_literals"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 40
  SHARD_PREFIX "calls_flow_comparisons_literals"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.calls_flow.effects"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 118
  SHARD_PREFIX "calls_flow_effects"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.imports"
  TARGET PrimeStruct_semantics_tests
  TIMEOUT 900
  LABEL "parallel-safe"
  TOTAL_CASES 52
  CASES_PER_SHARD 1
  SHARD_PREFIX "imports"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.lambdas"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 12
  SHARD_PREFIX "lambdas"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.maybe"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 11
  SHARD_PREFIX "maybe"
)

addPrimeStructManagedDoctestSuite(
  "primestruct.semantics.type_resolution_graph"
  ${PrimeStructManagedSemanticsCommon}
  TOTAL_CASES 18
  SHARD_PREFIX "type_resolution_graph"
)

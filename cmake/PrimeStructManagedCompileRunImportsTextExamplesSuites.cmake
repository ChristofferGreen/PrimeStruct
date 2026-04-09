list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.imports
  primestruct.compile.run.text_filters
  primestruct.compile.run.bindings
  primestruct.compile.run.examples
  primestruct.compile.run.math_conformance
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.imports"
                                  TIMEOUT 900
                                  SHARD_PREFIX "versioned_import_resolution"
                                  SOURCE_FILE "*test_compile_run_imports_versions.h"
                                  TOTAL_CASES 22
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.imports"
                                  TIMEOUT 900
                                  SHARD_PREFIX "block_and_operator_rewrites"
                                  SOURCE_FILE "*test_compile_run_imports_blocks.h"
                                  TOTAL_CASES 25
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.imports"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "operations_and_collections"
                                  SOURCE_FILE "*test_compile_run_imports_operations.h"
                                  TOTAL_CASES 81
                                  CASES_PER_SHARD 2)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "flag_and_transform_basics"
                                  SOURCE_FILE "*test_compile_run_text_filters_core.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 28
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "semantic_transform_rules"
                                  SOURCE_FILE "*test_compile_run_text_filters_core.h"
                                  RANGE_FIRST 29
                                  RANGE_LAST 53)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "text_transform_rules"
                                  SOURCE_FILE "*test_compile_run_text_filters_core.h"
                                  RANGE_FIRST 54
                                  RANGE_LAST 84
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "misc_language_and_diagnostics_basics"
                                  SOURCE_FILE "*test_compile_run_text_filters_misc.h"
                                  RANGE_FIRST 1
                                  RANGE_LAST 18
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.text_filters"
                                  TIMEOUT 900
                                  SHARD_PREFIX "misc_language_and_diagnostics_extended"
                                  SOURCE_FILE "*test_compile_run_text_filters_misc.h"
                                  RANGE_FIRST 19
                                  RANGE_LAST 35
                                  CASES_PER_SHARD 2)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.bindings"
                                  TIMEOUT 900
                                  SHARD_PREFIX "bindings"
                                  TOTAL_CASES 8
                                  CASES_PER_SHARD 1)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "examples_ir_and_spinning_cube"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 26
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "spinning_cube_demo_script"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.cpp"
                                  RANGE_FIRST 27
                                  RANGE_LAST 30
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "spinning_cube_argument_validation"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.cpp"
                                  RANGE_FIRST 31
                                  RANGE_LAST 55
                                  CASES_PER_SHARD 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "native_window_launcher_and_preflight"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.cpp"
                                  RANGE_FIRST 56
                                  RANGE_LAST 71
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.examples"
                                  TIMEOUT 900
                                  SHARD_PREFIX "metal_pipeline_and_borrow_checker"
                                  SOURCE_FILE "*test_compile_run_bindings_and_examples.cpp"
                                  RANGE_FIRST 72
                                  RANGE_LAST 93
                                  CASES_PER_SHARD 1)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "harness_and_docs"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 4)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "scalar_basics"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 5
                                  RANGE_LAST 8
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "trig_quadrant_axes"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 9
                                  RANGE_LAST 9)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "trig_quadrant_symmetries"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 10
                                  RANGE_LAST 10)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "trig_quadrant_range_reduction"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 11
                                  RANGE_LAST 11)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_parser_helpers"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 12
                                  RANGE_LAST 13)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "trig_basics"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 14
                                  RANGE_LAST 14)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "inverse_trig_and_atan2"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 15
                                  RANGE_LAST 15)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "hyperbolic"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 16
                                  RANGE_LAST 16)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "inverse_hyperbolic"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 17
                                  RANGE_LAST 17)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "exp_and_log"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 18
                                  RANGE_LAST 18)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "exp_log_domains"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 19
                                  RANGE_LAST 19)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float64_basics"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 20
                                  RANGE_LAST 20)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float64_inverse_trig_logs"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 21
                                  RANGE_LAST 21)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float64_hyperbolic"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 22
                                  RANGE_LAST 22)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float64_grids"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 23
                                  RANGE_LAST 23)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float64_rounding"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 24
                                  RANGE_LAST 24)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "roots_rounding_and_misc"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 25
                                  RANGE_LAST 27
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "stress_grid"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 28
                                  RANGE_LAST 28)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_baseline_trigonometric"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 29
                                  RANGE_LAST 29)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_baseline_transcendental"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 30
                                  RANGE_LAST 30)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_baseline_composition"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 31
                                  RANGE_LAST 31)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_grid_sin"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 32
                                  RANGE_LAST 32)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_grid_cos"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 33
                                  RANGE_LAST 33)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_grid_exp_log"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 34
                                  RANGE_LAST 34)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "float_grid_hypot"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 35
                                  RANGE_LAST 35)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "native_approximation_limits"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 36
                                  RANGE_LAST 36)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "heavy_trig_workload"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 37
                                  RANGE_LAST 37)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "heavy_exp_log_workload"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 38
                                  RANGE_LAST 38)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "array_dense_and_deterministic"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 39
                                  RANGE_LAST 41
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "deterministic_and_conversions"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 42
                                  RANGE_LAST 44
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.math_conformance"
                                  TIMEOUT 600
                                  LABEL "math-conformance"
                                  SHARD_PREFIX "policy_and_edge_cases"
                                  SOURCE_FILE "*test_compile_run_math_conformance.cpp"
                                  RANGE_FIRST 45
                                  RANGE_LAST 49
                                  CASES_PER_SHARD 1)

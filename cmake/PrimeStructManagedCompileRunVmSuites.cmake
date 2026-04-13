list(APPEND PrimeStructManagedCompileRunSuites
  primestruct.compile.run.vm.core
  primestruct.compile.run.vm.collections
  primestruct.compile.run.vm.math
  primestruct.compile.run.vm.outputs
)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01a_01_05"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 5)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01a_06_10"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 6
                                  RANGE_LAST 10
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01a_11_14"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 11
                                  RANGE_LAST 14
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01b_15_19"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 15
                                  RANGE_LAST 19
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01b_20_24"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 20
                                  RANGE_LAST 24)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_01b_25_28"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 25
                                  RANGE_LAST 28)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_02_29_36"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 29
                                  RANGE_LAST 36)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_02_37_44"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 37
                                  RANGE_LAST 44)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_02_45_52"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 45
                                  RANGE_LAST 52)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_02_53_56"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 53
                                  RANGE_LAST 56)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03a_57_63"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 57
                                  RANGE_LAST 63
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03a_64_66"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 64
                                  RANGE_LAST 66)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03a_67_70"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 67
                                  RANGE_LAST 70
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03b_71_75"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 71
                                  RANGE_LAST 75
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03b_76_80"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 76
                                  RANGE_LAST 80)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.core"
                                  TIMEOUT 900
                                  SHARD_PREFIX "core_03b_81_83"
                                  SOURCE_FILE "*test_compile_run_vm_core_*.cpp"
                                  RANGE_FIRST 81
                                  RANGE_LAST 83)

addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.math"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "math_helpers_1_10"
                                  SOURCE_FILE "*test_compile_run_vm_math.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 10
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.math"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "math_helpers_11_20"
                                  SOURCE_FILE "*test_compile_run_vm_math.cpp"
                                  RANGE_FIRST 11
                                  RANGE_LAST 20
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.math"
                                  TIMEOUT 900
                                  RUN_SERIAL
                                  SHARD_PREFIX "math_helpers_21_30"
                                  SOURCE_FILE "*test_compile_run_vm_math.cpp"
                                  RANGE_FIRST 21
                                  RANGE_LAST 30
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_basics_1_10"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 10
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_basics_11_20"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 11
                                  RANGE_LAST 20
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_basics_21_22"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 21
                                  RANGE_LAST 22
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_emitters_23_32"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 23
                                  RANGE_LAST 32
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_emitters_33_44"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 33
                                  RANGE_LAST 44)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_exe_backend_45_55"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 45
                                  RANGE_LAST 55
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_exe_backend_56_66"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 56
                                  RANGE_LAST 66
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_argv_arrays_67_76"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 67
                                  RANGE_LAST 76
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_argv_arrays_77_86"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 77
                                  RANGE_LAST 86
                                  CASES_PER_SHARD 1)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.outputs"
                                  TIMEOUT 900
                                  SHARD_PREFIX "ir_and_output_modes_argv_arrays_87_88"
                                  SOURCE_FILE "*test_compile_run_vm_outputs.cpp"
                                  RANGE_FIRST 87
                                  RANGE_LAST 88)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "alias_and_basics"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 1
                                  RANGE_LAST 60)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_61_70"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 61
                                  RANGE_LAST 70)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_71_80"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 71
                                  RANGE_LAST 80)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_81_90"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 81
                                  RANGE_LAST 90)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_91_100"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 91
                                  RANGE_LAST 100)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_101_110"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 101
                                  RANGE_LAST 110)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_111_120"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 111
                                  RANGE_LAST 120)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_121_130"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 121
                                  RANGE_LAST 130)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "templated_wrapper_parity_131_138"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 131
                                  RANGE_LAST 138)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "stdlib_collection_shims"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 139
                                  RANGE_LAST 254)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "user_shadow_and_receiver_precedence"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 255
                                  RANGE_LAST 331)
addPrimeStructManagedDoctestSuite("primestruct.compile.run.vm.collections"
                                  TIMEOUT 900
                                  SHARD_PREFIX "growth_limits_and_syntax"
                                  SOURCE_FILE "*test_compile_run_vm_collections.cpp"
                                  RANGE_FIRST 332
                                  RANGE_LAST 352)

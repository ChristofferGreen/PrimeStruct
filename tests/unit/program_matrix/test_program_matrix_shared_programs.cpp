#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.shared_programs");

// Programs that the vm and native compile-run suites used to carry twice (TODO-5466,
// scripts/migrate_compile_run_cases.py): one case runs the program on vm-O2 and
// native-O2 and checks the shared exit code.

TEST_CASE("runs vm with heap alloc intrinsic") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_heap_alloc_intrinsic_0";
  program.source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  program.exitCode = 9;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with heap free intrinsic") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_heap_free_intrinsic_1";
  program.source = R"(
[unsafe return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [i32] value{dereference(ptr)}
  /std/intrinsics/memory/free(ptr)
  return(value)
}
)";
  program.exitCode = 9;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with heap realloc intrinsic") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_heap_realloc_intrinsic_2";
  program.source = R"(
[unsafe return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  program.exitCode = 13;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with checked memory at intrinsic") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_checked_memory_at_intrinsic_3";
  program.source = R"(
[unsafe return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  program.exitCode = 13;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with unchecked memory at intrinsic") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_unchecked_memory_at_intrinsic_4";
  program.source = R"(
[unsafe return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  program.exitCode = 13;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with import alias") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_import_alias_5";
  program.source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  program.exitCode = 5;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with multiple imports") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_multiple_imports_6";
  program.source = R"(
import /util, /std/math/*
namespace util {
  [public return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  program.exitCode = 8;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with qualified math names") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_qualified_math_names_7";
  program.source = R"(
[return<int>]
main() {
  [i32] a{/std/math/abs(-5i32)}
  [i32] b{/std/math/sign(-5i32)}
  [i32] c{/std/math/min(7i32, 2i32)}
  [i32] d{/std/math/max(7i32, 2i32)}
  [i32] e{convert<int>(/std/math/pi)}
  return(plus(plus(plus(a, b), plus(c, d)), e))
}
)";
  program.exitCode = 16;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with Maybe some and pick") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_maybe_some_and_pick_8";
  program.source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] value{[some] 2i32}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  program.exitCode = 2;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with pick used as a bare statement") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_pick_used_as_a_bare_statement_9";
  program.source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32>] value{[some] 5i32}
  [i32 mut] result{-99i32}
  pick(value) {
    none {
      assign(result, -1i32)
    }
    some(v) {
      assign(result, v)
    }
  }
  return(result)
}
)";
  program.exitCode = 5;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with mutable pick arm binding assigned through") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_mutable_pick_arm_binding_assigned_through_10";
  program.source = R"(
import /std/maybe/*

[return<int>]
main() {
  [Maybe<i32> mut] value{[some] 5i32}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      assign(v, plus(v, 1i32))
      return(v)
    }
  })
}
)";
  program.exitCode = 6;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with uninitialized local storage") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_uninitialized_local_storage_11";
  program.source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 3i32)
  drop(storage)
  init(storage, 5i32)
  return(take(storage))
}
)";
  program.exitCode = 5;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with pointer-backed uninitialized storage") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_pointer_backed_uninitialized_storage_12";
  program.source = R"(
[unsafe effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<i32>>] ptr{/std/intrinsics/memory/alloc<uninitialized<i32>>(1i32)}
  init(dereference(ptr), 7i32)
  [i32] out{take(dereference(ptr))}
  /std/intrinsics/memory/free(ptr)
  return(out)
}
)";
  program.exitCode = 7;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with reference-backed uninitialized storage") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_reference_backed_uninitialized_storage_13";
  program.source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] ref{location(storage)}
  init(dereference(ref), 7i32)
  return(take(dereference(ref)))
}
)";
  program.exitCode = 7;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("runs vm with pointer-backed uninitialized struct storage") {
  program_matrix::ProgramCase program;
  program.name = "runs_vm_with_pointer_backed_uninitialized_struct_storage_14";
  program.source = R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[unsafe effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<Pair>>] ptr{/std/intrinsics/memory/alloc<uninitialized<Pair>>(1i32)}
  init(dereference(ptr), Pair{3i32, 9i32})
  [Pair] value{take(dereference(ptr))}
  /std/intrinsics/memory/free(ptr)
  return(value.right)
}
)";
  program.exitCode = 9;
  program.onlyConfigs = {"vm-O2", "native-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("vector count and capacity read a vector field") {
  program_matrix::ProgramCase program;
  program.name = "vector_count_and_capacity_read_a_vector_field_15";
  program.source = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Bag mut] bag{Bag{}}
  vectorPush<i32>(bag.items, 4i32)
  vectorPush<i32>(bag.items, 5i32)
  vectorPush<i32>(bag.items, 6i32)
  print_line(vectorCount<i32>(bag.items))
  print_line(vectorCapacity<i32>(bag.items))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

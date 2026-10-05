#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.lifecycle");

// Destructors run automatically (docs/spec/value-lifecycle.md): an owning local is destroyed
// when its scope ends on every exit path, unless its value was moved or returned; field
// initializers and assignments copy from places so no two values own the same storage.

TEST_CASE("Destroy runs at scope end in reverse order, never for moved or returned values") {
  program_matrix::ProgramCase program;
  program.name = "destroy_scope_end";
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<void>]
scoped() {
  [Noisy] first{Noisy{1i32, 0i64}}
  [Noisy] second{Noisy{2i32, 0i64}}
  if(true) {
    [Noisy] inner{Noisy{3i32, 0i64}}
  }
}

[effects(io_out) return<i32>]
early([bool] leave) {
  [Noisy] guard{Noisy{4i32, 0i64}}
  if(leave) {
    return(1i32)
  }
  [Noisy] later{Noisy{5i32, 0i64}}
  return(2i32)
}

[effects(io_out) return<void>]
take([Noisy move] value) {
}

[effects(io_out) return<Noisy>]
make() {
  [Noisy] made{Noisy{6i32, 0i64}}
  return(made)
}

[effects(io_out) return<int>]
main() {
  scoped()
  print_line(100i32)
  early(true)
  print_line(101i32)
  early(false)
  print_line(102i32)
  [Noisy] moved{Noisy{7i32, 0i64}}
  take(move(moved))
  print_line(103i32)
  for([i32 mut] i{0i32}, i < 2i32, i++) {
    [Noisy] each{Noisy{8i32, 0i64}}
  }
  print_line(104i32)
  [Noisy] got{make()}
  print_line(105i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n2\n1\n100\n4\n101\n5\n4\n102\n103\n8\n8\n104\n105\n6\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a return from a nested block destroys the enclosing locals") {
  program_matrix::ProgramCase program;
  program.name = "destroy_nested_return";
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}


[effects(io_out) return<i32>]
nested_return() {
  [Noisy] outer{Noisy{1i32, 0i64}}
  if(true) {
    [Noisy] inner{Noisy{2i32, 0i64}}
    return(1i32)
  }
  return(0i32)
}

[effects(io_out) return<int>]
main() {
  nested_return()
  print_line(100i32)
  for([i32 mut] i{0i32}, i < 3i32, i++) {
    [Noisy] each{Noisy{3i32, 0i64}}
    if(i == 1i32) {
      [Noisy] deep{Noisy{4i32, 0i64}}
    }
  }
  print_line(101i32)
  if(true) {
    [Noisy] top{Noisy{5i32, 0i64}}
    return(0i32)
  }
  return(1i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n1\n100\n3\n4\n3\n3\n101\n5\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a struct field initialized from a local owns its own copy") {
  program_matrix::ProgramCase program;
  program.name = "destroy_constructor_field_copy";
  program.source = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
  [i32 mut] tag{0i32}
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] items{vector<i32>()}
  vectorPush<i32>(items, 4i32)
  [Bag] bag{Bag{items, 1i32}}
  print_line(vectorCount<i32>(bag.items))
  print_line(vectorCount<i32>(items))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("assigning an owning value destroys the old one and copies the new one") {
  program_matrix::ProgramCase program;
  program.name = "destroy_owned_assignment";
  program.source = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
  [i32 mut] tag{0i32}
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] a{vector<i32>()}
  vectorPush<i32>(a, 4i32)
  [Vector<i32> mut] b{vector<i32>()}
  assign(b, a)
  vectorPush<i32>(b, 5i32)
  print_line(vectorCount<i32>(a))
  print_line(vectorCount<i32>(b))
  [Bag mut] bag{Bag{}}
  assign(bag.items, a)
  print_line(vectorCount<i32>(bag.items))
  b = vector<i32>()
  print_line(vectorCount<i32>(b))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n2\n1\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

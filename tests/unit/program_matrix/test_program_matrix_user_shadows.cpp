#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.user_shadows");

// A user definition can take the place of a collection helper or constructor spelling
// (docs/spec/type-system.md, rooted same-path shadows). The same program gives the same output on every backend.

TEST_CASE("rooted vector count and capacity shadows win for calls and not for methods") {
  program_matrix::ProgramCase program;
  program.name = "shadow_rooted_vector_helpers";
  program.source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  print_line(count(values))
  print_line(values.count())
  print_line(capacity(values))
  print_line(values.capacity())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "97\n2\n77\n2\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("user array at and at_unsafe methods shadow the builtin ones") {
  program_matrix::ProgramCase program;
  program.name = "shadow_array_at_methods";
  program.source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(63i32)
}

[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[effects(io_out), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  print_line(values.at(1i32))
  print_line(values.at_unsafe(1i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "63\n86\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("user functions named push, vector and map are ordinary calls") {
  program_matrix::ProgramCase program;
  program.name = "shadow_builtin_names_calls";
  program.source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[effects(io_out), return<int>]
main() {
  print_line(push(4i32, 3i32))
  print_line(vector([value] 9i32))
  print_line(map([key] 4i32, [value] 6i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "7\n9\n10\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("user vector and map functions take a block") {
  program_matrix::ProgramCase program;
  program.name = "shadow_builtin_names_blocks";
  program.source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[effects(io_out), return<int>]
main() {
  [i32 mut] block_vector{0i32}
  [i32 mut] block_map{0i32}
  vector(9i32) {
    assign(block_vector, 4i32)
  }
  map(4i32, 6i32) {
    assign(block_map, 7i32)
  }
  print_line(block_vector)
  print_line(block_map)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "4\n7\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a block-form call and a value call of one user function share a body") {
  program_matrix::ProgramCase program;
  program.name = "user_call_block_and_value_calls";
  program.source = R"(
[return<int>]
twice([i32] value) {
  return(plus(value, value))
}

[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[effects(io_out), return<int>]
main() {
  [i32 mut] seen{0i32}
  twice(9i32) {
    assign(seen, 4i32)
  }
  print_line(twice(5i32))
  print_line(seen)
  [i32] kept{twice(7i32) { assign(seen, 6i32) }}
  print_line(kept)
  print_line(seen)
  vector(9i32) {
    assign(seen, 8i32)
  }
  map(4i32, 6i32) {
    assign(seen, plus(seen, 1i32))
  }
  print_line(vector(3i32))
  print_line(map(1i32, 2i32))
  print_line(seen)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "10\n4\n14\n6\n3\n3\n9\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

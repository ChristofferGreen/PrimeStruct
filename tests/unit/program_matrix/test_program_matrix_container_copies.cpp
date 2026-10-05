#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.container_copies");

// Copying a container (docs/spec/value-lifecycle.md, Copies) gives the copy its own storage:
// a mutable binding initialized from an existing container, a copy parameter, and the
// elements and struct fields inside them run their Copy helpers. TODO-5487.

TEST_CASE("mutable vector binding copies its elements") {
  program_matrix::ProgramCase program;
  program.name = "vector_binding_copy";
  program.source = R"(
import /std/collections/*

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] a{vector<i32>()}
  vectorPush<i32>(a, 1i32)
  [Vector<i32> mut] b{a}
  vectorPush<i32>(b, 3i32)
  print_line(vectorCount<i32>(a))
  print_line(vectorCount<i32>(b))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n2\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("copied vector survives destroying the original") {
  program_matrix::ProgramCase program;
  program.name = "vector_copy_destroy_original";
  program.source = R"(
import /std/collections/*

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] a{vector<i32>()}
  vectorPush<i32>(a, 5i32)
  vectorPush<i32>(a, 6i32)
  [Vector<i32> mut] b{a}
  [Vector<i32> mut] c{b}
  b.Destroy()
  print_line(vectorCount<i32>(b))
  print_line(vectorAt<i32>(c, 1i32))
  c.Destroy()
  print_line(vectorCount<i32>(c))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n6\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("copies of nested vectors, struct fields and maps are independent") {
  program_matrix::ProgramCase program;
  program.name = "nested_container_copies";
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
  vectorPush<i32>(a, 1i32)
  vectorPush<i32>(a, 2i32)
  [Vector<i32> mut] b{a}
  vectorPush<i32>(b, 3i32)
  print_line(vectorCount<i32>(a))
  print_line(vectorCount<i32>(b))
  [Vector<Vector<i32>> mut] outer{vector<Vector<i32>>()}
  vectorPush<Vector<i32>>(outer, a)
  [Vector<Vector<i32>> mut] outerCopy{outer}
  [Vector<i32> mut] inner{vectorAt<Vector<i32>>(outerCopy, 0i32)}
  print_line(vectorCount<i32>(inner))
  [Bag mut] bag{Bag{}}
  vectorPush<i32>(bag.items, 7i32)
  [Bag mut] bag2{bag}
  vectorPush<i32>(bag2.items, 8i32)
  print_line(vectorCount<i32>(bag.items))
  print_line(vectorCount<i32>(bag2.items))
  [map<i32, i32> mut] m{map<i32, i32>()}
  m.insert(1i32, 10i32)
  m.insert(2i32, 20i32)
  m.insert(1i32, 11i32)
  print_line(m.count())
  print_line(m.at(1i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n3\n2\n1\n2\n2\n11\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("copy mut vector parameter does not write back") {
  program_matrix::ProgramCase program;
  program.name = "vector_copy_parameter";
  program.source = R"(
import /std/collections/*

[effects(heap_alloc) return<i32>]
grow_copy([Vector<i32> copy mut] values) {
  vectorPush<i32>(values, 9i32)
  vectorPush<i32>(values, 10i32)
  return(vectorCount<i32>(values))
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] a{vector<i32>()}
  vectorPush<i32>(a, 5i32)
  print_line(grow_copy(a))
  print_line(vectorCount<i32>(a))
  print_line(vectorAt<i32>(a, 0i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n1\n5\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("ring buffer copy owns its elements") {
  program_matrix::ProgramCase program;
  program.name = "ring_buffer_copy";
  program.source = R"(
import /std/collections/ring_buffer/*

[unsafe effects(io_out heap_alloc) return<int>]
main() {
  [RingBuffer<i32> mut] ring{ring_buffer<i32>(2i32)}
  push<i32>(ring, 1i32)
  push<i32>(ring, 2i32)
  push<i32>(ring, 3i32)
  [RingBuffer<i32> mut] copy{ring}
  push<i32>(copy, 4i32)
  print_line(at<i32>(ring, 0i32))
  print_line(at<i32>(ring, 1i32))
  print_line(at<i32>(copy, 0i32))
  print_line(at<i32>(copy, 1i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n3\n3\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

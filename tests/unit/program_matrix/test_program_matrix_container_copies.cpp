#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.container_copies");

// Copying a container (docs/spec/value-lifecycle.md, Copies) gives the copy its own storage:
// a mutable binding initialized from an existing container, a copy parameter, and the
// elements and struct fields inside them run their Copy helpers.

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

TEST_CASE("an empty vector literal owns its storage") {
  program_matrix::ProgramCase program;
  program.name = "empty_vector_literal_owns_storage";
  program.source = R"(
import /std/collections/*

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] items{vector<i32>()}
  print_line(items.ownsData)
  vectorPush<i32>(items, 5i32)
  items.Destroy()
  print_line(items.ownsData)
  print_line(vectorCount<i32>(items))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n0\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("read-only binding of an owning struct gets its own copy") {
  program_matrix::ProgramCase program;
  program.name = "readonly_binding_owning_struct_copy";
  program.source = R"(
[struct]
Tracked() {
  [i32 mut] copies{0i32}
  [i64 mut] id{0i64}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.id, other.id)
    assign(this.copies, plus(other.copies, 1i32))
  }

  [public]
  Destroy() {
  }
}

[effects(io_out) return<int>]
main() {
  [Tracked] original{Tracked{}}
  [Tracked] copy{original}
  print_line(copy.copies)
  print_line(original.copies)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("vector at reads struct elements at every index") {
  program_matrix::ProgramCase program;
  program.name = "vector_struct_element_at";
  program.source = R"(
import /std/collections/*

[struct]
Point() {
  [i32 mut] x{0i32}
  [i32 mut] y{0i32}
}

[struct]
Mixed() {
  [i32 mut] x{0i32}
  [i64 mut] y{0i64}
}

[effects(io_out) return<i32>]
second_y([vector<Point>] pts) {
  [Point] p{pts.at(1i32)}
  return(p.y)
}

[effects(io_out heap_alloc) return<int>]
main() {
  [vector<Point> mut] pts{vector<Point>()}
  pts.push(Point{1i32, 2i32})
  pts.push(Point{-3i32, -4i32})
  pts.push(Point{5i32, 6i32})
  [Point] first{pts.at(0i32)}
  print_line(first.y)
  print_line(second_y(pts))
  for([i32 mut] i{0i32}, i < 3i32, i++) {
    [Point] q{pts.at(i)}
    print_line(q.x)
  }
  [vector<Mixed> mut] ms{vector<Mixed>()}
  ms.push(Mixed{1i32, 2i64})
  ms.push(Mixed{-3i32, -4i64})
  [Mixed] m{ms.at(1i32)}
  print_line(m.x)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n-4\n1\n-3\n5\n-3\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

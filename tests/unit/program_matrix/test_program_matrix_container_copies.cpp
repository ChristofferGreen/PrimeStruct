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

// A builtin `vector<vector<T>>` stores each inner vector as an inline five-slot record
// (docs/spec/value-lifecycle.md, Copies): a local, a struct field, a parameter and a returned
// value push, index and count it, a copy gives every inner vector a buffer of its own, and a
// destroy frees each inner buffer. Before, `a[0i32].count()` did not lower, a pushed
// vector was stored as a pointer to its record, and a field's inner buffers leaked.
TEST_CASE("builtin vectors of vectors push, index, copy and free each level") {
  program_matrix::ProgramCase program;
  program.name = "nested_builtin_vector_values";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code{0i32}
  [String] name{String{}}
}

[struct]
Grid {
  [vector<vector<i32>> mut] rows
}

[effects(heap_alloc) return<Grid>]
make_grid() {
  [vector<vector<i32>> mut] built{vector<vector<i32>>()}
  built.push(vector<i32>(4i32, 5i32, 6i32))
  return(Grid{[rows] built})
}

[effects(heap_alloc) return<vector<vector<i32>>>]
make_rows() {
  [vector<vector<i32>> mut] built{vector<vector<i32>>()}
  built.push(vector<i32>(4i32))
  return(built)
}

[effects(heap_alloc) return<int>]
grow([vector<vector<i32>> copy] v) {
  v.push(vector<i32>(9i32))
  return(v.count() * 10i32 + v[0i32].count())
}

[effects(heap_alloc) return<int>]
peek([vector<vector<i32>>] v) {
  return(v.count() * 10i32 + v[0i32][1i32])
}

[effects(heap_alloc) return<int>]
locals() {
  [vector<vector<i32>> mut] a{vector<vector<i32>>()}
  a.push(vector<i32>(1i32, 2i32))
  [vector<i32> mut] b{vector<i32>(7i32)}
  b.push(8i32)
  a.push(b)
  [vector<i32>] view{a[0i32]}
  [vector<vector<i32>> mut] c{a}
  c.push(view)
  c.push(vector<i32>(9i32))
  [vector<i32> mut] first{c[0i32]}
  first.push(3i32)
  [vector<vector<i32>>] returned{make_rows()}
  make_rows()
  vector<vector<i32>>()
  return(a.count() * 100000i32 + c.count() * 10000i32 + view.count() * 1000i32 + first.count() * 100i32 +
         a[1i32][1i32] * 10i32 + returned[0i32][0i32] + grow(a) * 0i32)
}

[effects(heap_alloc) return<int>]
fields() {
  [Grid mut] g{Grid{[rows] vector<vector<i32>>()}}
  g.rows.push(vector<i32>(1i32, 2i32))
  [vector<i32> mut] b{vector<i32>(5i32)}
  g.rows.push(b)
  [Grid mut] h{g}
  h.rows.push(vector<i32>(3i32))
  [vector<vector<i32>> mut] loose{vector<vector<i32>>()}
  loose.push(g.rows[1i32])
  [Grid] made{make_grid()}
  [Grid mut] copied{made}
  copied.rows.push(vector<i32>(8i32))
  [Grid] moved{move(h)}
  return(g.rows.count() * 100000i32 + moved.rows.count() * 10000i32 + g.rows[0i32].count() * 1000i32 +
         g.rows[0i32][1i32] * 100i32 + loose[0i32][0i32] * 10i32 + made.rows[0i32][2i32] + copied.rows.count() * 0i32)
}

[effects(heap_alloc) return<int>]
parameters() {
  [vector<vector<i32>> mut] a{vector<vector<i32>>()}
  a.push(vector<i32>(1i32, 2i32))
  return(grow(a) * 1000i32 + peek(a) * 10i32 + a.count())
}

[effects(heap_alloc) return<int>]
named_rows() {
  [vector<Named> mut] row{vector<Named>()}
  row.push(Named{[code] 4i32, [name] stringFrom("a nested row element with a heap string")})
  [vector<vector<Named>> mut] table{vector<vector<Named>>()}
  table.push(row)
  [vector<vector<Named>> mut] copy{table}
  return(table.count() + copy[0i32].count() * 10i32 + copy[0i32][0i32].code * 100i32)
}

[return<int> effects(heap_alloc)]
main() {
  [i32 mut] total{0i32}
  repeat(200i32) {
    total = locals() - 242384i32 + fields() - 232256i32 + parameters() - 22121i32 + named_rows() - 411i32
  }
  return(total + 7i32)
}
)";
  program.exitCode = 7;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    program.onlyConfigs.push_back("native-O0");
    program.onlyConfigs.push_back("native-O2");
  }
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("builtin vectors of vectors free every inner buffer") {
  program_matrix::ProgramCase program;
  program.name = "nested_builtin_vector_heap_flat";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code{0i32}
  [String] name{String{}}
}

[struct]
Grid {
  [vector<vector<i32>> mut] rows
}

[effects(heap_alloc) return<Grid>]
make_grid() {
  [vector<vector<i32>> mut] built{vector<vector<i32>>()}
  built.push(vector<i32>(4i32, 5i32, 6i32))
  return(Grid{[rows] built})
}

[effects(heap_alloc) return<vector<vector<i32>>>]
make_rows() {
  [vector<vector<i32>> mut] built{vector<vector<i32>>()}
  built.push(vector<i32>(4i32))
  return(built)
}

[effects(heap_alloc) return<int>]
grow([vector<vector<i32>> copy] v) {
  v.push(vector<i32>(9i32))
  return(v.count() * 10i32 + v[0i32].count())
}

[effects(heap_alloc) return<int>]
peek([vector<vector<i32>>] v) {
  return(v.count() * 10i32 + v[0i32][1i32])
}

[effects(heap_alloc) return<int>]
locals() {
  [vector<vector<i32>> mut] a{vector<vector<i32>>()}
  a.push(vector<i32>(1i32, 2i32))
  [vector<i32> mut] b{vector<i32>(7i32)}
  b.push(8i32)
  a.push(b)
  [vector<i32>] view{a[0i32]}
  [vector<vector<i32>> mut] c{a}
  c.push(view)
  c.push(vector<i32>(9i32))
  [vector<i32> mut] first{c[0i32]}
  first.push(3i32)
  [vector<vector<i32>>] returned{make_rows()}
  make_rows()
  vector<vector<i32>>()
  return(a.count() * 100000i32 + c.count() * 10000i32 + view.count() * 1000i32 + first.count() * 100i32 +
         a[1i32][1i32] * 10i32 + returned[0i32][0i32] + grow(a) * 0i32)
}

[effects(heap_alloc) return<int>]
fields() {
  [Grid mut] g{Grid{[rows] vector<vector<i32>>()}}
  g.rows.push(vector<i32>(1i32, 2i32))
  [vector<i32> mut] b{vector<i32>(5i32)}
  g.rows.push(b)
  [Grid mut] h{g}
  h.rows.push(vector<i32>(3i32))
  [vector<vector<i32>> mut] loose{vector<vector<i32>>()}
  loose.push(g.rows[1i32])
  [Grid] made{make_grid()}
  [Grid mut] copied{made}
  copied.rows.push(vector<i32>(8i32))
  [Grid] moved{move(h)}
  return(g.rows.count() * 100000i32 + moved.rows.count() * 10000i32 + g.rows[0i32].count() * 1000i32 +
         g.rows[0i32][1i32] * 100i32 + loose[0i32][0i32] * 10i32 + made.rows[0i32][2i32] + copied.rows.count() * 0i32)
}

[effects(heap_alloc) return<int>]
parameters() {
  [vector<vector<i32>> mut] a{vector<vector<i32>>()}
  a.push(vector<i32>(1i32, 2i32))
  return(grow(a) * 1000i32 + peek(a) * 10i32 + a.count())
}

[effects(heap_alloc) return<int>]
named_rows() {
  [vector<Named> mut] row{vector<Named>()}
  row.push(Named{[code] 4i32, [name] stringFrom("a nested row element with a heap string")})
  [vector<vector<Named>> mut] table{vector<vector<Named>>()}
  table.push(row)
  [vector<vector<Named>> mut] copy{table}
  return(table.count() + copy[0i32].count() * 10i32 + copy[0i32][0i32].code * 100i32)
}

[effects(heap_alloc) return<int>]
round() {
  return(locals() - 242384i32 + fields() - 232256i32 + parameters() - 22121i32 + named_rows() - 411i32)
}

[return<int> effects(heap_alloc)]
main() {
  [i32] first{round()}
  [i32] before{heapLiveBlocks()}
  [u64] textBefore{stringHeapBytesInUse()}
  repeat(300i32) {
    round()
  }
  [i32] leakedBlocks{heapLiveBlocks() - before}
  [i32] leakedText{convert<i32>(stringHeapBytesInUse() - textBefore)}
  return(first + 23i32 + leakedBlocks * 1000i32 + leakedText * 1000i32)
}
)";
  program.exitCode = 23;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

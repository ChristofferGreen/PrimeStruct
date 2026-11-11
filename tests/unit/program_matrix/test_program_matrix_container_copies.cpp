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

// `push` takes its argument as `move` (docs/spec/value-lifecycle.md, Ownership): a named owner is
// moved into the vector and can be given a new value afterwards; a view or a borrowed parameter is
// copied. Shared by the two cases below.
const char *const PushMovesSource = R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code{0i32}
  [String] name{String{}}
}

[effects(heap_alloc) return<int>]
add_borrowed([vector<vector<i32>> mut] rows, [vector<i32>] row) {
  rows.push(row)
  return(row.count())
}

[effects(heap_alloc) return<int>]
moves() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32, 2i32)}
  rows.push(row)
  row = vector<i32>(5i32)
  row.push(6i32)
  if(rows.count() == 1i32) {
    rows.push(row)
  } else {
    rows.push(row)
  }
  [i32 mut] round{0i32}
  while(round < 3i32) {
    row = vector<i32>(round)
    rows.push(row)
    round = round + 1i32
  }
  [vector<i32> mut] last{vector<i32>(7i32, 8i32, 9i32)}
  rows.push(move(last))
  [vector<i32>] view{rows[0i32]}
  [vector<vector<i32>> mut] other{vector<vector<i32>>()}
  other.push(view)
  [i32] borrowed{add_borrowed(other, view)}
  [vector<Named> mut] names{vector<Named>()}
  [Named mut] named{Named{[code] 4i32, [name] stringFrom("a pushed name with a heap string")}}
  names.push(named)
  assign(named, Named{[code] 5i32, [name] stringFrom("a second name with a heap string")})
  names.push(named)
  return(rows.count() * 10000i32 + rows[5i32][2i32] * 1000i32 + other.count() * 100i32 + view.count() * 10i32 +
         borrowed + names[1i32].code * 0i32 + names.count() * 0i32)
}
)";

TEST_CASE("push moves named owners and copies views") {
  program_matrix::ProgramCase program;
  program.name = "push_moves_named_owners";
  program.source = std::string(PushMovesSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] total{0i32}
  repeat(200i32) {
    total = moves() - 69222i32
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

TEST_CASE("push moves named owners without leaking") {
  program_matrix::ProgramCase program;
  program.name = "push_moves_heap_flat";
  program.source = std::string(PushMovesSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32] first{moves()}
  [i32] before{heapLiveBlocks()}
  [u64] textBefore{stringHeapBytesInUse()}
  repeat(300i32) {
    moves()
  }
  [i32] leakedBlocks{heapLiveBlocks() - before}
  [i32] leakedText{convert<i32>(stringHeapBytesInUse() - textBefore)}
  return(first - 69222i32 + 23i32 + leakedBlocks * 1000i32 + leakedText * 1000i32)
}
)";
  program.exitCode = 23;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

// A push whose argument lives in the vector's own buffer (`rows.push(rows[0i32])`) reads it
// before the buffer moves, and a struct pushed from a place (an element, a borrowed parameter) is
// copied, so its String is owned once by each side. Shared by the two cases below.
const char *const SelfPushSource = R"(
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

[effects(heap_alloc) return<int>]
add_param([vector<Named> mut] names, [Named] item) {
  names.push(item)
  return(names.count())
}

[effects(heap_alloc) return<int>]
self_pushes() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32, 2i32))
  repeat(40i32) {
    rows.push(rows[0i32])
  }
  [vector<Named> mut] names{vector<Named>()}
  names.push(Named{[code] 3i32, [name] stringFrom("a long heap string for the first element")})
  repeat(20i32) {
    names.push(names[0i32])
  }
  [Named] kept{Named{[code] 9i32, [name] stringFrom("a borrowed parameter with a heap string")}}
  [i32] added{add_param(names, kept)}
  [Grid mut] g{Grid{[rows] vector<vector<i32>>()}}
  g.rows.push(vector<i32>(5i32, 6i32, 7i32))
  repeat(9i32) {
    g.rows.push(g.rows[0i32])
  }
  [vector<i32> mut] copied{rows[0i32]}
  copied.push(8i32)
  rows.push(copied)
  return(rows.count() * 100000i32 + rows[40i32][1i32] * 10000i32 + rows[41i32].count() * 1000i32 +
         names[20i32].code * 100i32 + names[21i32].code * 10i32 + g.rows[9i32][2i32] + added * 0i32 +
         names[20i32].name.count() * 0i32 + g.rows.count() * 0i32)
}
)";

TEST_CASE("a vector pushes its own elements") {
  program_matrix::ProgramCase program;
  program.name = "push_own_elements";
  program.source = std::string(SelfPushSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] total{0i32}
  repeat(50i32) {
    total = self_pushes() - 4223397i32
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

TEST_CASE("a vector pushing its own elements frees every copy") {
  program_matrix::ProgramCase program;
  program.name = "push_own_elements_heap_flat";
  program.source = std::string(SelfPushSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32] first{self_pushes()}
  [i32] before{heapLiveBlocks()}
  [u64] textBefore{stringHeapBytesInUse()}
  repeat(100i32) {
    self_pushes()
  }
  [i32] leakedBlocks{heapLiveBlocks() - before}
  [i32] leakedText{convert<i32>(stringHeapBytesInUse() - textBefore)}
  return(first - 4223397i32 + 23i32 + leakedBlocks * 1000i32 + leakedText * 1000i32)
}
)";
  program.exitCode = 23;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

// Mutating methods on an inner vector of a builtin vector of vectors (`a[i].push(x)`,
// `this.rows[i].pop()`) change it in place, and a `mut` binding of an element or a field gets a
// copy of its own (TODO-5831). Shared by the two cases below.
const char *const InnerVectorMutationSource = R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code{0i32}
  [String] name{String{}}
}

[struct]
Bag {
  [vector<i32> mut] items
}

[struct]
Grid {
  [vector<vector<i32>> mut] rows

  [effects(heap_alloc)]
  grow([i32] row, [i32] value) {
    this.rows[row].push(value)
  }

  [effects(heap_alloc)]
  shrink([i32] row) {
    this.rows[row].pop()
  }

  [effects(heap_alloc)]
  empty([i32] row) {
    this.rows[row].clear()
  }
}

[effects(heap_alloc) return<int>]
inner_mutations() {
  [vector<vector<i32>> mut] a{vector<vector<i32>>()}
  a.push(vector<i32>(1i32, 2i32))
  a.push(vector<i32>(3i32))
  repeat(20i32) {
    a[0i32].push(5i32)
  }
  a[1i32].pop()
  a[1i32].push(9i32)
  a[1i32].push(8i32)
  a[1i32].pop()
  [Grid mut] g{Grid{[rows] vector<vector<i32>>()}}
  g.rows.push(vector<i32>(4i32))
  g.rows.push(vector<i32>(6i32, 7i32))
  repeat(10i32) {
    g.grow(0i32, 7i32)
  }
  g.rows[0i32].push(3i32)
  g.shrink(1i32)
  g.empty(1i32)
  g.grow(1i32, 2i32)
  [vector<vector<Named>> mut] table{vector<vector<Named>>()}
  table.push(vector<Named>())
  repeat(5i32) {
    table[0i32].push(Named{[code] 6i32, [name] stringFrom("an inner element with a heap string")})
  }
  [vector<i32> mut] copy{a[0i32]}
  a[0i32].clear()
  [Bag mut] bag{Bag{[items] vector<i32>(7i32)}}
  [vector<i32> mut] items{bag.items}
  items.push(8i32)
  return(a[0i32].count() * 100000i32 + copy.count() * 1000i32 + a[1i32][0i32] * 100i32 + a[1i32].count() * 10i32 +
         g.rows[0i32].count() + g.rows[0i32][11i32] * 1000000i32 + g.rows[1i32][0i32] * 10000000i32 +
         table[0i32].count() * 100000000i32 + table[0i32][4i32].code - 6i32 +
         items.count() - bag.items.count() - 1i32)
}
)";

TEST_CASE("inner vectors of a vector of vectors change in place") {
  program_matrix::ProgramCase program;
  program.name = "inner_vector_mutations";
  program.source = std::string(InnerVectorMutationSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] total{0i32}
  repeat(50i32) {
    total = inner_mutations() - 523022922i32
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

TEST_CASE("inner vector mutations free every buffer") {
  program_matrix::ProgramCase program;
  program.name = "inner_vector_mutations_heap_flat";
  program.source = std::string(InnerVectorMutationSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32] first{inner_mutations()}
  [i32] before{heapLiveBlocks()}
  [u64] textBefore{stringHeapBytesInUse()}
  repeat(100i32) {
    inner_mutations()
  }
  [i32] leakedBlocks{heapLiveBlocks() - before}
  [i32] leakedText{convert<i32>(stringHeapBytesInUse() - textBefore)}
  return(first - 523022922i32 + 23i32 + leakedBlocks * 1000i32 + leakedText * 1000i32)
}
)";
  program.exitCode = 23;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

// `pop`, `clear` and the direct `push(v, x)` / `pop(v)` / `clear(v)` forms on builtin vectors of
// structs and of vectors, as locals, struct fields (also through `this`) and inner vectors, destroy
// the elements they drop: Strings and inner buffers are freed (TODO-5834). Shared by the two
// cases below.
const char *const DroppingVectorMutationSource = R"(
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
  [vector<Named> mut] names

  [effects(heap_alloc)]
  trim() {
    this.rows.pop()
    this.names.pop()
  }
}

[effects(heap_alloc) return<int>]
dropping_round() {
  [vector<Named> mut] v{vector<Named>()}
  v.push(Named{[code] 1i32, [name] stringFrom("one heap string here")})
  v.push(Named{[code] 2i32, [name] stringFrom("two heap string here")})
  v.push(Named{[code] 3i32, [name] stringFrom("three heap string here")})
  v.pop()
  pop(v)
  [i32] a{v.count()}
  v.clear()
  v.push(Named{[code] 4i32, [name] stringFrom("four heap string here")})
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32, 2i32)}
  push(rows, row)
  push(rows, vector<i32>(4i32))
  rows.push(vector<i32>(5i32, 6i32))
  rows.pop()
  [i32] b{rows.count()}
  clear(rows)
  rows.push(vector<i32>(7i32))
  [Grid mut] g{Grid{[rows] vector<vector<i32>>(), [names] vector<Named>()}}
  g.rows.push(vector<i32>(1i32))
  g.rows.push(vector<i32>(2i32, 3i32))
  g.names.push(Named{[code] 5i32, [name] stringFrom("five heap string here")})
  g.names.push(Named{[code] 6i32, [name] stringFrom("six heap string here")})
  g.trim()
  g.rows.push(vector<i32>(8i32))
  g.rows.pop()
  g.names.clear()
  g.names.push(Named{[code] 7i32, [name] stringFrom("seven heap string here")})
  [vector<vector<Named>> mut] table{vector<vector<Named>>()}
  table.push(vector<Named>())
  table[0i32].push(Named{[code] 8i32, [name] stringFrom("eight heap string here")})
  table[0i32].push(Named{[code] 9i32, [name] stringFrom("nine heap string here")})
  table[0i32].pop()
  table.push(vector<Named>())
  table[1i32].push(Named{[code] 10i32, [name] stringFrom("ten heap string here")})
  table[1i32].clear()
  table.pop()
  return(a * 1000000i32 + b * 100000i32 + v[0i32].code * 10000i32 + rows[0i32][0i32] * 1000i32 +
         g.rows.count() * 100i32 + g.names[0i32].code * 10i32 + table[0i32][0i32].code - 8i32)
}
)";

TEST_CASE("pop and clear drop elements of vectors of structs and vectors") {
  program_matrix::ProgramCase program;
  program.name = "dropping_vector_mutations";
  program.source = std::string(DroppingVectorMutationSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] total{0i32}
  repeat(50i32) {
    total = dropping_round() - 1247170i32
  }
  return(total + 9i32)
}
)";
  program.exitCode = 9;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    program.onlyConfigs.push_back("native-O0");
    program.onlyConfigs.push_back("native-O2");
  }
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("pop and clear free every dropped element") {
  program_matrix::ProgramCase program;
  program.name = "dropping_vector_mutations_heap_flat";
  program.source = std::string(DroppingVectorMutationSource) + R"(
[return<int> effects(heap_alloc)]
main() {
  [i32] first{dropping_round()}
  [i32] before{heapLiveBlocks()}
  [u64] textBefore{stringHeapBytesInUse()}
  repeat(100i32) {
    dropping_round()
  }
  [i32] leakedBlocks{heapLiveBlocks() - before}
  [i32] leakedText{convert<i32>(stringHeapBytesInUse() - textBefore)}
  return(first - 1247170i32 + 29i32 + leakedBlocks * 1000i32 + leakedText * 1000i32)
}
)";
  program.exitCode = 29;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("pop and clear run the Destroy helper of each dropped element once") {
  program_matrix::ProgramCase program;
  program.name = "dropping_vector_mutations_destroy";
  program.source = R"(
import /std/collections/*

[struct]
Noisy {
  [i32] id{0i32}

  [effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[struct]
Holder {
  [vector<Noisy> mut] items
}

[effects(heap_alloc io_out)]
drop_last([vector<Noisy> mut] values) {
  values.pop()
}

[effects(heap_alloc io_out) return<int>]
main() {
  [vector<Noisy> mut] v{vector<Noisy>()}
  v.push(Noisy{[id] 1i32})
  v.push(Noisy{[id] 2i32})
  v.push(Noisy{[id] 3i32})
  v.pop()
  print_line(100i32)
  v.clear()
  print_line(101i32)
  v.push(Noisy{[id] 4i32})
  v.push(Noisy{[id] 10i32})
  drop_last(v)
  [Holder mut] h{Holder{[items] vector<Noisy>()}}
  h.items.push(Noisy{[id] 5i32})
  h.items.push(Noisy{[id] 6i32})
  pop(h.items)
  print_line(102i32)
  [vector<vector<Noisy>> mut] t{vector<vector<Noisy>>()}
  t.push(vector<Noisy>())
  t[0i32].push(Noisy{[id] 7i32})
  t[0i32].push(Noisy{[id] 8i32})
  t[0i32].pop()
  print_line(103i32)
  t.push(vector<Noisy>())
  t[1i32].push(Noisy{[id] 9i32})
  t.pop()
  print_line(104i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n100\n1\n2\n101\n10\n6\n102\n8\n103\n9\n104\n7\n5\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("assigning to a mut vector local destroys what it held and copies a place") {
  // A fresh vector (a call, a constructor) is taken over; a place is copied and keeps its own buffer;
  // `kept = kept` and an assignment in a loop are valid.
  program_matrix::ProgramCase program;
  program.name = "vector_local_assignment";
  program.source = R"(
import /std/text/*
import /std/collections/*

[struct]
Named() {
  [String] text{String{}}
}

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}

[effects(heap_alloc), return<vector<i32>>]
make_values([i32] first) {
  [vector<i32> mut] values{vector<i32>{}}
  values.push(first)
  values.push(first + 1i32)
  return(values)
}

[effects(heap_alloc), return<vector<Named>>]
make_named([i32] id) {
  [vector<Named> mut] out{vector<Named>{}}
  out.push(Named{label(id)})
  out.push(Named{label(id + 1i32)})
  return(out)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    [vector<i32> mut] kept{make_values(1i32)}
    kept = make_values(3i32)
    print_line(kept[0i32])
    [vector<i32>] other{make_values(7i32)}
    kept = other
    kept.push(20i32)
    print_line(kept.count())
    print_line(other.count())
    kept = kept
    print_line(kept[2i32])
    [i32 mut] index{0i32}
    while(index < 3i32) {
      kept = make_values(index)
      index = index + 1i32
    }
    print_line(kept[1i32])
    kept = vector<i32>{}
    print_line(kept.count())
    kept.push(9i32)
    print_line(kept[0i32])
    [mut] inferred{make_values(1i32)}
    inferred = other
    inferred = make_values(5i32)
    print_line(inferred[0i32])
    [vector<Named> mut] names{make_named(1i32)}
    names = make_named(4i32)
    [vector<Named>] more{make_named(6i32)}
    names = more
    names = names
    print_line(convert<i32>(names[1i32].text.byteAt(0i32)))
    print_line(convert<i32>(more[0i32].text.byteAt(0i32)))
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n3\n2\n20\n3\n0\n9\n5\n55\n54\n0\n0\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a vector local assigned from another keeps its own elements on every backend") {
  program_matrix::ProgramCase program;
  program.name = "vector_local_assignment_values";
  program.source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
make_values([i32] first) {
  [vector<i32> mut] values{vector<i32>{}}
  values.push(first)
  values.push(first + 1i32)
  return(values)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [vector<i32> mut] kept{make_values(1i32)}
  [vector<i32> mut] other{make_values(7i32)}
  kept = other
  other.push(30i32)
  kept[0i32] = 40i32
  print_line(kept.count())
  print_line(other.count())
  print_line(kept[0i32] + other[0i32])
  kept = make_values(3i32)
  print_line(kept[1i32])
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n3\n47\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE(
    "structs of 15 to 33 slots copy every slot by binding, parameter, result and vector element") {
  // A struct of 16 slots or more is copied by a loop over chunks of 8 slots and an unrolled remainder
  // (emitStructCopyFromPtrs); the sizes cover below, at and above the threshold and every remainder length.
  std::string source = "import /std/collections/*\n\n";
  std::string main = "[effects(heap_alloc), return<int>]\nmain() {\n";
  int checkCode = 1;
  for (const int slots : {15, 16, 17, 23, 24, 25, 33}) {
    const std::string name = "Big" + std::to_string(slots);
    std::string expected;
    source += "[struct]\n" + name + " {\n";
    for (int i = 0; i < slots; ++i) {
      source += "  [i32 mut] f" + std::to_string(i) + "{0i32}\n";
      expected += std::string(i == 0 ? "" : " + ") + "b.f" + std::to_string(i) + " * " +
                  std::to_string(i + 1) + "i32";
    }
    source += "}\n\n";
    source += "[return<i32>]\nweigh" + std::to_string(slots) + "([" + name +
              " copy] b) {\n  return(" + expected + ")\n}\n\n";
    source += "[return<" + name + ">]\nmake" + std::to_string(slots) + "([i32] base) {\n  [" +
              name + " mut] b{" + name + "{}}\n";
    for (int i = 0; i < slots; ++i) {
      source += "  b.f" + std::to_string(i) + " = base + " + std::to_string(i) + "i32\n";
    }
    source += "  return(b)\n}\n\n";
    int weighed = 0;
    for (int i = 0; i < slots; ++i) {
      weighed += (10 + i) * (i + 1);
    }
    const std::string n = std::to_string(slots);
    const std::string last = std::to_string(slots - 1);
    main += "  [" + name + " mut] a" + n + "{make" + n + "(10i32)}\n";
    main += "  [" + name + "] copied" + n + "{a" + n + "}\n";
    main += "  a" + n + ".f0 = 500i32\n  a" + n + ".f" + last + " = 600i32\n";
    main += "  [vector<" + name + "> mut] vec" + n + "{vector<" + name + ">{}}\n";
    main += "  vec" + n + ".push(copied" + n + ")\n  vec" + n + ".push(a" + n + ")\n";
    main += "  if(copied" + n + ".f0 != 10i32 || copied" + n + ".f" + last +
            " != " + std::to_string(10 + slots - 1) + "i32) {\n    return(" +
            std::to_string(checkCode++) + ")\n  }\n";
    main += "  if(vec" + n + "[0i32].f" + last + " != " + std::to_string(10 + slots - 1) +
            "i32 || vec" + n + "[1i32].f0 != 500i32 || vec" + n + "[1i32].f" + last +
            " != 600i32) {\n    return(" + std::to_string(checkCode++) + ")\n  }\n";
    main += "  if(weigh" + n + "(copied" + n + ") != " + std::to_string(weighed) +
            "i32) {\n    return(" + std::to_string(checkCode++) + ")\n  }\n";
  }
  main += "  return(0i32)\n}\n";
  program_matrix::ProgramCase program;
  program.name = "large_struct_slot_loop_copies";
  program.source = source + main;
  program.exitCode = 0;
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

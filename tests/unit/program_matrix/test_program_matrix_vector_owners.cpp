#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.vector_owners");

// Who owns a builtin `vector<T>` (docs/spec/value-lifecycle.md, Ownership): a local, a parameter by its mode, a
// temporary. Each program reads the heap statistics before and after its body and prints the differences last.

namespace {

// `label(id)` is one byte, the digit `id`; `make_values(first)` holds `first` and `first + 1`, `make_named(id)` the
// labels `id` and `id + 1`.
const std::string VectorOwnerPrelude = R"(
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

[return<i32>]
first_byte([String] text) {
  return(convert<i32>(text.byteAt(0i32)))
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
)";

void runVectorOwnerProgram(const std::string &name,
                           const std::string &definitions,
                           const std::string &body,
                           const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = VectorOwnerPrelude + definitions + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
)" + body + R"(
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = stdoutText + "0\n0\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("assigning to a mut vector parameter replaces the caller's vector in place") {
  // A temporary argument belongs to the callee; a place stays the caller's and sees the new vector.
  runVectorOwnerProgram("vector_mut_parameter_assignment", R"(
[effects(heap_alloc, io_out)]
swap_in([vector<i32> mut] values) {
  values = make_values(8i32)
  print_line(values[0i32])
}

[effects(heap_alloc, io_out)]
swap_place([vector<i32> mut] values) {
  [vector<i32>] other{make_values(4i32)}
  values = other
  values.push(1i32)
  print_line(values.count())
  print_line(other.count())
}

[effects(heap_alloc)]
swap_often([vector<i32> mut] values) {
  [i32 mut] index{0i32}
  while(index < 3i32) {
    values = make_values(index)
    index = index + 1i32
  }
}
)",
                        R"(
    swap_in(make_values(1i32))
    [vector<i32> mut] mine{make_values(2i32)}
    swap_in(mine)
    print_line(mine[0i32])
    swap_place(mine)
    print_line(mine.count())
    swap_place(make_values(3i32))
    swap_often(mine)
    swap_often(make_values(1i32))
    print_line(mine[0i32])
)",
                        "8\n8\n8\n3\n2\n3\n3\n2\n2\n");
}

TEST_CASE("assigning to a mut vector-of-struct parameter destroys the elements it held") {
  runVectorOwnerProgram("vector_mut_parameter_struct_elements", R"(
[struct]
Holder() {
  [vector<Named>] items{vector<Named>{}}
}

[effects(heap_alloc, io_out)]
swap_named([vector<Named> mut] values) {
  values = make_named(8i32)
  print_line(first_byte(values[1i32].text))
}

[effects(heap_alloc, io_out)]
swap_named_place([vector<Named> mut] values) {
  [vector<Named>] other{make_named(4i32)}
  values = other
  values.push(Named{label(1i32)})
  print_line(values.count())
  print_line(other.count())
}

[effects(heap_alloc, io_out)]
swap_through([vector<Named> mut] values) {
  swap_named(values)
  values.push(Named{label(1i32)})
}
)",
                        R"(
    swap_named(make_named(1i32))
    [vector<Named> mut] mine{make_named(2i32)}
    swap_named(mine)
    print_line(first_byte(mine[0i32].text))
    swap_named_place(mine)
    print_line(mine.count())
    swap_named_place(make_named(3i32))
    swap_through(mine)
    print_line(mine.count())
    [Holder mut] holder{Holder{make_named(1i32)}}
    swap_through(holder.items)
    print_line(holder.items.count())
    print_line(first_byte(holder.items[0i32].text))
)",
                        "57\n57\n56\n3\n2\n3\n3\n2\n57\n3\n57\n3\n56\n");
}

TEST_CASE("a mut vector parameter that is not assigned writes through to the caller") {
  program_matrix::ProgramCase program;
  program.name = "vector_mut_parameter_write_through";
  program.source = R"(
import /std/collections/*

[effects(heap_alloc)]
grow([vector<i32> mut] values) {
  values.push(9i32)
  values[0i32] = 5i32
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [vector<i32> mut] mine{vector<i32>{}}
  mine.push(1i32)
  grow(mine)
  grow(mine)
  print_line(mine.count())
  print_line(mine[0i32])
  print_line(mine[2i32])
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n5\n9\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("an inferred vector-of-struct local destroys its elements and owns what it is assigned") {
  // The element type comes from the type semantics inferred: the local is copied from a place, takes a
  // temporary over and destroys every element at scope end.
  runVectorOwnerProgram("vector_inferred_struct_local", "", R"(
    [mut] names{make_named(1i32)}
    print_line(first_byte(names[0i32].text))
    [mut] assigned{make_named(1i32)}
    assigned = make_named(4i32)
    [vector<Named>] more{make_named(6i32)}
    assigned = more
    assigned = assigned
    assigned.push(Named{label(1i32)})
    print_line(first_byte(assigned[1i32].text))
    print_line(more.count())
    [mut] copied{assigned}
    copied.push(Named{label(2i32)})
    print_line(assigned.count())
    print_line(copied.count())
    [mut] numbers{make_values(1i32)}
    numbers = make_values(5i32)
    print_line(numbers[0i32])
)",
                        "49\n55\n2\n3\n4\n5\n");
}

TEST_CASE("a copy mut vector parameter gets a vector of its own and a move parameter takes one over") {
  runVectorOwnerProgram("vector_owned_parameter_modes", R"(
[effects(heap_alloc, io_out)]
own_named([vector<Named> copy mut] values) {
  values.push(Named{label(3i32)})
  print_line(values.count())
  values = make_named(5i32)
  print_line(values.count())
}

[effects(heap_alloc, io_out)]
take_named([vector<Named> move] values) {
  print_line(values.count())
}

[effects(heap_alloc), return<vector<Named>>]
pass_on([vector<Named> move] values) {
  return(values)
}

[effects(heap_alloc), return<vector<Named>>]
changed([vector<Named> copy mut] values) {
  values = make_named(3i32)
  return(values)
}

[effects(heap_alloc, io_out)]
read_named([vector<Named>] values) {
  print_line(first_byte(values[2i32].text))
}
)",
                        R"(
    [vector<Named> mut] mine{make_named(2i32)}
    own_named(mine)
    own_named(make_named(1i32))
    print_line(mine.count())
    take_named(make_named(1i32))
    [vector<Named> mut] three{make_named(1i32)}
    three.push(Named{label(7i32)})
    read_named(three)
    [vector<Named>] back{pass_on(move(three))}
    print_line(back.count())
    [vector<Named>] other{changed(back)}
    print_line(first_byte(other[0i32].text))
    print_line(first_byte(back[0i32].text))
    take_named(mine)
)",
                        "3\n2\n3\n2\n2\n2\n55\n3\n51\n49\n2\n");
}

TEST_SUITE_END();

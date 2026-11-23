#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.element_assign");

// Assigning to an element of a builtin vector of structs or Strings (`items[at] = items[at - 1]`) steps by the
// element size, copies the value with its Copy helpers when it is a place and takes it over when it is a temporary,
// and destroys the element it replaces (docs/spec/value-lifecycle.md, Copies). Each program reads the heap and
// string-heap statistics before and after the body, which must match.

namespace {

const std::string AssignPrelude = R"(
import /std/text/*
import /std/text/view/*
import /std/collections/vector/*

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}

[effects(heap_alloc), return<void>]
insert_first([vector<String> mut] items, [String copy] item) {
  [i32 mut] slot{0i32}
  items.push(item)
  [i32 mut] at{items.count() - 1i32}
  while(at > slot) {
    items[at] = items[at - 1i32]
    at = at - 1i32
  }
}

[struct]
Pair() {
  [i32 mut] a{0i32}
  [i32 mut] b{0i32}
}

[struct]
Bag() {
  [vector<String> mut] names{vector<String>{}}

  [mut return<void> effects(heap_alloc)]
  copyFirstOver([i32] at) {
    this.names[at] = this.names[0i32]
  }
}
)";

// The programs of this file that run on Wasm like the VM (`PRIMESTRUCT_MATRIX_WASM_RUN=1`).
const std::vector<std::string> WasmPrograms = {"element_assign_scalar_struct"};

void runAssignProgram(const std::string &name,
                      const std::string &body,
                      const std::string &stdoutText,
                      const std::string &imports = "",
                      const std::string &definitions = "") {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = imports + AssignPrelude + definitions + R"(
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
  program_matrix::listWasmIf(program, WasmPrograms);
  program_matrix::runProgramMatrix(program);
}

// The elements of a stdlib `Vector` are places: assigning a whole element replaces it through the element's
// reference, destroys the old element once and copies a place or takes over a temporary.
const std::string StdlibElementDefinitions = R"(
[struct]
Record() {
  [i32 mut] n{0i32}
  [String mut] name{String{}}
}

[struct]
Shelf() {
  [Vector<Record> mut] records{Vector<Record>{}}
}

[effects(heap_alloc), return<Record>]
make_record([i32] n) {
  return(Record{[n] n, [name] label(n)})
}

[effects(heap_alloc), return<void>]
replace_first([Vector<Record> mut] records, [i32] n) {
  records[0i32] = make_record(n)
}
)";

} // namespace

TEST_CASE("an element assignment of a stdlib Vector replaces the element and frees the old one") {
  runAssignProgram("element_assign_stdlib_vector",
                   R"(
    [Vector<Record> mut] records{Vector<Record>{}}
    records.push(make_record(1i32))
    records.push(make_record(2i32))
    records[0i32] = make_record(7i32)
    print_line(records[0i32].n)
    print_line(convert<i32>(records[0i32].name.byteAt(0i32)))
    records[1i32] = records[0i32]
    records[0i32].n = 9i32
    print_line(records[1i32].n)
    print_line(convert<i32>(records[1i32].name.byteAt(0i32)))
    records[1i32] = records[1i32]
    print_line(records[1i32].n)
    replace_first(records, 4i32)
    print_line(records[0i32].n)
    print_line(records.count())
)",
                   "7\n55\n7\n55\n7\n4\n2\n",
                   "import /std/collections/*\n",
                   StdlibElementDefinitions);
}

TEST_CASE("an element assignment through a stdlib Vector field and of scalars") {
  runAssignProgram("element_assign_stdlib_field",
                   R"(
    [Shelf mut] shelf{Shelf{}}
    shelf.records.push(make_record(1i32))
    shelf.records[0i32] = make_record(5i32)
    print_line(convert<i32>(shelf.records[0i32].name.byteAt(0i32)))
    [Vector<i32> mut] numbers{Vector<i32>{}}
    numbers.push(1i32)
    numbers.push(2i32)
    numbers[1i32] = 8i32
    numbers[0i32] = numbers[1i32]
    print_line(numbers[0i32])
    print_line(numbers[1i32])
)",
                   "53\n8\n8\n",
                   "import /std/collections/*\n",
                   StdlibElementDefinitions);
}

TEST_CASE("shifting the strings of a vector up with element assignments frees each string once") {
  // The sixth push grows the buffer after five shifts; every element must still read as the shifted copy.
  runAssignProgram("element_assign_shift_strings",
                   R"(
    [vector<String> mut] items{vector<String>{}}
    [String] text{label(3i32)}
    insert_first(items, text)
    insert_first(items, text)
    insert_first(items, text)
    insert_first(items, text)
    insert_first(items, text)
    insert_first(items, text)
    print_line(items.count())
    for([i32 mut] i{0i32}, i < items.count(), i++) {
      print_line(items[i].view().count())
    }
    print_line(convert<i32>(text.byteAt(0i32)))
)",
                   "6\n2\n2\n2\n2\n2\n2\n51\n");
}

TEST_CASE("an element assignment copies a place and leaves its source readable") {
  runAssignProgram("element_assign_place_copy",
                   R"(
    [vector<String> mut] items{vector<String>{}}
    items.push(label(1i32))
    items.push(label(2i32))
    items.push(label(3i32))
    items[2i32] = items[0i32]
    items[1i32] = items[1i32]
    print_line(convert<i32>(items[0i32].byteAt(0i32)))
    print_line(convert<i32>(items[1i32].byteAt(0i32)))
    print_line(convert<i32>(items[2i32].byteAt(0i32)))
    items[0i32] = label(7i32)
    print_line(convert<i32>(items[0i32].byteAt(1i32)))
    print_line(convert<i32>(items[2i32].byteAt(1i32)))
)",
                   "49\n50\n49\n55\n49\n");
}

TEST_CASE("an element assignment of a struct of scalars steps by the element size") {
  runAssignProgram("element_assign_scalar_struct",
                   R"(
    [vector<Pair> mut] items{vector<Pair>{}}
    items.push(Pair{1i32, 2i32})
    items.push(Pair{3i32, 4i32})
    items.push(Pair{5i32, 6i32})
    items[1i32] = items[0i32]
    items[2i32] = Pair{9i32, 8i32}
    print_line(items[0i32].a)
    print_line(items[0i32].b)
    print_line(items[1i32].a)
    print_line(items[1i32].b)
    print_line(items[2i32].a)
    print_line(items[2i32].b)
)",
                   "1\n2\n1\n2\n9\n8\n");
}

TEST_CASE("an element assignment through a vector field copies the element") {
  runAssignProgram("element_assign_field",
                   R"(
    [Bag mut] bag{Bag{}}
    bag.names.push(label(1i32))
    bag.names.push(label(2i32))
    bag.copyFirstOver(1i32)
    bag.names[0i32] = label(5i32)
    print_line(convert<i32>(bag.names[0i32].byteAt(0i32)))
    print_line(convert<i32>(bag.names[1i32].byteAt(0i32)))
)",
                   "53\n49\n");
}

TEST_SUITE_END();

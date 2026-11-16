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

namespace {

// A sum with a struct, a vector and a scalar payload: `make_item(id)` holds the label `id` (0 to 5), the vector
// `make_values(id)` (above 5) or the number `id` (negative).
const std::string SumElementDefinitions = R"(
import /std/maybe/*

[sum]
Item {
  [Named] named
  [vector<i32>] numbers
  [i32] plain
}

[effects(heap_alloc), return<Item>]
make_item([i32] id) {
  if(id < 0i32) {
    return(Item{[plain] id})
  }
  if(id > 5i32) {
    return(Item{[numbers] make_values(id)})
  }
  return(Item{[named] Named{label(id)}})
}

[effects(heap_alloc), return<vector<Item>>]
make_items([i32] id) {
  [vector<Item> mut] out{vector<Item>{}}
  out.push(make_item(id))
  out.push(make_item(id + 5i32))
  out.push(make_item(0i32 - 1i32))
  return(out)
}

[effects(heap_alloc, io_out)]
show([Item] item) {
  pick(item) {
    named(n) {
      print_line(first_byte(n.text))
    }
    numbers(v) {
      print_line(v.count())
    }
    plain(p) {
      print_line(p)
    }
  }
}

[effects(heap_alloc), return<Maybe<String>>]
get([i32] id) {
  if(id < 0i32) {
    return(none<String>())
  }
  return(some<String>(label(id)))
}
)";

} // namespace

TEST_CASE("a vector of sums destroys the payload of each element") {
  // A temporary and a named sum local are taken over; `pop` and `clear` destroy what they drop.
  runVectorOwnerProgram("vector_sum_elements_destroyed",
                        SumElementDefinitions,
                        R"(
    [vector<Maybe<String>> mut] all{vector<Maybe<String>>{}}
    all.push(get(5i32))
    [Maybe<String>] named{get(6i32)}
    all.push(named)
    all.push(get(-1i32))
    all.push(get(7i32))
    print_line(all.count())
    pick(all[1i32]) {
      none {
        print_line(0i32)
      }
      some(text) {
        print_line(first_byte(text))
      }
    }
    all.pop()
    print_line(all.count())
    [vector<Maybe<String>> mut] cleared{vector<Maybe<String>>{}}
    cleared.push(get(1i32))
    cleared.push(get(2i32))
    cleared.clear()
    print_line(cleared.count())
    [vector<Item> mut] items{make_items(1i32)}
    show(items[0i32])
    show(items[1i32])
    show(items[2i32])
    items.pop()
    items.pop()
    print_line(items.count())
)",
                        "4\n54\n3\n0\n49\n2\n-1\n1\n");
}

TEST_CASE("remove_at and remove_swap destroy the sum element they remove") {
  // TODO-5887: the removed payload is destroyed by its tag and the elements that move keep theirs.
  runVectorOwnerProgram("vector_sum_elements_removed",
                        SumElementDefinitions + R"(
[effects(heap_alloc, io_out)]
show_maybe([Maybe<String>] value) {
  pick(value) {
    none {
      print_line(0i32)
    }
    some(text) {
      print_line(first_byte(text))
    }
  }
}
)",
                        R"(
    [vector<Maybe<String>> mut] all{vector<Maybe<String>>{}}
    all.push(get(5i32))
    all.push(get(-1i32))
    all.push(get(6i32))
    all.push(get(7i32))
    all.remove_at(0i32)
    print_line(all.count())
    show_maybe(all[0i32])
    show_maybe(all[1i32])
    show_maybe(all[2i32])
    all.remove_swap(0i32)
    print_line(all.count())
    show_maybe(all[0i32])
    show_maybe(all[1i32])
    all.remove_swap(1i32)
    all.remove_at(0i32)
    print_line(all.count())
    [vector<Item> mut] items{make_items(1i32)}
    items.remove_at(1i32)
    show(items[0i32])
    show(items[1i32])
    items.remove_swap(0i32)
    print_line(items.count())
    show(items[0i32])
    [vector<Maybe<i32>> mut] plain{vector<Maybe<i32>>{}}
    plain.push(some<i32>(4i32))
    plain.push(none<i32>())
    plain.remove_at(0i32)
    plain.remove_swap(0i32)
    print_line(plain.count())
)",
                        "3\n0\n54\n55\n2\n55\n54\n0\n49\n-1\n1\n-1\n0\n");
}

TEST_CASE("remove_at past the end of a vector of sums traps") {
  program_matrix::ProgramCase program;
  program.name = "vector_sum_remove_out_of_range";
  program.source = VectorOwnerPrelude + SumElementDefinitions + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [vector<Maybe<String>> mut] all{vector<Maybe<String>>{}}
  all.push(get(5i32))
  all.push(get(6i32))
  all.remove_swap(1i32)
  print_line(all.count())
  all.remove_at(1i32)
  print_line(99i32)
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "1\n";
  program.vmStderr = "array index out of bounds\n";
  program.nativeStderr = "array index out of bounds\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a vector of sums without owning payloads frees only its buffer") {
  runVectorOwnerProgram("vector_sum_elements_plain",
                        SumElementDefinitions,
                        R"(
    [vector<Maybe<i32>> mut] plain{vector<Maybe<i32>>{}}
    plain.push(some<i32>(4i32))
    plain.push(none<i32>())
    [Maybe<i32>] named{some<i32>(6i32)}
    plain.push(named)
    plain.pop()
    print_line(plain.count())
    [vector<Maybe<i32>> mut] other{plain}
    other.clear()
    print_line(plain.count())
    pick(plain[0i32]) {
      none {
        print_line(0i32)
      }
      some(value) {
        print_line(value)
      }
    }
)",
                        "2\n2\n4\n");
}

TEST_CASE("copying a vector of sums gives each element a payload of its own") {
  runVectorOwnerProgram("vector_sum_elements_copied",
                        SumElementDefinitions + R"(
[effects(heap_alloc, io_out)]
take_items([vector<Item> copy mut] items) {
  items.push(make_item(2i32))
  print_line(items.count())
}

[struct]
Bag() {
  [vector<Item>] items{vector<Item>{}}
}
)",
                        R"(
    [vector<Item> mut] items{make_items(1i32)}
    items.push(items[0i32])
    items.push(items[1i32])
    print_line(items.count())
    [vector<Item> mut] copied{items}
    copied.pop()
    print_line(copied.count())
    items = make_items(2i32)
    items = copied
    take_items(items)
    take_items(make_items(3i32))
    print_line(items.count())
    [Bag mut] bag{Bag{make_items(1i32)}}
    [Bag] other{bag}
    bag.items.pop()
    bag.items.clear()
    print_line(other.items.count())
    [vector<vector<Item>> mut] rows{vector<vector<Item>>{}}
    rows.push(make_items(1i32))
    rows.push(make_items(4i32))
    rows[0i32].pop()
    print_line(rows[0i32].count())
    [Item] view{items[0i32]}
    show(view)
    [Item mut] own{items[1i32]}
    own = make_item(3i32)
    show(own)
    show(items[1i32])
)",
                        "5\n4\n5\n4\n4\n3\n2\n49\n51\n2\n");
}

namespace {

const std::string RowDefinitions = R"(
[struct]
Row() {
  [i32] id{0i32}
  [String] text{String{}}
}

[effects(heap_alloc), return<vector<Row>>]
make_rows([i32] id) {
  [vector<Row> mut] out{vector<Row>{}}
  out.push(Row{id, label(id)})
  out.push(Row{id + 1i32, label(id + 1i32)})
  return(out)
}

[effects(heap_alloc), return<i32>]
count_of([i32] first) {
  return(make_values(first).count())
}
)";

} // namespace

TEST_CASE("a vector temporary that is only read is destroyed after the read") {
  // The receiver of `count()`, `capacity()` or an index with a scalar result is a hidden local of a scope
  // that spans the read: the buffer and every element are freed once the result is loaded.
  runVectorOwnerProgram("vector_temporary_read",
                        RowDefinitions,
                        R"(
    print_line(make_values(3i32).count())
    print_line(make_values(5i32)[0i32])
    print_line(make_values(5i32).at(1i32))
    print_line(make_values(7i32).at_unsafe(1i32))
    print_line(count(make_values(3i32)))
    print_line(make_values(3i32).capacity() > 0i32)
    print_line(vector<i32>{1i32, 2i32, 3i32}.count())
    print_line(make_rows(3i32).count())
    print_line(make_rows(5i32)[1i32].id)
    print_line(make_named(3i32).count())
    print_line(count_of(4i32))
)",
                        "2\n5\n6\n8\n2\n1\n3\n2\n6\n2\n2\n");
}

TEST_CASE(
    "a vector temporary read in an expression, a condition and a loop is destroyed each time") {
  runVectorOwnerProgram("vector_temporary_read_nested",
                        RowDefinitions,
                        R"(
    [i32] total{make_values(1i32).count() + make_values(10i32)[1i32]}
    print_line(total)
    if(make_values(1i32).count() > 1i32) {
      print_line(make_rows(8i32)[0i32].id + make_rows(9i32).count())
    }
    [i32 mut] sum{0i32}
    for([i32 mut] i{0i32}, i < make_values(0i32).count(), ++i) {
      sum = sum + make_values(i)[i]
    }
    print_line(sum)
)",
                        "13\n10\n2\n");
}

TEST_CASE("reading a named vector leaves it usable") {
  runVectorOwnerProgram("vector_named_read",
                        RowDefinitions,
                        R"(
    [vector<i32>] kept{make_values(4i32)}
    print_line(kept.count())
    print_line(kept[1i32])
    print_line(kept.count())
    [vector<Row>] rows{make_rows(6i32)}
    print_line(rows.count())
    print_line(rows[1i32].id)
    print_line(first_byte(rows[1i32].text))
    print_line(rows.count())
)",
                        "2\n5\n2\n2\n7\n55\n2\n");
}

namespace {

const std::string RowReaderDefinitions = RowDefinitions + R"(
[struct]
Holder() {
  [i32] id{0i32}
  [vector<i32>] values{vector<i32>{}}
  [Row] row{Row{}}
}

[effects(heap_alloc), return<vector<Holder>>]
make_holders() {
  [vector<Holder> mut] out{vector<Holder>{}}
  out.push(Holder{1i32, vector<i32>{4i32, 5i32}, Row{8i32, label(8i32)}})
  out.push(Holder{2i32, vector<i32>{6i32, 7i32, 8i32}, Row{9i32, label(9i32)}})
  return(out)
}

[effects(heap_alloc), return<vector<String>>]
make_texts([i32] id) {
  [vector<String> mut] out{vector<String>{}}
  out.push(label(id))
  out.push(label(id + 1i32))
  out.push(label(id + 2i32))
  return(out)
}

[effects(heap_alloc), return<vector<vector<i32>>>]
make_nested() {
  [vector<vector<i32>> mut] out{vector<vector<i32>>{}}
  out.push(vector<i32>{1i32})
  out.push(vector<i32>{1i32, 2i32, 3i32})
  return(out)
}

[return<i32>]
row_code([Row] row) {
  return(row.id * 100i32 + first_byte(row.text))
}

[effects(heap_alloc), return<Row>]
second_row([i32] id) {
  return(make_rows(id)[1i32])
}

[effects(heap_alloc), return<String>]
second_text([i32] id) {
  return(make_rows(id)[1i32].text)
}

[effects(heap_alloc), return<Row>]
first_row_or_empty([i32] id) {
  if(id < 0i32) {
    return(Row{})
  }
  return(make_rows(id)[0i32])
}
)";

} // namespace

TEST_CASE("an element of a vector temporary read as a struct or a String is copied out of it") {
  // The vector is a hidden local that lives to the end of the statement: the element is a place every consumer
  // copies from, at the stride of the element, and the vector is destroyed once the statement is done.
  runVectorOwnerProgram("vector_temporary_element",
                        RowReaderDefinitions,
                        R"(
    [Row] copy{make_rows(6i32)[1i32]}
    print_line(copy.id)
    print_line(first_byte(copy.text))
    print_line(first_byte(make_rows(4i32)[1i32].text))
    [String] text{make_rows(6i32)[0i32].text}
    print_line(first_byte(text))
    print_line(row_code(make_rows(1i32)[1i32]) + row_code(make_rows(5i32)[0i32]))
    print_line(row_code(make_rows(1i32).at(1i32)))
    print_line(row_code(make_rows(1i32).at_unsafe(0i32)))
    [Row mut] row{Row{1i32, label(1i32)}}
    row = make_rows(7i32)[1i32]
    print_line(row_code(row))
    [vector<Row> mut] rows{vector<Row>{}}
    rows.push(make_rows(2i32)[0i32])
    rows.push(make_rows(2i32)[1i32])
    print_line(first_byte(rows[1i32].text))
)",
                        "7\n55\n53\n54\n803\n250\n149\n856\n51\n");
}

TEST_CASE(
    "an element of a vector temporary read as a String, a vector or a field of those is copied") {
  runVectorOwnerProgram("vector_temporary_element_kinds",
                        RowReaderDefinitions,
                        R"(
    [String] text{make_texts(3i32)[2i32]}
    print_line(first_byte(text))
    print_line(first_byte(make_texts(1i32)[1i32]))
    print_line(make_nested()[1i32].count())
    [vector<i32>] inner{make_nested()[1i32]}
    print_line(inner[2i32])
    [vector<i32>] values{make_holders()[1i32].values}
    print_line(values[2i32])
    [Row] held{make_holders()[0i32].row}
    print_line(row_code(held))
    print_line(make_holders()[1i32].values[1i32])
    print_line(make_holders()[1i32].values.count())
)",
                        "53\n50\n3\n3\n8\n856\n7\n3\n");
}

TEST_CASE("an element of a vector temporary read where code may not run or runs again is destroyed "
          "each time") {
  // A loop condition and the right side of `and`/`or` are scopes of their own; a returned element is copied
  // out before the definition's scopes are cleaned.
  runVectorOwnerProgram("vector_temporary_element_paths",
                        RowReaderDefinitions,
                        R"(
    [i32 mut] i{0i32}
    while(first_byte(make_rows(i)[1i32].text) < 53i32) {
      i = i + 1i32
    }
    print_line(i)
    for([i32 mut] k{0i32}, first_byte(make_rows(k)[0i32].text) < 51i32, k = k + 1i32) {
      print_line(row_code(make_rows(k)[1i32]))
    }
    print_line(false && first_byte(make_rows(1i32)[1i32].text) == 50i32)
    print_line(true && first_byte(make_rows(1i32)[1i32].text) == 50i32)
    print_line(false || first_byte(make_rows(1i32)[0i32].text) == 49i32)
    print_line(true || first_byte(make_rows(1i32)[0i32].text) == 49i32)
    [i32] picked{if(i > 1i32) { first_byte(make_rows(4i32)[1i32].text) } else { 0i32 }}
    print_line(picked)
    [Row] returned{second_row(3i32)}
    print_line(row_code(returned))
    [String] returnedText{second_text(4i32)}
    print_line(first_byte(returnedText))
    print_line(first_row_or_empty(-1i32).id + first_row_or_empty(2i32).id)
)",
                        "4\n149\n250\n351\n0\n1\n1\n1\n53\n452\n53\n2\n");
}

TEST_CASE("an element read from a named vector is a copy and leaves the vector usable") {
  runVectorOwnerProgram("vector_named_element",
                        RowReaderDefinitions,
                        R"(
    [vector<Row>] rows{make_rows(6i32)}
    [Row] copy{rows[1i32]}
    [String] text{rows[0i32].text}
    print_line(row_code(copy))
    print_line(first_byte(text))
    print_line(row_code(rows[1i32]))
    print_line(first_byte(rows[0i32].text))
    print_line(rows.count())
)",
                        "755\n54\n755\n54\n2\n");
}

TEST_CASE("assigning move(other) to a vector local takes the other vector over") {
  // Nothing is copied: the target frees what it held, and the moved-from local frees nothing and can be
  // assigned again. Moving a local into itself keeps it.
  runVectorOwnerProgram("vector_assign_move_local",
                        "",
                        R"(
    [vector<i32> mut] a{make_values(1i32)}
    [vector<i32> mut] b{make_values(5i32)}
    a = move(b)
    print_line(a[0i32])
    [vector<Named> mut] c{make_named(1i32)}
    [vector<Named> mut] d{make_named(5i32)}
    c = move(d)
    print_line(c.count())
    print_line(first_byte(c[1i32].text))
    d = make_named(7i32)
    print_line(first_byte(c[0i32].text) + first_byte(d[0i32].text))
    c = move(c)
    print_line(first_byte(c[1i32].text))
    [i32 mut] round{0i32}
    while(round < 3i32) {
      [vector<Named> mut] next{make_named(round)}
      c = move(next)
      round = round + 1i32
    }
    print_line(first_byte(c[0i32].text))
)",
                        "5\n2\n54\n108\n54\n50\n");
}

TEST_CASE("the elements of a vector<String> field are bound, viewed and passed") {
  runVectorOwnerProgram("vector_string_field",
                        R"(
import /std/text/view/*

[struct]
Greeter() {
  [vector<String> mut] lines{vector<String>{}}
}

[return<i32>]
first_of([Reference<Greeter>] greeter, [i32] index) {
  return(first_byte(greeter.lines[index]))
}

[effects(heap_alloc), return<i32>]
view_length([Greeter] greeter, [i32] index) {
  return(greeter.lines[index].view().count())
}
)",
                        R"(
    [Greeter mut] greeter{Greeter{}}
    greeter.lines.push(label(1i32))
    greeter.lines.push(label(2i32))
    greeter.lines.push(label(3i32))
    [String] second{greeter.lines[1i32]}
    print_line(first_byte(second))
    print_line(first_byte(greeter.lines[2i32]))
    print_line(greeter.lines[0i32].view().count())
    print_line(first_of(location(greeter), 1i32))
    print_line(view_length(greeter, 2i32))
    print_line(greeter.lines.count())
)",
                        "50\n51\n1\n50\n1\n3\n");
}

TEST_CASE("returning a vector field or a vector element hands the caller a copy and returning a "
          "local moves it") {
  // The place keeps its own vector; the returned one is a buffer of its own, for an `i32` element and for a struct
  // element with a `String` field, and the heap statistics end where they began.
  runVectorOwnerProgram("vector_return_places",
                        RowReaderDefinitions + R"(
[struct]
RowHolder() {
  [vector<Row> mut] rows{vector<Row>{}}
}

[effects(heap_alloc), return<vector<i32>>]
field_values() {
  [Holder] holder{make_holders()[1i32]}
  return(holder.values)
}

[effects(heap_alloc), return<vector<i32>>]
element_values() {
  [vector<Holder> mut] holders{make_holders()}
  return(holders[1i32].values)
}

[effects(heap_alloc), return<vector<i32>>]
nested_element() {
  [vector<vector<i32>> mut] nested{make_nested()}
  return(nested[0i32])
}

[effects(heap_alloc), return<vector<Row>>]
field_rows() {
  [RowHolder mut] holder{RowHolder{}}
  holder.rows = make_rows(5i32)
  return(holder.rows)
}

[effects(heap_alloc), return<vector<i32>>]
temporary_element_values() {
  return(make_holders()[0i32].values)
}

[effects(heap_alloc), return<vector<i32>>]
moved_local() {
  [vector<i32> mut] local{make_values(7i32)}
  return(local)
}
)",
                        R"(
    [vector<i32> mut] a{field_values()}
    print_line(a.count())
    a.push(9i32)
    print_line(a[3i32])
    [vector<i32>] b{element_values()}
    print_line(b[2i32])
    [vector<i32>] c{nested_element()}
    print_line(c.count())
    [vector<Row>] d{field_rows()}
    print_line(row_code(d[1i32]))
    [vector<i32>] e{temporary_element_values()}
    print_line(e[1i32])
    [vector<i32>] f{moved_local()}
    print_line(f[1i32])
)",
                        "3\n9\n8\n1\n654\n5\n8\n");
}

TEST_CASE("a block expression whose value is a local of the block hands it over once") {
  // The binding or argument outside takes the block's local over; a value that is a local of an outer scope is
  // copied and leaves that local usable. The heap statistics end where they began.
  runVectorOwnerProgram("block_value_handover",
                        RowReaderDefinitions,
                        R"(
    [String] outer{label(4i32)}
    [String] same{block() { [i32] n{1i32} outer }}
    print_line(first_byte(same))
    print_line(first_byte(outer))
    [String] text{block() { [String] e{label(3i32)} return(e) }}
    print_line(first_byte(text))
    [Row] row{block() { [Row] r{Row{9i32, label(2i32)}} r }}
    print_line(row_code(row))
    print_line(first_byte(block() { [String] e{label(1i32)} e }))
    [String] fromRows{block() { [String] e{label(3i32)} e }}
    print_line(first_byte(fromRows))
    print_line(first_byte(block() { [vector<Row>] v{make_rows(4i32)} [String] e{v[1i32].text} e }))
    [Row] copy{block() { [vector<Row>] v{make_rows(6i32)} [Row] e{v[1i32]} e }}
    print_line(row_code(copy))
)",
                        "52\n52\n51\n950\n49\n51\n53\n755\n");
}

TEST_CASE("the mutating methods change a local declared Vector<T>") {
  // `push`, `reserve`, `pop`, `remove_at`, `remove_swap` and `clear` take the stdlib helper of that name, which is
  // declared on a `Vector<T> mut`. The heap statistics end where they began.
  runVectorOwnerProgram("vector_struct_mutating_methods",
                        "",
                        R"(
    [Vector<i32> mut] grown{vector<i32>(3i32)}
    grown.push(4i32)
    grown.push(5i32)
    grown.reserve(16i32)
    print_line(grown.count())
    grown.pop()
    grown.remove_at(0i32)
    print_line(grown[0i32])
    grown.push(9i32)
    grown.remove_swap(0i32)
    print_line(grown[0i32])
    grown.clear()
    print_line(grown.count())
)",
                        "3\n4\n9\n0\n");
}

TEST_CASE("a Vector of Strings destroys the strings that pop, clear, remove_at and remove_swap remove") {
  // The stdlib vector struct has no element size of its own: `pop` and `clear` take it from the specialization, so
  // the removed strings are freed and the heap statistics end where they began.
  runVectorOwnerProgram("vector_string_removals",
                        "",
                        R"(
    [Vector<String> mut] names{vector<String>{}}
    names.push(label(1i32))
    names.push(label(2i32))
    names.push(label(3i32))
    names.push(label(4i32))
    names.pop()
    print_line(names.count())
    names.remove_at(0i32)
    print_line(first_byte(/std/collections/vector/at<String>(names, 0i32)))
    names.remove_swap(0i32)
    print_line(first_byte(/std/collections/vector/at<String>(names, 0i32)))
    names.clear()
    print_line(names.count())
    names.push(label(5i32))
)",
                        "3\n50\n51\n0\n");
}

TEST_CASE("at, at_unsafe and [] read the element of a local declared Vector<String>") {
  // The method and index forms take the stdlib `at` like the explicit helper: a String is more than one slot.
  runVectorOwnerProgram("vector_string_element_reads",
                        "",
                        R"(
    [Vector<String> mut] names{vector<String>{}}
    names.push(label(1i32))
    names.push(label(2i32))
    names.push(label(3i32))
    print_line(first_byte(names[1i32]))
    print_line(first_byte(names.at(2i32)))
    print_line(first_byte(names.at_unsafe(0i32)))
    names.remove_at(0i32)
    print_line(first_byte(names[0i32]))
    print_line(first_byte(names.at(1i32)))
    names.remove_swap(0i32)
    print_line(first_byte(names[0i32]))
    [String] kept{names[0i32]}
    names.clear()
    print_line(first_byte(kept))
)",
                        "50\n51\n49\n50\n51\n51\n51\n");
}

TEST_CASE("at and [] read a struct element and a scalar element of a local declared Vector<T>") {
  // A struct with a String is read into a binding (a copy the scope destroys); a vector of i32 that only pushed
  // reads its elements through the same helper.
  runVectorOwnerProgram("vector_struct_and_scalar_element_reads",
                        "",
                        R"(
    [Vector<Named> mut] items{vector<Named>{}}
    items.push(Named{label(1i32)})
    items.push(Named{label(2i32)})
    [Named] second{items[1i32]}
    [Named] third{items.at(0i32)}
    print_line(first_byte(second.text))
    print_line(first_byte(third.text))
    items.remove_at(0i32)
    [Named] now_first{items[0i32]}
    print_line(first_byte(now_first.text))
    [Vector<i32> mut] numbers{vector<i32>{}}
    numbers.push(7i32)
    numbers.push(8i32)
    print_line(numbers[0i32] + numbers.at(1i32))
)",
                        "50\n49\n50\n15\n");
}

TEST_CASE("a field read of a struct a call returns destroys the temporary") {
  // `items.at(i).code` copies the element, reads one field and drops the copy: the String it holds is freed.
  runVectorOwnerProgram("call_result_field_read",
                        R"(
[struct]
Coded() {
  [String] code{String{}}
}

[effects(heap_alloc), return<Coded>]
make_coded([i32] id) {
  return(Coded{label(id)})
}
)",
                        R"(
    [Vector<Coded> mut] items{vector<Coded>{}}
    items.push(make_coded(1i32))
    items.push(make_coded(2i32))
    items.push(make_coded(3i32))
    print_line(first_byte(/std/collections/vector/at(items, 2i32).code))
    print_line(first_byte(items.at(1i32).code))
    print_line(first_byte(items[0i32].code))
    print_line(first_byte(make_coded(4i32).code))
)",
                        "51\n50\n49\n52\n");
}

TEST_CASE("a field read of a place a call returns is not destroyed") {
  // A Reference-returning call hands back the owner's own value: reading a field of it must leave it intact.
  runVectorOwnerProgram("reference_result_field_read",
                        R"(
[struct]
Coded() {
  [String] code{String{}}
}

[return<Reference<Coded>>]
pick_coded([Reference<Coded> mut] value) {
  return(value)
}
)",
                        R"(
    [Coded mut] kept{Coded{label(5i32)}}
    print_line(first_byte(pick_coded(kept).code))
    print_line(first_byte(pick_coded(kept).code))
    print_line(first_byte(kept.code))
    [vector<Coded> mut] rows{vector<Coded>{}}
    rows.push(Coded{label(6i32)})
    print_line(first_byte(rows[0i32].code))
    print_line(first_byte(rows.at(0i32).code))
    print_line(first_byte(rows[0i32].code))
)",
                        "53\n53\n53\n54\n54\n54\n");
}

// A text of 47 bytes is on the string heap; `long_text(id)` ends in the digit `id`.
const std::string LongTextHelper = R"(
[effects(heap_alloc), return<String>]
long_text([i32] id) {
  [String mut] text{String{}}
  text.append("a text long enough to live on the string heap-")
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}
)";

TEST_CASE(
    "a method call on a String element of a local declared Vector<String> destroys the copy") {
  // `names[i].count()` and `names.at(i).count()` copy the element, read it and drop the copy.
  runVectorOwnerProgram("vector_string_element_method_call",
                        LongTextHelper,
                        R"(
    [Vector<String> mut] names{/std/collections/vector/vector<String>()}
    names.push(long_text(1i32))
    names.push(long_text(2i32))
    print_line(names[0i32].count())
    print_line(names.at(1i32).count())
    print_line(names.at_unsafe(1i32).count())
    [i32 mut] total{0i32}
    [i32 mut] i{0i32}
    while(i < names.count()) {
      total = total + names[i].count()
      i = i + 1i32
    }
    print_line(total)
    print_line(first_byte(names[1i32]))
)",
                        "47\n47\n47\n94\n97\n");
}

TEST_CASE("element reads of a Vector<i32> and a builtin vector<String> destroy nothing") {
  // A scalar element has no destroy, and a builtin vector element is a place its vector keeps owning.
  runVectorOwnerProgram("vector_scalar_element_reads",
                        "",
                        R"(
    [Vector<i32> mut] numbers{vector<i32>{}}
    numbers.push(4i32)
    numbers.push(5i32)
    print_line(numbers[0i32] + numbers.at(1i32))
    [vector<String> mut] rows{vector<String>{}}
    rows.push(label(3i32))
    print_line(rows[0i32].count())
    print_line(rows[0i32].count())
)",
                        "9\n1\n1\n");
  runVectorOwnerProgram("builtin_vector_string_element_reads",
                        LongTextHelper,
                        R"(
    [vector<String> mut] rows{vector<String>{}}
    rows.push(long_text(3i32))
    print_line(rows[0i32].count())
    print_line(rows.at(0i32).count())
    print_line(rows[0i32].count())
)",
                        "47\n47\n47\n");
}

// The element read of a stdlib vector never depends on which stdlib modules the program imports: importing the map
// module used to send the bare `at` behind `[]` to the map helper and read the element with the builtin layout.
namespace {

void runItemReadProgram(const std::string &name, const std::string &extraImport) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = "import /std/text/*\nimport /std/collections/*\n" + extraImport + R"(
[struct]
Item() {
  [i32] id{0i32}
  [String] label{String{}}
}

[return<i32> effects(heap_alloc)]
items_size([Vector<Item> copy mut] items) {
  items.push(Item{99i32, String{}})
  [i32 mut] sum{0i32}
  [i32 mut] i{0i32}
  while(i < items.count()) {
    sum = sum + items[i].id + items[i].label.count()
    i = i + 1i32
  }
  return(sum)
}

[return<int> effects(heap_alloc, io_out)]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    [Vector<Item> mut] items{/std/collections/vector/vector<Item>()}
    [String mut] l{String{}}
    l.append("lbl")
    items.push(Item{3i32, l})
    print_line(items_size(items))
    print_line(items.count())
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "105\n1\n0\n0\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a Vector of structs reads an element of a copy mut parameter without a map import") {
  runItemReadProgram("vector_item_parameter_read_plain", "");
}

TEST_CASE("a Vector of structs reads an element of a copy mut parameter with the map import") {
  runItemReadProgram("vector_item_parameter_read_with_map", "import /std/collections/map/*\n");
}

// A stdlib `Vector` passed to a `[Vector<T> mut]` parameter is a borrow of the caller's record: `count()`, `capacity()`,
// element reads (also of a `String` or a struct) and a push through it all see the caller's vector, and handing the
// parameter on to a read-only parameter passes the same record.
TEST_CASE("a Vector mut parameter reads the caller's count, capacity and elements") {
  runVectorOwnerProgram("vector_mut_parameter_reads",
                        LongTextHelper + R"(
[struct]
Item() {
  [i32] id{0i32}
  [String] label{String{}}
}

[return<i32> effects(heap_alloc)]
read_numbers([Vector<i32> mut] values) {
  return(values.count() * 100i32 + values[0i32] + values.at(1i32))
}

[return<i32> effects(heap_alloc)]
view_numbers([Vector<i32>] values) {
  return(values.count() * 100i32 + values[0i32])
}

[return<i32> effects(heap_alloc)]
forward_numbers([Vector<i32> mut] values) {
  return(view_numbers(values))
}

[return<i32> effects(heap_alloc)]
same_capacity([Vector<i32> mut] values, [i32] expected) {
  if(values.capacity() == expected) {
    return(1i32)
  }
  return(0i32)
}

[effects(heap_alloc)]
add_number([Vector<i32> mut] values, [i32] number) {
  values.push(number)
}

[return<i32> effects(heap_alloc)]
name_length([Vector<String> mut] names) {
  return(names[0i32].count() + names.at(1i32).count())
}

[return<i32> effects(heap_alloc)]
item_sum([Vector<Item> mut] items) {
  [i32 mut] sum{0i32}
  [i32 mut] i{0i32}
  while(i < items.count()) {
    sum = sum + items[i].id + items[i].label.count()
    i = i + 1i32
  }
  return(sum)
}
)",
                        R"(
    [Vector<i32> mut] numbers{/std/collections/vector/vector<i32>()}
    numbers.push(7i32)
    numbers.push(8i32)
    print_line(read_numbers(numbers))
    [i32] capacity{numbers.capacity()}
    print_line(same_capacity(numbers, capacity))
    add_number(numbers, 9i32)
    print_line(numbers.count())
    print_line(forward_numbers(numbers))
    [Vector<String> mut] names{/std/collections/vector/vector<String>()}
    names.push(long_text(1i32))
    names.push(long_text(2i32))
    print_line(name_length(names))
    [Vector<Item> mut] items{/std/collections/vector/vector<Item>()}
    items.push(Item{3i32, label(1i32)})
    items.push(Item{5i32, label(2i32)})
    print_line(item_sum(items))
)",
                        "215\n1\n3\n307\n94\n10\n");
}

TEST_CASE("a place passed to a Vector mut parameter cannot be passed to another parameter") {
  program_matrix::ProgramCase program;
  program.name = "vector_mut_parameter_alias_rejected";
  program.source = R"(
import /std/collections/*

[return<i32> effects(heap_alloc)]
both([Vector<i32> mut] first, [Vector<i32>] second) {
  first.push(1i32)
  return(second.count())
}

[effects(heap_alloc) return<int>]
main() {
  [Vector<i32> mut] v{/std/collections/vector/vector<i32>()}
  return(both(v, v))
}
)";
  program.exitCode = 2;
  program.stdoutText = "";
  program.onlyConfigs = {"vm-O0"};
  program_matrix::runProgramMatrix(program);
}

// The element access of a builtin `vector<T>` is the builtin access even when the program also holds a stdlib vector:
// the stdlib `at` has a generated instance for that vector's element type, and a builtin receiver of another element
// type must not take its result type or its code.
TEST_CASE("at on a builtin vector beside a stdlib Vector of another element type") {
  runVectorOwnerProgram("builtin_vector_at_beside_stdlib_vector",
                        LongTextHelper,
                        R"(
    [Vector<i32> mut] numbers{/std/collections/vector/vector<i32>()}
    numbers.push(7i32)
    numbers.push(8i32)
    print_line(numbers.at(1i32))
    [vector<String> mut] rows{vector<String>{}}
    rows.push(long_text(1i32))
    rows.push(long_text(2i32))
    print_line(rows.at(0i32).count())
    print_line(rows.at_unsafe(1i32).count())
    print_line(first_byte(rows.at(1i32)))
    [String] second{rows.at(1i32)}
    print_line(second.count())
)",
                        "8\n47\n47\n97\n47\n");
}

TEST_CASE("at on a stdlib Vector still reads its own element type beside builtin vectors") {
  runVectorOwnerProgram("stdlib_vector_at_beside_builtin_vector",
                        LongTextHelper,
                        R"(
    [vector<String> mut] rows{vector<String>{}}
    rows.push(long_text(4i32))
    [Vector<String> mut] names{/std/collections/vector/vector<String>()}
    names.push(long_text(3i32))
    print_line(names.at(0i32).count())
    print_line(rows.at(0i32).count())
    print_line(first_byte(names.at(0i32)))
)",
                        "47\n47\n97\n");
}

// The return kind of the element access is the builtin one as well: `print_line` and arithmetic on `values.at(i)` must
// not ask the generated instances of the stdlib `at` made for other element types.
TEST_CASE("printing at of a builtin vector beside stdlib Vectors of other element types") {
  runVectorOwnerProgram("builtin_vector_at_print_beside_stdlib_vector",
                        LongTextHelper,
                        R"(
    [Vector<i64> mut] wide{/std/collections/vector/vector<i64>()}
    wide.push(5000000000i64)
    [Vector<String> mut] names{/std/collections/vector/vector<String>()}
    names.push(long_text(3i32))
    [vector<i32> mut] values{vector<i32>{}}
    values.push(7i32)
    values.push(9i32)
    print_line(values.at(0i32))
    print_line(values.at_unsafe(1i32))
    print_line(values.at(1i32) + 1i32)
    print_line(wide.at(0i32))
    print_line(names.at(0i32).count())
)",
                        "7\n9\n10\n5000000000\n47\n");
}

// A definition of the stdlib returns a vector it built, and the caller owns it: a function of an imported stdlib module
// is a definition under `/std/`, which the lowerer once took for an accessor that hands out a place. The namespace
// below stands in for such a module.
namespace {
const std::string StdlibVectorReturns = R"(
namespace std {
namespace probe {
  [effects(heap_alloc), return<vector<i32>>]
  make_ints([i32] first) {
    [vector<i32> mut] out{vector<i32>{}}
    out.push(first)
    out.push(first + 1i32)
    return(out)
  }

  [effects(heap_alloc), return<vector<String>>]
  make_labels([i32] id) {
    [vector<String> mut] out{vector<String>{}}
    out.push(/label(id))
    out.push(/label(id + 1i32))
    return(out)
  }
}
}
)";
} // namespace

TEST_CASE("a vector of scalars returned by a stdlib definition is freed by its caller") {
  runVectorOwnerProgram("stdlib_returned_vector_scalars",
                        StdlibVectorReturns,
                        R"(
    [i32 mut] total{0i32}
    repeat(40i32) {
      [vector<i32>] values{/std/probe/make_ints(3i32)}
      total = total + values.count() + values.at(1i32)
    }
    print_line(total)
)",
                        "240\n");
}

TEST_CASE("a vector of Strings returned by a stdlib definition is freed by its caller") {
  runVectorOwnerProgram("stdlib_returned_vector_strings",
                        StdlibVectorReturns,
                        R"(
    [i32 mut] total{0i32}
    repeat(40i32) {
      [vector<String>] labels{/std/probe/make_labels(1i32)}
      total = total + labels.count() + first_byte(labels.at(1i32))
    }
    print_line(total)
)",
                        "2080\n");
}

// A method that returns a vector hands over a vector of its own, like a function does; one that returns a place hands
// out the owner's field and must leave it alone.
namespace {
const std::string MethodVectorReturns = R"(
import /std/text/view/*

[struct]
Maker() {
  [i32] base{3i32}
  [vector<i32>] kept{vector<i32>{}}

  [effects(heap_alloc), return<vector<i32>>]
  makeValues() {
    return(make_values(this.base))
  }

  [effects(heap_alloc), return<vector<String>>]
  makeLabels() {
    [vector<String> mut] out{vector<String>{}}
    out.push(label(this.base))
    out.push(label(this.base + 1i32))
    return(out)
  }

  [effects(heap_alloc), return<vector<i32>>]
  makeAgain() {
    [vector<i32>] inner{this.makeValues()}
    return(inner)
  }

  [return<Reference<vector<i32>>>]
  keptValues() {
    return(this.kept)
  }
}

[effects(heap_alloc), return<vector<i32>>]
forward_values([Reference<Maker>] maker) {
  return(maker.makeValues())
}

[return<i32>]
count_of([vector<i32>] values) {
  return(values.count())
}

[effects(heap_alloc), return<i32>]
count_through([Reference<Maker>] maker) {
  [vector<i32>] values{maker.makeValues()}
  return(values.count())
}
)";
} // namespace

TEST_CASE("a vector a user method returns is freed by its caller") {
  runVectorOwnerProgram("method_returned_vector_scalars",
                        MethodVectorReturns,
                        R"(
    [Maker] maker{Maker{}}
    [i32 mut] total{0i32}
    repeat(40i32) {
      [vector<i32>] values{maker.makeValues()}
      total = total + values.count() + values.at(1i32)
    }
    print_line(total)
)",
                        "240\n");
}

TEST_CASE("a vector of Strings a user method returns is freed by its caller") {
  runVectorOwnerProgram("method_returned_vector_strings",
                        MethodVectorReturns,
                        R"(
    [Maker] maker{Maker{}}
    [i32 mut] total{0i32}
    repeat(40i32) {
      [vector<String>] labels{maker.makeLabels()}
      total = total + labels.count() + first_byte(labels.at(1i32))
    }
    print_line(total)
)",
                        "2160\n");
}

TEST_CASE(
    "a vector a method returns is freed through a reference receiver and when it is passed on") {
  runVectorOwnerProgram("method_returned_vector_reference_receiver",
                        MethodVectorReturns,
                        R"(
    [Maker mut] maker{Maker{}}
    [i32 mut] total{0i32}
    repeat(40i32) {
      total = total + count_through(maker)
      [vector<i32>] again{maker.makeAgain()}
      [vector<i32>] forwarded{forward_values(maker)}
      total = total + again.count() + forwarded.count()
      total = total + count_of(maker.makeValues())
    }
    print_line(total)
)",
                        "320\n");
}

TEST_CASE("a vector of scalars a method returns is freed as a temporary and when discarded") {
  runVectorOwnerProgram("method_returned_vector_temporary",
                        MethodVectorReturns,
                        R"(
    [Maker] maker{Maker{}}
    [i32 mut] total{0i32}
    repeat(40i32) {
      total = total + maker.makeValues().count()
      maker.makeValues()
    }
    print_line(total)
)",
                        "80\n");
}

TEST_CASE("a vector of Strings a method returns is freed as a temporary") {
  runVectorOwnerProgram("method_returned_vector_strings_temporary",
                        MethodVectorReturns,
                        R"(
    [Maker] maker{Maker{}}
    [i32 mut] total{0i32}
    repeat(40i32) {
      total = total + maker.makeLabels().count()
    }
    print_line(total)
)",
                        "80\n");
}

TEST_CASE("a vector of Strings a method returns is freed when its result is discarded") {
  runVectorOwnerProgram("method_returned_vector_strings_discarded",
                        MethodVectorReturns,
                        R"(
    [Maker] maker{Maker{}}
    repeat(40i32) {
      maker.makeLabels()
    }
)",
                        "");
}

TEST_CASE("a vector of Strings a function returns is freed as a temporary") {
  runVectorOwnerProgram("function_returned_vector_strings_temporary",
                        MethodVectorReturns,
                        R"(
    [i32 mut] total{0i32}
    repeat(40i32) {
      total = total + make_named(1i32).count()
    }
    print_line(total)
)",
                        "80\n");
}

TEST_CASE("a method that returns a reference to a vector field leaves the field alone") {
  runVectorOwnerProgram("method_returned_vector_reference",
                        MethodVectorReturns,
                        R"(
    [Maker mut] maker{Maker{}}
    maker.kept.push(7i32)
    [i32 mut] total{0i32}
    repeat(40i32) {
      total = total + maker.keptValues().count()
    }
    print_line(total)
    print_line(maker.kept.at(0i32))
)",
                        "40\n7\n");
}

TEST_CASE("a vector of Strings a stdlib view method returns is freed by its caller") {
  runVectorOwnerProgram("stdlib_method_returned_vector_strings",
                        MethodVectorReturns,
                        R"(
    [i32 mut] total{0i32}
    repeat(40i32) {
      [StringView] text{textOf("a,b,,c")}
      [vector<String>] pieces{text.splitOwned(textOf(","))}
      total = total + pieces.count()
      total = total + text.splitOwned(textOf(",")).count()
    }
    print_line(total)
)",
                        "320\n");
}

TEST_SUITE_END();

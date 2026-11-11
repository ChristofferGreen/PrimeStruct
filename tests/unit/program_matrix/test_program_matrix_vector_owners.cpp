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

TEST_SUITE_END();

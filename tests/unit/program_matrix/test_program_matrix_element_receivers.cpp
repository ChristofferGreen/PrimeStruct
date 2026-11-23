#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.element_receivers");

// A method call on an element of a builtin vector (`values[i].view()`, `this.nodes[i].id()`) reads the element in
// place: the vector keeps owning it and the call destroys nothing (docs/spec/value-lifecycle.md, Ownership). The
// programs import only the vector module of the collections library, as a stdlib file does, where `values[i]` is
// the stdlib `at` helper specialized for the element type, and read the heap statistics before and after the body.

namespace {

// `label(id)` is one byte, the digit `id`; `make_names(id)` holds the labels `id` and `id + 1`.
const std::string ElementPrelude = R"(
import /std/text/*
import /std/text/view/*
import /std/collections/vector/*

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}

[effects(heap_alloc), return<vector<String>>]
make_names([i32] id) {
  [vector<String> mut] out{vector<String>{}}
  out.push(label(id))
  out.push(label(id + 1i32))
  return(out)
}
)";

void runElementProgram(const std::string &name,
                       const std::string &definitions,
                       const std::string &body,
                       const std::string &stdoutText,
                       const std::string &imports = "") {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = imports + ElementPrelude + definitions + R"(
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

TEST_CASE("a method call on an element of a local vector of strings leaves the element alone") {
  runElementProgram("element_receiver_local",
                    "",
                    R"(
    [vector<String> mut] names{make_names(1i32)}
    print_line(names[0i32].view().count())
    print_line(names[1i32].view().count())
    print_line(convert<i32>(names[1i32].byteAt(0i32)))
    [StringView] first{names[0i32].view()}
    print_line(first.count())
    [String] copied{names[1i32]}
    names.push(label(5i32))
    print_line(names[0i32].view().count())
    print_line(convert<i32>(copied.byteAt(0i32)))
    print_line(convert<i32>(names[2i32].byteAt(0i32)))
)",
                    "1\n1\n50\n1\n1\n50\n53\n");
}

TEST_CASE("a by-value vector of strings parameter reads its elements for every kind of argument") {
  // A local, a field and a temporary given to a read-only parameter; the body calls a method on each element.
  runElementProgram("element_receiver_by_value_parameter",
                    R"(
[struct]
Holder() {
  [vector<String> mut] names{vector<String>{}}
}

[effects(heap_alloc), return<i32>]
total_bytes([vector<String>] names) {
  [i32 mut] sum{0i32}
  for([i32 mut] i{0i32}, i < names.count(), i++) {
    sum = sum + convert<i32>(names[i].byteAt(0i32)) + names[i].view().count()
  }
  return(sum)
}

[effects(heap_alloc), return<i32>]
forwarded([vector<String>] names) {
  return(total_bytes(names) + total_bytes(names))
}
)",
                    R"(
    [vector<String>] local{make_names(1i32)}
    print_line(total_bytes(local))
    print_line(total_bytes(local))
    [Holder mut] holder{Holder{}}
    holder.names.push(label(4i32))
    print_line(total_bytes(holder.names))
    print_line(total_bytes(make_names(2i32)))
    print_line(forwarded(local))
    print_line(forwarded(holder.names))
    print_line(convert<i32>(local[0i32].byteAt(0i32)))
    print_line(convert<i32>(holder.names[0i32].byteAt(0i32)))
)",
                    "101\n101\n53\n103\n202\n106\n49\n52\n");
}

TEST_CASE("an element method call behaves the same under the wide collections import") {
  runElementProgram("element_receiver_wide_import",
                    R"(
[effects(heap_alloc), return<i32>]
total_bytes([vector<String>] names) {
  [i32 mut] sum{0i32}
  for([i32 mut] i{0i32}, i < names.count(), i++) {
    sum = sum + names[i].view().count()
  }
  return(sum)
}
)",
                    R"(
    [vector<String> mut] names{make_names(1i32)}
    print_line(names[0i32].view().count())
    print_line(total_bytes(names))
    print_line(total_bytes(make_names(3i32)))
)",
                    "1\n2\n2\n",
                    "import /std/collections/*\n");
}

TEST_CASE("a binding of a user at keeps treating the copy it returns as a temporary") {
  // `bag.at(i)` is a user method that returns a String by value: the binding takes the copy over, while
  // `names[i]` of a builtin vector is the element place the vector keeps owning.
  runElementProgram("element_receiver_user_at",
                    R"(
[struct]
Bag() {
  [vector<String> mut] names{vector<String>{}}

  [effects(heap_alloc), return<String>]
  at([i32] index) {
    return(this.names[index])
  }
}
)",
                    R"(
    [Bag mut] bag{Bag{}}
    bag.names.push(label(1i32))
    bag.names.push(label(2i32))
    [String] first{bag.at(0i32)}
    [String] second{bag.names[1i32]}
    print_line(first.view().count())
    print_line(convert<i32>(first.byteAt(0i32)))
    print_line(convert<i32>(second.byteAt(0i32)))
    print_line(convert<i32>(bag.names[1i32].byteAt(0i32)))
)",
                    "1\n49\n50\n50\n");
}

TEST_CASE("a read-only method calls methods of the elements of a vector field in place") {
  // `this.nodes[i].method()` in a method that is not `mut`, twice, in a loop and through a Reference parameter: the
  // receiver is the element's own place, so the graph reads the same afterwards.
  runElementProgram("element_receiver_this_field",
                    R"(
[struct]
Node() {
  [String] id{String{}}
  [vector<String> mut] deps{vector<String>{}}

  [effects(heap_alloc), return<i32>]
  first() {
    return(convert<i32>(this.id.byteAt(0i32)) + this.id.view().count())
  }

  [return<i32>]
  depCount() {
    return(this.deps.count())
  }

  [effects(heap_alloc), return<i32>]
  depBytes() {
    [i32 mut] sum{0i32}
    for([i32 mut] i{0i32}, i < this.deps.count(), i++) {
      sum = sum + convert<i32>(this.deps[i].byteAt(0i32))
    }
    return(sum)
  }
}

[struct]
Graph() {
  [vector<Node> mut] nodes{vector<Node>{}}

  [effects(heap_alloc), return<i32>]
  firstOf([i32] index) {
    return(this.nodes[index].first())
  }

  [effects(heap_alloc), return<i32>]
  total() {
    [i32 mut] sum{0i32}
    for([i32 mut] i{0i32}, i < this.nodes.count(), i++) {
      sum = sum + this.nodes[i].first() + this.nodes[i].depCount() + this.nodes[i].depBytes()
      sum = sum + this.nodes[i].id.view().count()
    }
    return(sum)
  }
}

[effects(heap_alloc), return<i32>]
sum_nodes([Reference<vector<Node>>] nodes) {
  [i32 mut] sum{0i32}
  for([i32 mut] i{0i32}, i < nodes.count(), i++) {
    sum = sum + nodes[i].first() + nodes[i].depBytes()
  }
  return(sum)
}
)",
                    R"(
    [Graph mut] graph{Graph{}}
    [Node mut] node{Node{label(1i32)}}
    node.deps.push(label(7i32))
    node.deps.push(label(8i32))
    graph.nodes.push(node)
    graph.nodes.push(Node{label(2i32), make_names(3i32)})
    print_line(graph.firstOf(0i32))
    print_line(graph.firstOf(1i32))
    print_line(graph.total())
    print_line(graph.total())
    print_line(sum_nodes(graph.nodes))
    print_line(graph.firstOf(0i32))
    print_line(convert<i32>(graph.nodes[0i32].id.byteAt(0i32)))
    print_line(graph.nodes[1i32].depCount())
)",
                    "50\n51\n321\n321\n315\n50\n49\n2\n");
}

TEST_CASE(
    "a struct holding vectors of strings keeps them when it is pushed into a vector of structs") {
  // Pushed from a local, a `copy` parameter, a temporary, a local built in a method, `move(...)` and past the
  // capacity (the elements move to a new buffer), then read back and copied with the whole graph.
  runElementProgram("element_receiver_push_struct_with_vectors",
                    R"(
[struct]
Node() {
  [String] id{String{}}
  [vector<String> mut] deps{vector<String>{}}
  [vector<String> mut] args{vector<String>{}}

  [effects(heap_alloc), return<i32>]
  sum() {
    [i32 mut] total{convert<i32>(this.id.byteAt(0i32))}
    for([i32 mut] i{0i32}, i < this.deps.count(), i++) {
      total = total + convert<i32>(this.deps[i].byteAt(0i32))
    }
    for([i32 mut] i{0i32}, i < this.args.count(), i++) {
      total = total + convert<i32>(this.args[i].byteAt(0i32))
    }
    return(total)
  }
}

[struct]
Graph() {
  [vector<Node> mut] nodes{vector<Node>{}}

  [mut effects(heap_alloc)]
  add([Node copy] node) {
    this.nodes.push(node)
  }

  [mut effects(heap_alloc)]
  addNew([i32] id) {
    [Node mut] made{Node{label(id)}}
    made.deps.push(label(id + 1i32))
    made.args.push(label(id + 2i32))
    this.nodes.push(made)
  }

  [mut effects(heap_alloc)]
  addMoved([i32] id) {
    [Node mut] made{Node{label(id)}}
    made.deps.push(label(id + 1i32))
    this.nodes.push(move(made))
  }

  [effects(heap_alloc), return<i32>]
  total() {
    [i32 mut] sum{0i32}
    for([i32 mut] i{0i32}, i < this.nodes.count(), i++) {
      sum = sum + this.nodes[i].sum()
    }
    return(sum)
  }
}
)",
                    R"(
    [Graph mut] graph{Graph{}}
    [Node mut] node{Node{label(1i32)}}
    node.deps.push(label(2i32))
    graph.add(node)
    print_line(graph.total())
    graph.add(Node{label(3i32)})
    graph.addNew(1i32)
    graph.addMoved(2i32)
    for([i32 mut] k{0i32}, k < 12i32, k++) {
      graph.addNew(k)
    }
    print_line(graph.nodes.count())
    print_line(graph.total())
    print_line(node.sum())
    [Graph mut] other{graph}
    print_line(other.total())
    other.addNew(1i32)
    print_line(graph.total())
    print_line(other.total())
)",
                    "99\n16\n2363\n99\n2363\n2363\n2513\n");
}

TEST_CASE("a user method named at that returns a value is a call result, not an element place") {
  // `bag.at(i)` is shaped like an element access but resolves to a definition that returns a copy (TODO-6008): the
  // result is a temporary that a receiver read (`.view()`, `.byteAt()`, a field) destroys at the end of the
  // statement, as for any other call. The vector's own `names[i]` inside `at` stays a place.
  runElementProgram("element_receiver_user_at",
                    R"(
[struct]
Named() {
  [String] text{String{}}
  [i32] id{0i32}
}

[effects(heap_alloc), return<Named>]
make_named([i32] id) {
  return(Named{label(id), id})
}

[struct]
Bag() {
  [vector<String> mut] names{vector<String>{}}
  [vector<Named> mut] items{vector<Named>{}}

  [effects(heap_alloc), return<String>]
  at([i32] index) {
    return(this.names[index])
  }

  [effects(heap_alloc), return<String>]
  at_unsafe([i32] index) {
    return(this.names[index])
  }

  [effects(heap_alloc), return<Named>]
  item([i32] index) {
    return(this.items[index])
  }
}
)",
                    R"(
    [Bag mut] bag{Bag{}}
    bag.names.push(label(1i32))
    bag.names.push(label(2i32))
    bag.items.push(make_named(3i32))
    bag.items.push(make_named(4i32))
    print_line(bag.at(0i32).view().count())
    print_line(convert<i32>(bag.at(1i32).byteAt(0i32)))
    print_line(bag.at_unsafe(1i32).view().count())
    print_line(bag.item(1i32).id)
    print_line(bag.item(1i32).text.view().count())
    print_line(bag.item(0i32).id)
    [String] copied{bag.at(1i32)}
    print_line(copied.view().count())
    bag.at(0i32)
    print_line(bag.names[0i32].view().count())
)",
                    "1\n50\n1\n4\n1\n3\n1\n1\n");
}

TEST_CASE(
    "a user method result given to a read-only or copy parameter is destroyed by the callee") {
  // `take(bag.first())` hands a String the method returned by value to a parameter the callee only reads: the
  // result is a temporary of the call, as for `take(make_named(1i32))` (TODO-6025). A user method named `at` is
  // a call result too, while `take(names[0i32])` reads the vector's element in place and destroys nothing.
  runElementProgram("method_result_argument",
                    R"(
[struct]
Named() {
  [String] text{String{}}
  [i32] id{0i32}
}

[effects(heap_alloc), return<Named>]
make_named([i32] id) {
  return(Named{label(id), id})
}

[effects(heap_alloc), return<int>]
take([String] text) {
  return(convert<int>(text.view().count()))
}

[effects(heap_alloc), return<int>]
take_copy([String copy] text) {
  return(convert<int>(text.view().count()))
}

[effects(heap_alloc), return<int>]
take_named([Named] value) {
  return(value.id)
}

[effects(heap_alloc), return<int>]
take_named_copy([Named copy] value) {
  return(value.id)
}

[struct]
Bag() {
  [vector<String> mut] names{vector<String>{}}
  [vector<Named> mut] items{vector<Named>{}}

  [effects(heap_alloc), return<String>]
  first() {
    return(this.names[0i32])
  }

  [effects(heap_alloc), return<String>]
  at([i32] index) {
    return(this.names[index])
  }

  [effects(heap_alloc), return<Named>]
  item([i32] index) {
    return(this.items[index])
  }
}
)",
                    R"(
    [Bag mut] bag{Bag{}}
    bag.names.push(label(1i32))
    bag.names.push(label(2i32))
    bag.items.push(make_named(3i32))
    bag.items.push(make_named(4i32))
    print_line(take(bag.first()))
    print_line(take(bag.at(1i32)))
    print_line(take_copy(bag.first()))
    print_line(take_named(bag.item(1i32)))
    print_line(take_named_copy(bag.item(0i32)))
    print_line(take_named(make_named(5i32)))
    print_line(take(bag.names[0i32]))
    print_line(take_named(bag.items[1i32]))
)",
                    "1\n1\n1\n4\n3\n5\n1\n4\n");
}

TEST_CASE("an element of a stdlib Vector field of strings is a copy that is destroyed") {
  // `holder.names[i]` of a `[Vector<String>]` field calls the stdlib `at`, which returns a copy: the copy is a
  // temporary the read destroys and the element stays the vector's, so nothing is left live.
  runElementProgram("element_receiver_stdlib_vector_field",
                    R"(
[struct]
Holder() {
  [Vector<String> mut] names{/std/collections/vector/vector<String>()}

  [effects(heap_alloc), return<i32>]
  firstByte() {
    return(convert<i32>(this.names[0i32].byteAt(0i32)))
  }
}
)",
                    R"(
    [Holder mut] holder{Holder{}}
    holder.names.push(label(2i32))
    holder.names.push(label(3i32))
    print_line(holder.names[0i32].view().count())
    print_line(convert<i32>(holder.names[1i32].byteAt(0i32)))
    print_line(holder.firstByte())
    [String] copied{holder.names[0i32]}
    holder.names.push(label(4i32))
    print_line(convert<i32>(copied.byteAt(0i32)))
    print_line(convert<i32>(holder.names.at(2i32).byteAt(0i32)))
    [i32] total{holder.names.count()}
    print_line(total)
)",
                    "1\n51\n50\n50\n52\n3\n",
                    "import /std/collections/*\n");
}

TEST_CASE("a write to an element of a stdlib Vector changes the element") {
  // The element of a stdlib `Vector` is a place, as for the builtin `vector`: a field assignment, a `[mut]` method,
  // a `mut` parameter and a `push` into a vector field all change the element, not a copy of it, for a local, a
  // `mut` parameter, a field and `this`.
  program_matrix::ProgramCase program;
  program.name = "stdlib_vector_element_write";
  program.source = R"(
import /std/text/*
import /std/collections/*

[struct]
Inner() {
  [i32 mut] k{0i32}
  [mut] bump() { this.k = this.k + 1i32 }
}

[struct]
Item() {
  [i32 mut] n{0i32}
  [Inner mut] inner{Inner{}}
  [Vector<i32> mut] tags{Vector<i32>{}}
  [mut] bump() { this.n = this.n + 1i32 }
}

[struct]
Bag() {
  [Vector<Item> mut] items{Vector<Item>{}}
  [mut] touch() {
    this.items[0i32].n = 30i32
    this.items[0i32].bump()
  }
}

[return<void>]
poke([Item mut] item) {
  item.n = item.n + 100i32
}

[return<void>]
through_parameter([Vector<Item> mut] values) {
  values[1i32].n = 20i32
  values.at(1i32).bump()
  at(values, 1i32).inner.k = 4i32
  values[1i32].inner.bump()
}

[effects(heap_alloc, io_out) return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  if(true) {
    [Vector<Item> mut] local{/std/collections/vector/vector<Item>()}
    local.push(Item{})
    local.push(Item{})
    local[0i32].n = 7i32
    local[0i32].bump()
    [i32] first{local[0i32].n}
    print_line(first)
    local[0i32].n = local[0i32].n + local[1i32].n + 2i32
    poke(local[0i32])
    local[0i32].tags.push(5i32)
    local[0i32].tags.push(6i32)
    [i32] tagCount{local[0i32].tags.count()}
    print_line(local[0i32].n)
    print_line(tagCount)
    print_line(local[0i32].tags[1i32])
    through_parameter(local)
    print_line(local[1i32].n)
    print_line(local[1i32].inner.k)
    [Bag mut] bag{Bag{}}
    bag.items.push(Item{})
    bag.items[0i32].n = 10i32
    bag.items[0i32].bump()
    print_line(bag.items[0i32].n)
    bag.touch()
    print_line(bag.items[0i32].n)
    [Vector<Bag> mut] bags{Vector<Bag>{}}
    bags.push(Bag{})
    bags[0i32].items.push(Item{})
    bags[0i32].items[0i32].n = 40i32
    bags[0i32].items[0i32].bump()
    bags[0i32].touch()
    bags[0i32].items[0i32].inner.bump()
    print_line(bags[0i32].items[0i32].n)
    print_line(bags[0i32].items[0i32].inner.k)
  }
  print_line(heapLiveBlocks() - blocks)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "8\n110\n2\n6\n21\n5\n11\n31\n31\n1\n0\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    program.onlyConfigs.push_back("native-O0");
    program.onlyConfigs.push_back("native-O2");
  }
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("an out of range element write of a stdlib Vector is the bounds fault") {
  program_matrix::ProgramCase program;
  program.name = "stdlib_vector_element_write_bounds";
  program.source = R"(
import /std/collections/*

[struct]
Item() {
  [i32 mut] n{0i32}
}

[effects(heap_alloc, io_out) return<int>]
main() {
  [Vector<Item> mut] local{Vector<Item>{}}
  local.push(Item{})
  local[0i32].n = 1i32
  print_line(local[0i32].n)
  local[1i32].n = 2i32
  print_line(local[0i32].n)
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "1\n";
  program.vmStderr = "array index out of bounds\n";
  program.nativeStderr = "array index out of bounds\n";
  program.cppStderr = "array index out of bounds\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    program.onlyConfigs.push_back("native-O0");
    program.onlyConfigs.push_back("native-O2");
  }
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("element writes of a stdlib Vector of structs holding Strings leave nothing live") {
  // A String field assigned through the element replaces the element's String (the old one is freed once), a
  // `[mut]` method and a `mut` parameter change the element's own String and vector, and an element of a vector
  // field of an element is a place too.
  runElementProgram("stdlib_vector_element_write_strings",
                    R"(
[struct]
Row() {
  [i32 mut] n{0i32}
  [String mut] name{String{}}
  [Vector<String> mut] tags{Vector<String>{}}

  [mut effects(heap_alloc)]
  rename([i32] id) {
    this.name = label(id)
    this.n = this.n + 1i32
  }
}

[struct]
Table() {
  [Vector<Row> mut] rows{Vector<Row>{}}

  [mut effects(heap_alloc)]
  touch([i32] index) {
    this.rows[index].n = this.rows[index].n + 10i32
    this.rows[index].rename(7i32)
    this.rows[index].tags.push(label(8i32))
  }
}

[effects(heap_alloc), return<void>]
retag([Row mut] row, [i32] id) {
  row.tags.push(label(id))
  row.n = row.n + 100i32
}

[effects(heap_alloc), return<void>]
through_parameter([Vector<Row> mut] rows) {
  rows[1i32].name = label(5i32)
  rows.at(1i32).rename(6i32)
  retag(at(rows, 1i32), 4i32)
}
)",
                    R"(
    [Vector<Row> mut] rows{Vector<Row>{}}
    rows.push(Row{[n] 1i32, [name] label(1i32)})
    rows.push(Row{[n] 2i32, [name] label(2i32)})
    rows[0i32].n = 5i32
    rows[0i32].name = label(3i32)
    rows[0i32].rename(9i32)
    rows[0i32].tags.push(label(1i32))
    rows[0i32].tags.push(label(2i32))
    rows[0i32].tags[1i32].pushByte(convert<u8>(65i32))
    retag(rows[0i32], 3i32)
    through_parameter(rows)
    print_line(rows[0i32].n)
    print_line(convert<i32>(rows[0i32].name.byteAt(0i32)))
    [i32] tagCount{rows[0i32].tags.count()}
    print_line(tagCount)
    print_line(rows[0i32].tags[1i32].view().count())
    print_line(rows[1i32].n)
    print_line(convert<i32>(rows[1i32].name.byteAt(0i32)))
    [Table mut] table{Table{}}
    table.rows.push(Row{[n] 3i32, [name] label(3i32)})
    table.rows[0i32].n = 4i32
    table.touch(0i32)
    print_line(table.rows[0i32].n)
    print_line(convert<i32>(table.rows[0i32].name.byteAt(0i32)))
    [i32] tableTags{table.rows[0i32].tags.count()}
    print_line(tableTags)
)",
                    "106\n57\n3\n2\n103\n54\n15\n55\n1\n",
                    "import /std/collections/*\n");
}

TEST_SUITE_END();

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

TEST_SUITE_END();

#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.struct_methods");

// Methods of generic structs and overloaded struct methods (docs/spec/language-core.md, Helper overloading). These
// programs run on the VM kernels, optexe and native, the backends web servers and apps are built with.

namespace {

std::vector<std::string> methodConfigs() {
  std::vector<std::string> configs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    configs.push_back("native-O0");
    configs.push_back("native-O2");
  }
  return configs;
}

void runMethodProgram(const std::string &name, const std::string &source, int exitCode) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  program.onlyConfigs = methodConfigs();
  // Wasm runs these programs; the others use opcodes it lacks.
  static const std::vector<std::string> wasmPrograms = {
      "struct_methods_two_mutating_field_calls_in_expression",
      "struct_methods_mutating_field_call_in_expression",
      "struct_methods_generic_overloads",
      "struct_methods_generic_clear",
      "struct_methods_generic_struct_parameter",
      "struct_methods_imported_static_helper",
      "struct_methods_infer_through_generic_struct",
      "struct_methods_namespaced_sum",
      "struct_methods_overload_arguments",
      "struct_methods_unused_tuple_import"};
  if (program_matrix::listed(wasmPrograms, name)) {
    program.extraConfigs = {"wasm-wasi"};
  }
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a generic struct's method calls clear on a reference parameter, a local and a vector field") {
  runMethodProgram("struct_methods_generic_clear", R"(
import /std/collections/*

[struct]
Cell() {
  [i32 mut] n{5i32}

  [mut]
  clear() {
    this.n = 0i32
  }
}

[struct]
Holder<T>() {
  [vector<i32> mut] items{vector<i32>()}
  [Cell mut] own{Cell{}}

  [mut effects(heap_alloc)]
  fill() {
    this.items.push(1i32)
    this.items.push(2i32)
  }

  [mut]
  wipeParam([Reference<Cell>] cell) {
    cell.clear()
  }

  [mut]
  wipeLocal() {
    [Cell mut] mine{Cell{}}
    mine.clear()
    this.own.n = mine.n + 7i32
  }

  [mut effects(heap_alloc)]
  wipeField() {
    this.items.clear()
    this.own.clear()
  }

  [return<i32>]
  total() {
    return(this.items.count() + this.own.n)
  }
}

[return<int> effects(heap_alloc)]
main() {
  [Holder<i32> mut] h{Holder<i32>{}}
  h.fill()
  [Cell mut] shared{Cell{}}
  h.wipeParam(shared)
  h.wipeLocal()
  [i32] before{h.total()}
  h.wipeField()
  return(before * 10i32 + h.total() + shared.n)
}
)",
                   90);
}

TEST_CASE("same-arity struct method overloads resolve by a literal, a local, a parameter and a call") {
  runMethodProgram("struct_methods_overload_arguments", R"(
[struct]
Tag() {
  [i32] n{0i32}
}

[struct]
Other() {
  [i32] n{0i32}
}

[struct]
Box() {
  [return<int>]
  add([string] label, [Tag] tag) {
    return(1i32)
  }

  [return<int>]
  add([string] label, [i32] count) {
    return(2i32)
  }

  [return<int>]
  add([string] label, [Other] other) {
    return(4i32)
  }
}

[return<Tag>]
make_tag() {
  return(Tag{})
}

[return<int>]
add_tag([Box] box, [Tag] tag) {
  return(box.add("a", tag))
}

[return<int>]
main() {
  [Box] box{Box{}}
  [Tag] tag{Tag{}}
  [Other] other{Other{}}
  return(box.add("a", 3i32) + box.add("a", tag) * 3i32 + box.add("a", other) * 9i32 + add_tag(box, tag) * 27i32 +
         box.add("a", make_tag()) * 81i32)
}
)",
                   149);
}

TEST_CASE("same-arity overloads of a generic struct's methods resolve by argument type") {
  runMethodProgram("struct_methods_generic_overloads", R"(
[struct]
Tag() {
  [i32] n{0i32}
}

[struct]
Other() {
  [i32] n{0i32}
}

[struct]
Counter() {
  [i32 mut] n{0i32}
}

[struct]
Holder<T>() {
  [T mut] total{}
  [i32 mut] calls{0i32}

  [mut]
  on([Tag] tag, [i32] weight) {
    this.calls = this.calls + weight
  }

  [mut]
  on([Other] other, [i32] weight) {
    this.calls = this.calls + weight * 10i32
  }

  [mut]
  run([Tag] tag, [Function<void, Reference<T>> move] handler) {
    handler(location(this.total))
    this.calls = this.calls + 1i32
  }

  [mut]
  run([Other] other, [Function<void, Reference<T>> move] handler) {
    handler(location(this.total))
    this.calls = this.calls + 10i32
  }
}

[return<int> effects(heap_alloc)]
main() {
  [Holder<Counter> mut] holder{Holder<Counter>{}}
  [Tag] tag{Tag{}}
  [Other] other{Other{}}
  holder.on(tag, 1i32)
  holder.on(other, 2i32)
  holder.run(tag, []([Reference<Counter>] counter) { counter.n = counter.n + 2i32 })
  holder.run(other, []([Reference<Counter>] counter) { counter.n = counter.n + 5i32 })
  return(holder.calls + holder.total.n)
}
)",
                   39);
}

TEST_CASE("a sum parameter of a namespace and a pick in a generic struct's method lower") {
  runMethodProgram("struct_methods_namespaced_sum", R"(
namespace ui {
  [public sum]
  Event {
    none
    [i32] number
    [string] label
  }

  [public return<i32>]
  weight([Event] event) {
    [i32 mut] total{0i32}
    pick(event) {
      number(n) { total = n }
      label(s) { total = 7i32 }
      none { total = 1i32 }
    }
    return(total)
  }

  [public struct]
  Handlers<T>() {
    [mut]
    handle([Event] event, [Reference<i32> mut] sum) {
      [i32 mut] add{0i32}
      pick(event) {
        number(n) { add = n }
        label(s) { add = 20i32 }
        none { add = 1i32 }
      }
      sum = sum + add
    }

    [return<i32>]
    describe([Event] event) {
      return(weight(event))
    }
  }
}

[return<int>]
main() {
  [/ui/Handlers<i32> mut] handlers{/ui/Handlers<i32>{}}
  [i32 mut] sum{0i32}
  [/ui/Event] number{[number] 4i32}
  [/ui/Event] label{[label] "x"}
  [/ui/Event] nothing{}
  handlers.handle(number, location(sum))
  handlers.handle(label, location(sum))
  handlers.handle(nothing, location(sum))
  return(sum * 2i32 + handlers.describe(number) + handlers.describe(label) + /ui/weight(nothing))
}
)",
                   62);
}

TEST_CASE("template arguments are inferred through a generic struct parameter of a function and a method") {
  runMethodProgram("struct_methods_infer_through_generic_struct", R"(
namespace lib {
  [public struct]
  Pair<A, B>() {
    [public A] first
    [public B] second
  }
}

[struct]
Holder<S>() {
  [S mut] state{}
}

[struct]
Meter() {
  [return<int>]
  measure<S>([Holder<S>] holder) {
    return(holder.state + 100i32)
  }
}

[return<S>]
state_of<S>([Holder<S>] holder) {
  return(holder.state)
}

[return<A>]
first_of<A, B>([/lib/Pair<A, B>] pair) {
  return(pair.first)
}

[return<B>]
second_of<A, B>([/lib/Pair<A, B>] pair) {
  return(pair.second)
}

[return<int>]
main() {
  [Holder<i32> mut] holder{Holder<i32>{}}
  holder.state = 5i32
  [/lib/Pair<i32, i32>] pair{/lib/Pair<i32, i32>{[first] 7i32, [second] 9i32}}
  [Meter] meter{Meter{}}
  [i32] second{second_of(pair) + state_of(holder)}
  return(meter.measure(holder) + first_of(pair) + second)
}
)",
                   126);
}

TEST_CASE("a method parameter naming a generic struct of the root resolves it") {
  runMethodProgram("struct_methods_generic_struct_parameter", R"(
[struct]
Holder<S>() {
  [S mut] state{}
}

[return<int>]
read_state([Holder<i32>] holder) {
  return(holder.state + 100i32)
}

[struct]
Meter() {
  [return<int>]
  measure([Holder<i32>] holder) {
    return(holder.state + read_state(holder))
  }
}

[return<int>]
main() {
  [Holder<i32> mut] holder{Holder<i32>{}}
  holder.state = 5i32
  [Meter] meter{Meter{}}
  return(read_state(holder) + meter.measure(holder))
}
)",
                   215);
}

TEST_CASE("a wildcard import of the tuple module is valid when no tuple is built") {
  runMethodProgram("struct_methods_unused_tuple_import",
                   R"(
import /std/tuple/*
import /std/maybe/*

[return<int>]
main() {
  return(21i32)
}
)",
                   21);
}

TEST_CASE("an omitted field initializer of a generic struct in a namespace is its type's default") {
  runMethodProgram("struct_methods_namespace_default_field",
                   R"(
import /std/text/*

[struct]
Cell() {
  [i32] n{5i32}
}

namespace ns {
  [public struct]
  Holder<S>() {
    [S mut] state{}
  }
}

[return<int> effects(heap_alloc)]
main() {
  [/ns/Holder<i32>] a{/ns/Holder<i32>{}}
  [/ns/Holder<Cell>] b{/ns/Holder<Cell>{}}
  [/ns/Holder<String>] c{/ns/Holder<String>{}}
  return(a.state + b.state.n + c.state.count() + 40i32)
}
)",
                   45);
}

// A method that pushes onto a vector field and then returns its count, called inside an arithmetic expression: the
// operands before the call keep their values and the operands after it see the push, left to right on every backend
// (native was reported to return a wrong sum for `direct * 10i32 + m.add(6i32)`).
TEST_CASE(
    "a mutating vector-field method inside an arithmetic expression keeps its other operands") {
  runMethodProgram("struct_methods_mutating_field_call_in_expression",
                   R"(
import /std/collections/*

[struct]
KeepV {
  [vector<i32> mut] seen

  [mut effects(heap_alloc) return<i32>]
  add([i32] x) {
    this.seen.push(x)
    return(this.seen.count())
  }
}

[return<int> effects(heap_alloc)]
main() {
  [KeepV mut] m{KeepV{[seen] vector<i32>(5i32)}}
  m.seen.push(1i32)
  [i32] direct{m.seen.count()}
  [i32] before{direct * 10i32 + m.add(6i32)}
  if(before != 23i32) { return(1i32) }
  [i32] after{m.add(7i32) + direct * 10i32}
  if(after != 24i32) { return(2i32) }
  [i32] viaCount{m.seen.count() * 10i32 + m.add(8i32)}
  if(viaCount != 45i32) { return(3i32) }
  [i32] countAfter{m.add(9i32) * 10i32 + m.seen.count()}
  if(countAfter != 66i32) { return(4i32) }
  return(0i32)
}
)",
                   0);
}

TEST_CASE("two mutating vector-field methods and a nested call in one expression evaluate left to "
          "right") {
  runMethodProgram("struct_methods_two_mutating_field_calls_in_expression",
                   R"(
import /std/collections/*

[struct]
KeepV {
  [vector<i32> mut] seen

  [mut effects(heap_alloc) return<i32>]
  add([i32] x) {
    this.seen.push(x)
    return(this.seen.count())
  }
}

[return<int> effects(heap_alloc)]
main() {
  [KeepV mut] m{KeepV{[seen] vector<i32>(5i32)}}
  m.seen.push(1i32)
  [i32] direct{m.seen.count()}
  [i32] twice{m.add(1i32) * 10i32 + m.add(2i32)}
  if(twice != 34i32) { return(1i32) }
  [i32] nested{direct + m.add(1i32) * (m.seen.count() + m.add(2i32))}
  if(nested != 57i32) { return(2i32) }
  [i32] grouped{(direct + m.add(1i32)) * 10i32 + m.add(2i32) * 100i32}
  if(grouped != 890i32) { return(3i32) }
  return(0i32)
}
)",
                   0);
}

TEST_CASE("a struct with its own count, capacity and push still calls them on a vector field") {
  // `this.nodes.count()` is the vector's count whatever the enclosing struct defines; `bag.count()` is the struct's.
  runMethodProgram("struct_methods_own_count_with_vector_field",
                   R"(
import /std/text/*
import /std/collections/vector/*

[struct]
Bag() {
  [vector<i32> mut] nodes{vector<i32>{}}
  [String] label{String{}}

  [return<i32>]
  count() {
    return(70i32)
  }

  [return<i32>]
  capacity() {
    return(71i32)
  }

  [mut effects(heap_alloc)]
  push([i32] value) {
    this.nodes.push(value)
    this.nodes.push(value)
  }

  [return<i32>]
  total() {
    [i32] local{this.nodes.count()}
    if(this.nodes.count() > 1i32) {
      return(local + this.nodes.count() + this.nodes[0i32])
    }
    return(local)
  }

  [return<i32>]
  labelSize() {
    return(this.label.count())
  }

  [mut effects(heap_alloc)]
  grow() {
    this.nodes.push(5i32)
    this.nodes.clear()
    this.nodes.push(6i32)
  }

  [return<i32>]
  both() {
    return(this.count() + this.nodes.count())
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Bag mut] bag{Bag{}}
  bag.push(3i32)
  [i32 mut] sum{0i32}
  sum = sum + bag.count()
  sum = sum + bag.capacity()
  sum = sum + bag.total()
  sum = sum + bag.nodes.count()
  sum = sum + bag.labelSize()
  bag.grow()
  sum = sum + bag.nodes.count()
  sum = sum + bag.both()
  return(sum - 150i32)
}
)",
                   // 70 + 71 + 7 + 2 + 0 + 1 + 71 = 222, less 150
                   72);
}

TEST_CASE("a static helper of an imported struct is called as Type.helper()") {
  runMethodProgram("struct_methods_imported_static_helper",
                   R"(
namespace shapes {
[public struct]
Counter() {
  [public i32] value{5i32}

  [public static return<i32>]
  default_step() {
    return(2i32)
  }
}
}

import /shapes/*

[return<i32>]
main() {
  [Counter] counter{Counter{}}
  return(plus(counter.value, Counter.default_step()))
}
)",
                   7);
}

TEST_CASE("a generic struct parameter infers its template arguments through a Reference argument") {
  runMethodProgram("struct_methods_infer_through_reference",
                   R"(
[struct]
Box<S>() {
  [public S mut] state
}

[return<i32>]
mode<S>([Box<S>] h) {
  return(h.state + 1i32)
}

[return<i32>]
mode([i32] n) {
  return(n + 100i32)
}

[return<i32>]
via_param([Reference<Box<i32>>] r) {
  return(mode(r))
}

[return<int>]
main() {
  [Box<i32> mut] b{Box<i32>{3i32}}
  [Reference<Box<i32>>] r{location(b)}
  f{[ref b]([i32] k) { return(k + mode(b)) }}
  return(mode(r) + via_param(location(b)) + f(10i32) + mode(7i32))
}
)",
                   129);
}

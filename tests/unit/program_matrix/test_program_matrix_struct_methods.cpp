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

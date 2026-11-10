#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.parameter_modes");

// Parameter modes (docs/spec/value-lifecycle.md, Parameter Passing) run through every
// execution config: a `copy` parameter is the callee's own value, so its writes never
// reach the caller, and it is filled through the type's `Copy` helper when it has one.

TEST_CASE("copy mut scalar parameter does not write back") {
  program_matrix::ProgramCase program;
  program.name = "copy_mut_scalar_parameter";
  program.source = R"(
[i32]
bump([i32 copy mut] value) {
  assign(value, plus(value, 10i32))
  return(value)
}

[effects(io_out) return<int>]
main() {
  [i32 mut] start{1i32}
  print_line(bump(start))
  print_line(start)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "11\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("copy mut struct parameter does not write back") {
  program_matrix::ProgramCase program;
  program.name = "copy_mut_struct_parameter";
  program.source = R"(
[struct]
Tally() {
  [i32 mut] count{0i32}
}

[struct]
Mixed() {
  [i32 mut] count{0i32}
  [i64 mut] total{0i64}
}

[i32]
grow_tally([Tally copy mut] value) {
  assign(value.count, plus(value.count, 5i32))
  return(value.count)
}

[i32]
grow_mixed([Mixed copy mut] value) {
  assign(value.count, plus(value.count, 7i32))
  return(value.count)
}

[effects(io_out) return<int>]
main() {
  [Tally mut] tally{Tally{}}
  print_line(grow_tally(tally))
  print_line(tally.count)
  [Mixed mut] mixed{Mixed{}}
  print_line(grow_mixed(mixed))
  print_line(mixed.count)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "5\n0\n7\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("copy parameter runs the Copy helper unless the argument is moved") {
  program_matrix::ProgramCase program;
  program.name = "copy_parameter_copy_helper";
  program.source = R"(
[struct]
Tracked() {
  [i32 mut] id{0i32}
  [i32 mut] copies{0i32}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.id, other.id)
    assign(this.copies, plus(other.copies, 1i32))
  }
}

[i32]
copies_seen([Tracked copy] value) {
  return(value.copies)
}

[effects(io_out) return<int>]
main() {
  [Tracked] kept{Tracked{}}
  print_line(copies_seen(kept))
  print_line(copies_seen(kept))
  print_line(kept.copies)
  [Tracked] given{Tracked{}}
  print_line(copies_seen(move(given)))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n0\n0\n";
  program_matrix::runProgramMatrix(program);
}

// Real calls (docs/spec/vm-design.md, Real calls): on the VM and optexe a definition with more than a tiny body is
// lowered once and called. The receiver and every struct, `mut` and Reference argument pass as an address of the
// caller's place, so writes through them reach the caller, and a String result moves back to the call site.
TEST_CASE("real calls write the caller's places through receiver, mut and Reference parameters") {
  program_matrix::ProgramCase program;
  program.name = "real_call_places";
  program.source = R"(
import /std/text/*

[struct]
Log() {
  [String mut] text{String{}}
  [i32 mut] lines{0i32}

  [mut effects(heap_alloc)]
  add([i32] value) {
    this.text.append("[")
    this.text.appendI64(convert<i64>(value))
    this.text.append("]")
    this.lines = this.lines + 1i32
  }

  [return<i32>]
  total() {
    [i32 mut] sum{0i32}
    for([i32 mut] i{0i32}; i < this.text.count(); ++i) {
      sum = sum + 1i32
    }
    return(sum + this.lines * 100i32)
  }
}

[struct]
Pair() {
  [i32 mut] left{0i32}
  [i32 mut] right{0i32}
}

[effects(heap_alloc)]
record_twice([Log mut] log, [i32] value) {
  log.add(value)
  log.add(value + 1i32)
  log.lines = log.lines + 10i32
}

swap_pair([Reference<Pair>] pair) {
  [i32] left{pair.left}
  pair.left = pair.right
  pair.right = left
}

bump([i32 mut] counter, [i32] by) {
  counter = counter + by
  counter = counter * 2i32
  counter = counter - 1i32
}

[return<String> effects(heap_alloc)]
label([i32] id, [Pair] pair) {
  [String mut] out{String{}}
  out.append("#")
  out.appendI64(convert<i64>(id))
  out.append(":")
  out.appendI64(convert<i64>(pair.left + pair.right))
  return(out)
}

[effects(io_out, heap_alloc) return<int>]
main() {
  [Log mut] log{Log{}}
  record_twice(log, 1i32)
  record_twice(log, 5i32)
  log.add(9i32)
  print_line(log.lines)
  print_line(log.total())
  [Pair mut] pair{Pair{[left] 3i32, [right] 4i32}}
  swap_pair(location(pair))
  print_line(pair.left)
  print_line(pair.right)
  [i32 mut] counter{1i32}
  bump(counter, 2i32)
  bump(counter, 3i32)
  print_line(counter)
  [String] first{label(7i32, pair)}
  [String] second{label(8i32, Pair{[left] 1i32, [right] 1i32})}
  print_line(first.count() + second.count())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "25\n2515\n4\n3\n15\n8\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a runtime error inside a real call stops the program") {
  program_matrix::ProgramCase program;
  program.name = "real_call_runtime_error";
  program.source = R"(
[return<i32>]
pick([i32] index, [i32] offset) {
  [array<i32>] values{array<i32>(10i32, 20i32, 30i32)}
  [i32] at{index + offset}
  [i32] doubled{at * 2i32}
  return(values[doubled - at])
}

[effects(io_out) return<int>]
main() {
  print_line(pick(1i32, 0i32))
  print_line(pick(1i32, 1i32))
  print_line(pick(5i32, 0i32))
  print_line(99i32)
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "20\n30\n";
  program.vmStderr = "array index out of bounds\n";
  program.nativeStderr = "array index out of bounds\n";
  program_matrix::runProgramMatrix(program);
}

// A method that takes a `Function` by `move` is a real call on the VM and optexe (TODO-5748): the call site binds the
// closure in its own frame and the body owns it, destroying it unless it moves it on (here into a vector, or on to
// another `move` parameter). The closure's captures are destroyed once, as in the inlined call.
TEST_CASE("a Function moved into a method is owned by the call") {
  program_matrix::ProgramCase program;
  program.name = "real_call_function_move";
  program.source = R"(
import /std/collections/*

[struct]
Counter() {
  [i32 mut] total{0i32}
}

[struct]
Noisy() {
  [i32] id{0i32}

  [effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[struct]
Slot<State>() {
  [Function<void, Reference<State>, i32> mut] handler
}

[struct]
Table<State>() {
  [State mut] state
  [vector<Slot<State>> mut] slots{vector<Slot<State>>()}

  [mut effects(heap_alloc) return<i32>]
  add([Function<void, Reference<State>, i32> move] handler) {
    [i32] index{this.slots.count()}
    this.slots.push(Slot<State>{[handler] handler})
    return(index)
  }

  [mut effects(heap_alloc) return<i32>]
  addTwice([Function<void, Reference<State>, i32> move] handler) {
    [i32] first{this.add(handler)}
    return(first)
  }

  [mut effects(heap_alloc)]
  fire([i32] index, [i32] amount) {
    [Reference<State>] state{location(this.state)}
    this.slots[index].handler(state, amount)
  }
}

[effects(heap_alloc, io_out) return<int>]
run() {
  [Table<Counter> mut] table{Table<Counter>{[state] Counter{}}}
  [Noisy] noisy{Noisy{[id] 7i32}}
  [i32] a{table.add([noisy]([Reference<Counter>] c, [i32] n) { c.total = c.total + n * noisy.id })}
  [i32] b{table.addTwice([]([Reference<Counter>] c, [i32] n) { c.total = c.total - n })}
  [Function<void, Reference<Counter>, i32>] named{[]([Reference<Counter>] c, [i32] n) { c.total = c.total + 100i32 * n }}
  [i32] c{table.add(move(named))}
  table.fire(a, 2i32)
  table.fire(b, 1i32)
  table.fire(c, 1i32)
  print_line(table.state.total)
  return(table.state.total + a + b + c)
}

[effects(heap_alloc, io_out) return<int>]
main() {
  [i32] result{run()}
  print_line(99i32)
  return(result)
}
)";
  program.exitCode = 116;
  program.stdoutText = "7\n113\n7\n99\n";
  program_matrix::runProgramMatrix(program);
}

// A builtin `[vector<T> copy]` parameter owns a buffer of its own, which the callee frees: a named or field argument
// is copied, a fresh vector or `move(x)` is handed over (docs/spec/value-lifecycle.md, Parameter Passing). Before
// TODO-5829 the parameter aliased the caller's record, so the callee's pushes reached the caller.
TEST_CASE("a builtin vector copy parameter gets its own buffer") {
  program_matrix::ProgramCase program;
  program.name = "builtin_vector_copy_parameter_owns_buffer";
  program.source = R"(
import /std/collections/*

[struct]
KeepV {
  [vector<i32> mut] seen
}

[effects(heap_alloc) return<int>]
grow([vector<i32> copy] v) {
  v.push(9i32)
  v.push(10i32)
  v.push(11i32)
  return(v.count())
}

[effects(heap_alloc) return<int>]
total([vector<i32>] v) {
  return(v.count())
}

[effects(heap_alloc) return<KeepV>]
keep([vector<i32> copy] v) {
  v.push(4i32)
  return(KeepV{[seen] v})
}

[effects(heap_alloc) return<vector<i32>>]
pass_on([vector<i32> copy] v) {
  v.push(5i32)
  return(v)
}

[effects(heap_alloc) return<int>]
round() {
  [vector<i32> mut] a{vector<i32>(1i32, 2i32)}
  [KeepV mut] k{KeepV{[seen] vector<i32>(7i32)}}
  [i32] g1{grow(a)}
  [i32] g2{grow(k.seen)}
  [i32] g3{grow(vector<i32>(1i32))}
  [i32] t1{total(vector<i32>(1i32, 2i32, 3i32))}
  [KeepV] kept{keep(a)}
  [vector<i32> mut] passed{pass_on(a)}
  passed.push(6i32)
  [i32] after{a.count() * 10000000i32}
  [i32] g4{grow(move(a))}
  return(g1 + g2 * 10i32 + g3 * 100i32 + t1 * 1000i32 + kept.seen.count() * 10000i32 + passed.count() * 100000i32 +
         k.seen.count() * 1000000i32 + after + g4 * 0i32)
}

[return<int> effects(heap_alloc)]
main() {
  [i32 mut] last{0i32}
  repeat(100i32) {
    last = round()
  }
  return(last - 21433400i32)
}
)";
  program.exitCode = 45;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    program.onlyConfigs.push_back("native-O0");
    program.onlyConfigs.push_back("native-O2");
  }
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("builtin vector copy parameters free what they own") {
  program_matrix::ProgramCase program;
  program.name = "builtin_vector_copy_parameter_heap_flat";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code
  [String] name
}

[effects(heap_alloc) return<int>]
grow([vector<i32> copy] v) {
  v.push(9i32)
  return(v.count())
}

[effects(heap_alloc) return<int>]
rename([vector<Named> copy] rows) {
  rows.push(Named{[code] 9i32, [name] stringFrom("pushed into the copy, long enough for the heap")})
  return(rows.count())
}

[effects(heap_alloc) return<vector<i32>>]
pass_on([vector<i32> copy] v) {
  v.push(5i32)
  return(v)
}

[effects(heap_alloc) return<int>]
round() {
  [vector<i32> mut] a{vector<i32>(1i32, 2i32)}
  [vector<Named> mut] rows{vector<Named>()}
  rows.push(Named{[code] 1i32, [name] stringFrom("an original row long enough for the heap")})
  [i32] g1{grow(a)}
  [i32] g2{grow(vector<i32>(1i32))}
  [i32] r1{rename(rows)}
  [vector<i32> mut] passed{pass_on(a)}
  [i32] g3{grow(move(a))}
  return(g1 + g2 * 10i32 + r1 * 100i32 + rows.count() * 1000i32 + passed.count() * 10000i32 + g3 * 0i32)
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
  return(first - 31200i32 + leakedBlocks * 1000i32 + leakedText * 1000i32)
}
)";
  program.exitCode = 23;
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();

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

TEST_SUITE_END();

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

TEST_SUITE_END();
